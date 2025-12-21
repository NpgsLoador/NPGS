#include "stdafx.h"
#include "ShaderManager.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <utility>
#include <vulkan/vulkan_to_string.hpp>

#include "Engine/Core/Utils/Hash.hpp"
#include "Engine/Core/Logger.hpp"

namespace Npgs
{
    struct FSpecializationConstantBufferHash
    {
        std::size_t operator()(const FSpecializationConstantBuffer& Buffer) const noexcept
        {
            struct FSpecializationMapEntryHash
            {
                std::size_t operator()(const vk::SpecializationMapEntry& Entry) const noexcept
                {
                    std::size_t Hash = 0;
                    Utils::HashCombine(Entry.constantID, Hash);
                    Utils::HashCombine(Entry.offset, Hash);
                    Utils::HashCombine(Entry.size, Hash);
                    return Hash;
                }
            };

            std::size_t Hash = 0;
            Utils::HashCombineRange<vk::SpecializationMapEntry, FSpecializationMapEntryHash>(Buffer.Entries, Hash);
            Utils::HashCombineRange<std::byte>(Buffer.DataBuffer, Hash);
            return Hash;
        }
    };

    struct FShaderInfoHash
    {
        std::size_t operator()(const FShaderAcquireInfo::FShaderInfo& Info) const noexcept
        {
            std::size_t Hash = 0;
            Utils::HashCombine(Info.Name,      Hash);
            Utils::HashCombine(Info.NextStage, Hash);
            return Hash;
        }
    };

    std::size_t FShaderAcquireInfoHash::operator()(const FShaderAcquireInfo& Info) const noexcept
    {
        std::size_t Hash = 0;

        for (const auto& [Stage, ShaderInfo] : Info.ShaderInfos)
        {
            Utils::HashCombine(Stage, Hash);
            Utils::HashCombine<FShaderAcquireInfo::FShaderInfo, FShaderInfoHash>(ShaderInfo, Hash);
        }

        Utils::HashCombine<FSpecializationConstantBuffer, FSpecializationConstantBufferHash>(
            Info.SpecializationConstantBuffer_, Hash);

        return Hash;
    }

    void FShaderResource::ApplyHandles()
    {
        static constexpr std::array kAllGraphicsStages
        {
            vk::ShaderStageFlagBits::eVertex,
            vk::ShaderStageFlagBits::eTessellationControl,
            vk::ShaderStageFlagBits::eTessellationEvaluation,
            vk::ShaderStageFlagBits::eGeometry,
            vk::ShaderStageFlagBits::eFragment,
            vk::ShaderStageFlagBits::eTaskEXT,
            vk::ShaderStageFlagBits::eMeshEXT
        };

        if (StoragedHandles_.size() == 1 && Stages.front() == vk::ShaderStageFlagBits::eCompute)
        {
            Stages  = { vk::ShaderStageFlagBits::eCompute };
            Handles = { *StoragedHandles_.front() };
            return;
        }

        auto TempStages = Stages;
        Stages.assign_range(kAllGraphicsStages);
        Handles.assign(kAllGraphicsStages.size(), vk::ShaderEXT(nullptr));

        for (std::size_t i = 0; i != StoragedHandles_.size(); ++i)
        {
            const auto& Handle = StoragedHandles_[i];
            const auto  Stage  = TempStages[i];
            auto it = std::ranges::find(Stages, Stage);
            if (it != Stages.end())
            {
                auto Index = std::distance(Stages.begin(), it);
                Handles[Index] = *Handle;
            }
        }
    }

    FShaderManager::FShaderManager(FVulkanContext* VulkanContext, FAssetManager* AssetManager)
        : VulkanContext_(VulkanContext)
        , AssetManager_(AssetManager)
    {
    }

    FShaderResource* FShaderManager::AcquireShaderResource(const FShaderAcquireInfo& AcquireInfo)
    {
        {
            std::shared_lock Lock(SharedCacheMutex_);
            auto it = ShaderResourceCache_.find(AcquireInfo);
            if (it != ShaderResourceCache_.end())
            {
                return it->second.get();
            }
        }

        std::vector<std::pair<FShaderHandle, vk::ShaderStageFlagBits>> ShadersToLink;
        for (const auto& ShaderInfo : AcquireInfo.ShaderInfos)
        {
            if (ShaderInfo.second.Name.empty())
            {
                continue;
            }

            const auto& Shader = AssetManager_->AcquireAsset<FShader>(ShaderInfo.second.Name);
            ShadersToLink.emplace_back(Shader, ShaderInfo.second.NextStage);
        }

        if (ShadersToLink.empty())
        {
            NpgsCoreError("Acquire info must has one shader name.");
            return nullptr;
        }

        auto MergedSetLayoutBindings = MergeSetLayoutBindings(ShadersToLink);
        auto SetLayouts              = SetupDescriptorSetLayouts(MergedSetLayoutBindings);
        auto PushConstantRanges      = MergePushConstantRanges(ShadersToLink);
        auto PushConstantOffsetsMap  = GeneratePushConstantOffsetsMap(ShadersToLink);
        auto SetInfos                = GenerateDescriptorSetInfos(MergedSetLayoutBindings, SetLayouts);
        
        FShaderResource ShaderResource;
        ShaderResource.PushConstantOffsetsMap_ = std::move(PushConstantOffsetsMap);
        ShaderResource.SetInfos                = std::move(SetInfos);

        std::vector<vk::DescriptorSetLayout> NativeTypeSetLayouts;
        for (const auto& SetLayout : SetLayouts)
        {
            NativeTypeSetLayouts.push_back(*SetLayout);
        }

        vk::PipelineLayoutCreateInfo PipelineLayoutCreateInfo({}, NativeTypeSetLayouts, PushConstantRanges);
        ShaderResource.PipelineLayout = FVulkanPipelineLayout(VulkanContext_->GetDevice(), "PipelineLayout", PipelineLayoutCreateInfo);

        auto VertexShaderIt = AcquireInfo.ShaderInfos.find(vk::ShaderStageFlagBits::eVertex);
        if (VertexShaderIt != AcquireInfo.ShaderInfos.end())
        {
            const auto& VertexShader = AssetManager_->AcquireAsset<FShader>(VertexShaderIt->second.Name);
            ShaderResource.VertexInputBindings   = VertexShader->GetVertexInputBindings();
            ShaderResource.VertexInputAttributes = VertexShader->GetVertexInputAttributes();
        }

        for (const auto& Commit : AcquireInfo.ConstantCommitList_)
        {
            Commit(AssetManager_);
        }

        for (const auto& Shader : ShadersToLink)
        {
            const auto& [ShaderHandle, NextStage] = Shader;

            vk::SpecializationInfo SpecializationInfo;
            if (!AcquireInfo.SpecializationConstantBuffer_.Entries.empty())
            {
                vk::ArrayProxyNoTemporaries<const std::byte> DataBuffer = AcquireInfo.SpecializationConstantBuffer_.DataBuffer;
                SpecializationInfo = vk::SpecializationInfo(AcquireInfo.SpecializationConstantBuffer_.Entries, DataBuffer);
            }

            vk::ArrayProxyNoTemporaries<const std::uint32_t> ShaderCode = ShaderHandle->GetShaderCode();
            ShaderResource.Stages.push_back(ShaderHandle->GetShaderStage());

            // TODO: binary cache loading
            vk::ShaderCreateInfoEXT CreateInfo(
                {},
                ShaderHandle->GetShaderStage(),
                NextStage,
                vk::ShaderCodeTypeEXT::eSpirv,
                ShaderCode,
                "main",
                NativeTypeSetLayouts,
                PushConstantRanges,
                &SpecializationInfo
            );

            ShaderResource.StoragedHandles_.emplace_back(
                VulkanContext_->GetDevice(), std::format("{}_ShaderObject", ShaderHandle->GetFilename()), CreateInfo);
        }

        ShaderResource.ApplyHandles();

        {
            std::unique_lock Lock(CacheMutex_);
            if (!ShaderResourceCache_.contains(AcquireInfo))
            {
                ShaderResourceCache_.emplace(AcquireInfo, std::make_unique<FShaderResource>(std::move(ShaderResource)));
            }
        }

        return ShaderResourceCache_.at(AcquireInfo).get();
    }

    FShaderManager::FSetLayoutBindingMap FShaderManager::MergeSetLayoutBindings(const FShaderStagePairArray& Shaders) const
    {
        std::map<std::uint32_t, std::map<std::uint32_t, vk::DescriptorSetLayoutBinding>> MergedData;
        for (const auto& [Shader, _] : Shaders)
        {
            if (!Shader)
            {
                continue;
            }

            const auto& DescriptorSetBindings = Shader->GetSetLayoutBindings();
            for (const auto& [SetIndex, Bindings] : DescriptorSetBindings)
            {
                for (const auto& Binding : Bindings)
                {
                    auto& SetBindingMap = MergedData[SetIndex];

                    auto it = SetBindingMap.find(Binding.binding);
                    if (it == SetBindingMap.end())
                    {
                        SetBindingMap[Binding.binding] = Binding;
                    }
                    else
                    {
                        auto& ExistingBinding = it->second;

                        if (ExistingBinding.descriptorType != Binding.descriptorType)
                        {
                            throw std::runtime_error(std::format(
                                "Shader merge conflict at Set {} Binding {}: Type mismatch({} vs {}).",
                                SetIndex,
                                Binding.binding,
                                vk::to_string(ExistingBinding.descriptorType),
                                vk::to_string(Binding.descriptorType)
                            ));
                        }

                        if (ExistingBinding.descriptorCount != Binding.descriptorCount)
                        {
                            throw std::runtime_error(std::format(
                                "Shader merge conflict at Set {} Binding {}: Count mismatch({} vs {}).",
                                SetIndex,
                                Binding.binding,
                                ExistingBinding.descriptorCount,
                                Binding.descriptorCount
                            ));
                        }

                        ExistingBinding.stageFlags |= Binding.stageFlags;
                    }
                }
            }
        }

        FSetLayoutBindingMap Result;
        for (const auto& [SetIndex, BindingMap] : MergedData)
        {
            auto& BindingVector = Result[SetIndex];
            BindingVector.reserve(BindingMap.size());

            for (const auto& [_, Binding] : BindingMap)
            {
                BindingVector.push_back(Binding);
            }
        }

        return Result;
    }

    FShaderManager::FSetLayoutArray FShaderManager::SetupDescriptorSetLayouts(const FSetLayoutBindingMap& MergedSetLayoutBindings) const
    {
        if (MergedSetLayoutBindings.empty())
        {
            return {};
        }

        std::uint32_t MaxSetIndex = 0;
        for (const auto& [SetIndex, _] : MergedSetLayoutBindings)
        {
            MaxSetIndex = std::max(MaxSetIndex, SetIndex);
        }

        std::vector<FVulkanDescriptorSetLayout> SetLayouts(MaxSetIndex + 1);
        for (std::uint32_t i = 0; i <= MaxSetIndex; ++i)
        {
            if (MergedSetLayoutBindings.contains(i))
            {
                vk::DescriptorSetLayoutCreateInfo LayoutCreateInfo(
                    vk::DescriptorSetLayoutCreateFlagBits::eDescriptorBufferEXT, MergedSetLayoutBindings.at(i));
                SetLayouts[i] = FVulkanDescriptorSetLayout(
                    VulkanContext_->GetDevice(), std::format("DescriptorSetLayout_Set{}", i), LayoutCreateInfo);
            }
            else
            {
                vk::DescriptorSetLayoutCreateInfo EmptyLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlagBits::eDescriptorBufferEXT);
                SetLayouts[i] = FVulkanDescriptorSetLayout(
                    VulkanContext_->GetDevice(), "EmptyDescriptorSetLayout", EmptyLayoutCreateInfo);
            }
        }

        return SetLayouts;
    }

    std::vector<vk::PushConstantRange> FShaderManager::MergePushConstantRanges(const FShaderStagePairArray& Shaders) const
    {
        std::vector<vk::PushConstantRange> RawRanges;
        for (const auto& [Shader, _] : Shaders)
        {
            if (!Shader)
            {
                continue;
            }

            const auto& ShaderRanges = Shader->GetPushConstantRanges();
            RawRanges.append_range(ShaderRanges | std::views::as_rvalue);
        }

        if (RawRanges.empty())
        {
            return {};
        }

        std::ranges::sort(RawRanges, [](const auto& Lhs, const auto& Rhs) -> bool
        {
            return Lhs.offset < Rhs.offset;
        });

        std::vector<vk::PushConstantRange> MergedRanges;
        MergedRanges.push_back(RawRanges.front());

        auto RangesOverlapOrAdjacent = [](const auto& Lhs, const auto& Rhs) -> bool
        {
            std::uint32_t LhsEnd = Lhs.offset + Lhs.size;
            std::uint32_t RhsEnd = Rhs.offset + Rhs.size;
            // 如果不需要合并，考虑更改 <= 为 <
            return (Lhs.offset <= RhsEnd) && (Rhs.offset <= LhsEnd);
        };

        for (std::size_t i = 1; i != RawRanges.size(); ++i)
        {
            auto& Current = MergedRanges.back();
            const auto& Next = RawRanges[i];
            if (RangesOverlapOrAdjacent(Current, Next))
            {
                std::uint32_t NewStart   = std::min(Current.offset, Next.offset);
                std::uint32_t CurrentEnd = Current.offset + Current.size;
                std::uint32_t NextEnd    = Next.offset + Next.size;
                std::uint32_t NewEnd     = std::max(CurrentEnd, NextEnd);

                Current.offset      = NewStart;
                Current.size        = NewEnd - NewStart;
                Current.stageFlags |= Next.stageFlags;
            }
            else
            {
                MergedRanges.push_back(Next);
            }
        }

        return MergedRanges;
    }

    FShaderManager::FPushConstantOffsetsMap FShaderManager::GeneratePushConstantOffsetsMap(const FShaderStagePairArray& Shaders)
    {
        FPushConstantOffsetsMap OffsetsMap;
        for (const auto& Shader : Shaders)
        {
            for (const auto& [Name, Offset] : Shader.first->GetPushConstantOffsetsMap())
            {
                if (OffsetsMap.contains(Name))
                {
                    std::uint32_t ExistingOffset = OffsetsMap.at(Name);
                    if (ExistingOffset != Offset)
                    {
                        throw std::runtime_error(std::format(
                            "Push constant '{}' offset conflict: {} vs {}. The use of conflicting \
                             push constant names is not allowed within each pipeline.",
                            Name, ExistingOffset, Offset
                        ));
                    }
                }
                else
                {
                    OffsetsMap.emplace(Name, Offset);
                }
            }
        }

        return OffsetsMap;
    }

    FShaderManager::FDescriptorSetInfoMap
    FShaderManager::GenerateDescriptorSetInfos(const FSetLayoutBindingMap& SetLayoutBindings,
                                               const FShaderManager::FSetLayoutArray& SetLayouts)
    {
        if (SetLayouts.empty())
        {
            return {};
        }

        vk::Device Device = VulkanContext_->GetDevice();
        FDescriptorSetInfoMap SetInfos;

        for (const auto& [SetIndex, LayoutBindings] : SetLayoutBindings)
        {
            const auto& Layout = SetLayouts[SetIndex];
            vk::DeviceSize LayoutSize = Device.getDescriptorSetLayoutSizeEXT(*Layout);
            FDescriptorSetInfo SetInfo{ .Set = SetIndex, .Size = LayoutSize };

            for (const auto& Binding : LayoutBindings)
            {
                FDescriptorBindingInfo BindingInfo
                {
                    .Binding = Binding.binding,
                    .Type    = Binding.descriptorType,
                    .Count   = Binding.descriptorCount,
                    .Stage   = Binding.stageFlags,
                    .Offset  = Device.getDescriptorSetLayoutBindingOffsetEXT(*Layout, Binding.binding)
                };

                SetInfo.Bindings[Binding.binding] = std::move(BindingInfo);
            }

            SetInfos[SetIndex] = std::move(SetInfo);
        }

        return SetInfos;
    }
} // namespace Npgs
