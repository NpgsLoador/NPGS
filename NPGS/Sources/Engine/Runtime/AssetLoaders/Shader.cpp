#include "stdafx.h"
#include "Shader.hpp"

#include <cstddef>
#include <algorithm>
#include <exception>
#include <filesystem>
#include <numeric>
#include <set>
#include <stdexcept>
#include <utility>

#include <SPIRV-Reflect/spirv_reflect.h>

#include "Engine/Core/Logger.hpp"
#include "Engine/Runtime/Managers/AssetManager.hpp"

namespace Npgs
{
    namespace
    {
        vk::ShaderStageFlagBits GetShaderStageFromFilename(const std::string& Filename)
        {
            if (Filename.find("vert") != std::string::npos)
                return vk::ShaderStageFlagBits::eVertex;
            else if (Filename.find("frag") != std::string::npos)
                return vk::ShaderStageFlagBits::eFragment;
            else if (Filename.find("comp") != std::string::npos)
                return vk::ShaderStageFlagBits::eCompute;
            else if (Filename.find("geom") != std::string::npos)
                return vk::ShaderStageFlagBits::eGeometry;
            else if (Filename.find("tesc") != std::string::npos)
                return vk::ShaderStageFlagBits::eTessellationControl;
            else if (Filename.find("tese") != std::string::npos)
                return vk::ShaderStageFlagBits::eTessellationEvaluation;
            else
                return vk::ShaderStageFlagBits::eAll;
        }
    }

    FShader::FShader(FVulkanContext* VulkanContext, const std::vector<std::string>& ShaderFiles, const FResourceInfo& ResourceInfo)
        : VulkanContext_(VulkanContext)
    {
        vk::DescriptorSetLayoutCreateInfo EmptyLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlagBits::eDescriptorBufferEXT);
        EmptyDescriptorSetLayout_ = FVulkanDescriptorSetLayout(VulkanContext_->GetDevice(), "EmptyDescriptorSetLayout", EmptyLayoutCreateInfo);

        InitializeShaders(ShaderFiles, ResourceInfo);
        CreateDescriptorSetLayouts();
        GenerateDescriptorInfos();
    }

    FShader::FShader(FShader&& Other) noexcept
        : VulkanContext_(std::exchange(Other.VulkanContext_, nullptr))
        , ReflectionInfo_(std::exchange(Other.ReflectionInfo_, {}))
        , ShaderModules_(std::move(Other.ShaderModules_))
        , PushConstantOffsetsMap_(std::move(Other.PushConstantOffsetsMap_))
        , DescriptorSetInfos_(std::move(Other.DescriptorSetInfos_))
        , DescriptorSetLayoutsMap_(std::move(Other.DescriptorSetLayoutsMap_))
    {
    }

    FShader& FShader::operator=(FShader&& Other) noexcept
    {
        if (this != &Other)
        {
            VulkanContext_           = std::exchange(Other.VulkanContext_, nullptr);
            ReflectionInfo_          = std::exchange(Other.ReflectionInfo_, {});
            ShaderModules_           = std::move(Other.ShaderModules_);
            PushConstantOffsetsMap_  = std::move(Other.PushConstantOffsetsMap_);
            DescriptorSetInfos_      = std::move(Other.DescriptorSetInfos_);
            DescriptorSetLayoutsMap_ = std::move(Other.DescriptorSetLayoutsMap_);
        }

        return *this;
    }

    std::vector<vk::PipelineShaderStageCreateInfo> FShader::CreateShaderStageCreateInfo() const
    {
        std::vector<vk::PipelineShaderStageCreateInfo> ShaderStageCreateInfos;
        for (const auto& [Stage, ShaderModule] : ShaderModules_)
        {
            vk::PipelineShaderStageCreateInfo ShaderStageCreateInfo({}, Stage, *ShaderModule, "main");
            ShaderStageCreateInfos.push_back(std::move(ShaderStageCreateInfo));
        }

        return ShaderStageCreateInfos;
    }

    std::vector<vk::DescriptorSetLayout> FShader::GetDescriptorSetLayouts() const
    {
        if (DescriptorSetLayoutsMap_.empty())
        {
            return {};
        }

        const std::uint32_t MaxSet = DescriptorSetLayoutsMap_.rbegin()->first;
        std::vector<vk::DescriptorSetLayout> NativeTypeLayouts(MaxSet + 1);

        std::uint32_t LastFilledIndex = 0;

        for (const auto& [Set, Layout] : DescriptorSetLayoutsMap_)
        {
            for (std::uint32_t i = LastFilledIndex; i != Set; ++i)
            {
                NativeTypeLayouts[i] = *EmptyDescriptorSetLayout_;
            }

            NativeTypeLayouts[Set] = *Layout;
            LastFilledIndex        = Set + 1;
        }

        for (std::uint32_t i = LastFilledIndex; i <= MaxSet; ++i)
        {
            NativeTypeLayouts[i] = *EmptyDescriptorSetLayout_;
        }

        return NativeTypeLayouts;
    }

    void FShader::InitializeShaders(const std::vector<std::string>& ShaderFiles, const FResourceInfo& ResourceInfo)
    {
        for (const auto& Filename : ShaderFiles)
        {
            FShaderInfo ShaderInfo = LoadShader(GetAssetFullPath(EAssetType::kShader, Filename));
            if (ShaderInfo.Code.Empty())
            {
                return;
            }

            if (ShaderInfo.Code.Size() % 4 != 0)
            {
                throw std::runtime_error("Invalid SPIR-V shader code size: Not a multiple of 4 bytes.");
            }

            auto SpirvSpan = ShaderInfo.Code.GetDataAs<std::uint32_t>();
            vk::ShaderModuleCreateInfo ShaderModuleCreateInfo({}, SpirvSpan);
            FVulkanShaderModule ShaderModule(VulkanContext_->GetDevice(), Filename, ShaderModuleCreateInfo);
            ShaderModules_.emplace_back(ShaderInfo.Stage, std::move(ShaderModule));

            ReflectShader(std::move(ShaderInfo), ResourceInfo);
        }
    }

    FShader::FShaderInfo FShader::LoadShader(const std::string& Filename)
    {
        if (!std::filesystem::exists(Filename))
        {
            NpgsCoreError("Failed to load shader: \"{}\": No such file or directory.", Filename);
            return {};
        }

        FFileLoader ShaderCode;
        if (!ShaderCode.Load(Filename))
        {
            NpgsCoreError("Failed to open shader: \"{}\": Access denied.", Filename);
            return {};
        }

        vk::ShaderStageFlagBits Stage = GetShaderStageFromFilename(Filename);
        return { std::move(ShaderCode), Stage };
    }

    void FShader::ReflectShader(FShaderInfo&& ShaderInfo, const FResourceInfo& ResourceInfo)
    {
        auto ShaderCode = ShaderInfo.Code.StripData<std::uint32_t>();
        spv_reflect::ShaderModule ShaderModule(ShaderCode, SPV_REFLECT_MODULE_FLAG_NO_COPY);
        if (ShaderModule.GetResult() != SPV_REFLECT_RESULT_SUCCESS)
        {
            NpgsCoreError("Failed to reflect shader.");
            return;
        }

        std::uint32_t Count = 0;

        // Push Constants
        ShaderModule.EnumeratePushConstantBlocks(&Count, nullptr);
        if (Count > 0)
        {
            std::vector<SpvReflectBlockVariable*> PushConstants(Count);
            ShaderModule.EnumeratePushConstantBlocks(&Count, PushConstants.data());

            for (const auto* Block : PushConstants)
            {
                if (ResourceInfo.PushConstantInfos.count(ShaderInfo.Stage))
                {
                    const auto& ConfiguredNames = ResourceInfo.PushConstantInfos.at(ShaderInfo.Stage);
                    for (std::uint32_t i = 0; i != Block->member_count; ++i)
                    {
                        const auto&      Member     = Block->members[i];
                        std::string_view MemberName = ConfiguredNames[i];

                        if (!MemberName.empty())
                        {
                            PushConstantOffsetsMap_[std::string(MemberName)] = Member.offset;
                            NpgsCoreTrace("  Member \"{}\" at offset={}", MemberName, Member.offset);
                        }
                    }
                }

                NpgsCoreTrace("Push Constant \"{}\" size={} bytes, offset={}",
                              Block->name ? Block->name : "unnamed", Block->size, Block->offset);

                vk::PushConstantRange PushConstantRange(ShaderInfo.Stage, Block->offset, Block->size);
                AddPushConstantRange(PushConstantRange);
            }
        }

        // Descriptor Sets
        Count = 0;
        ShaderModule.EnumerateDescriptorSets(&Count, nullptr);
        if (Count > 0)
        {
            std::vector<SpvReflectDescriptorSet*> DescriptorSets(Count);
            ShaderModule.EnumerateDescriptorSets(&Count, DescriptorSets.data());

            for (const auto* Set : DescriptorSets)
            {
                for (std::uint32_t i = 0; i != Set->binding_count; ++i)
                {
                    const auto* Binding = Set->bindings[i];
                    vk::DescriptorType Type = static_cast<vk::DescriptorType>(Binding->descriptor_type);
                    std::uint32_t ArraySize = Binding->count;

                    NpgsCoreTrace("Descriptor \"{}\" at set={}, binding={}, type={}, array_size={}",
                                  Binding->name ? Binding->name : "unnamed", Set->set,
                                  Binding->binding, std::to_underlying(Type), ArraySize);

                    vk::DescriptorSetLayoutBinding LayoutBinding = vk::DescriptorSetLayoutBinding()
                        .setBinding(Binding->binding)
                        .setDescriptorType(Type)
                        .setDescriptorCount(ArraySize)
                        .setStageFlags(ShaderInfo.Stage);

                    AddDescriptorSetBindings(Set->set, LayoutBinding);
                }
            }
        }

        // Vertex Inputs
        if (ShaderInfo.Stage == vk::ShaderStageFlagBits::eVertex)
        {
            Count = 0;
            ShaderModule.EnumerateInputVariables(&Count, nullptr);
            if (Count > 0)
            {
                std::vector<SpvReflectInterfaceVariable*> InputVariables(Count);
                ShaderModule.EnumerateInputVariables(&Count, InputVariables.data());

                std::ranges::sort(InputVariables, [](const auto* Lhs, const auto* Rhs) -> bool
                {
                    return Lhs->location < Rhs->location;
                });

                std::unordered_map<std::uint32_t, FVertexBufferInfo> BufferMap;
                for (const auto& Buffer : ResourceInfo.VertexBufferInfos)
                {
                    BufferMap[Buffer.Binding] = Buffer;
                }

                // [Location, [Binding, Offset]]
                std::unordered_map<std::uint32_t, std::pair<std::uint32_t, std::uint32_t>> LocationMap;
                for (const auto& Vertex : ResourceInfo.VertexAttributeInfos)
                {
                    LocationMap[Vertex.Location] = std::make_pair(Vertex.Binding, Vertex.Offset);
                }

                std::uint32_t CurrentBinding = 0;
                std::uint32_t CurrentOffset  = 0;
                std::set<vk::VertexInputBindingDescription> UniqueBindings;

                for (const auto* Input : InputVariables)
                {
                    if (Input->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN)
                    {
                        continue;
                    }

                    const auto* TypeDescription = Input->type_description;

                    std::uint32_t Binding  = 0;
                    std::uint32_t Offset   = 0;
                    std::uint32_t Location = Input->location;
                    auto LocationIt = LocationMap.find(Location);
                    if (LocationIt != LocationMap.end())
                    {
                        Binding = LocationIt->second.first;
                        Offset  = LocationIt->second.second;
                    }
                    else
                    {
                        Binding = CurrentBinding;
                        Offset  = CurrentOffset;
                    }

                    std::uint32_t MatrixColumns = Input->numeric.matrix.column_count;
                    std::uint32_t MatrixRows    = Input->numeric.matrix.row_count;
                    std::uint32_t VectorSize    = Input->numeric.vector.component_count;
                    VectorSize = VectorSize == 0 ? 1 : VectorSize;

                    std::uint32_t Width        = Input->numeric.scalar.width;
                    std::uint32_t ScalarSize   = Width / 8;
                    std::uint32_t VariableSize = 0;
                    if (MatrixColumns > 1)
                    {
                        VariableSize = ScalarSize * MatrixRows * MatrixColumns;
                    }
                    else
                    {
                        VariableSize = ScalarSize * VectorSize;
                    }

                    std::uint32_t Stride = 0;
                    bool bIsPerInstance  = false;
                    auto BufferIt = BufferMap.find(Binding);
                    if (BufferIt != BufferMap.end())
                    {
                        Stride         = BufferIt->second.Stride;
                        bIsPerInstance = BufferIt->second.bIsPerInstance;
                    }
                    else
                    {
                        Stride = VariableSize;
                    }

                    UniqueBindings.emplace(Binding, Stride, bIsPerInstance ? vk::VertexInputRate::eInstance : vk::VertexInputRate::eVertex);
                
                    if (MatrixColumns > 1)
                    {
                        vk::Format ColumnFormat = static_cast<vk::Format>(Input->format);
                        for (std::uint32_t i = 0; i != MatrixColumns; ++i)
                        {
                            ReflectionInfo_.VertexInputAttributes.emplace_back(
                                Location + i, Binding, ColumnFormat, Offset + (ScalarSize * MatrixRows * i));
                        }

                        NpgsCoreTrace("Vertex Attribute \"{}\" at location={}, binding={}, offset={}, stride={}, rate={} (matrix)",
                                      Input->name ? Input->name : "unnamed", Location, Binding, Offset, Stride,
                                      bIsPerInstance ? "per instance" : "per vertex");
                    }
                    else
                    {
                        vk::Format Format = static_cast<vk::Format>(Input->format);
                        ReflectionInfo_.VertexInputAttributes.emplace_back(Location, Binding, Format, Offset);

                        NpgsCoreTrace("Vertex Attribute \"{}\" at location={}, binding={}, offset={}, stride={}, rate={}",
                                      Input->name ? Input->name : "unnamed", Location, Binding, Offset, Stride,
                                      bIsPerInstance ? "per instance" : "per vertex");
                    }

                    if (LocationIt == LocationMap.end())
                    {
                        CurrentOffset += VariableSize;
                    }
                }

                ReflectionInfo_.VertexInputBindings =
                    std::vector<vk::VertexInputBindingDescription>(UniqueBindings.begin(), UniqueBindings.end());
            }
        }

        NpgsCoreTrace("Shader reflection completed.");
    }

    void FShader::AddPushConstantRange(vk::PushConstantRange NewRange)
    {
        if (NewRange.size == 0)
        {
            return;
        }

        for (auto it = ReflectionInfo_.PushConstants.begin(); it != ReflectionInfo_.PushConstants.end();)
        {
            const auto& ExistingRange = *it;
            bool bOverlaps = NewRange.offset < (ExistingRange.offset + ExistingRange.size) &&
                             ExistingRange.offset < (NewRange.offset + NewRange.size);

            if (bOverlaps)
            {
                std::uint32_t MinOffset = std::min(NewRange.offset, ExistingRange.offset);
                std::uint32_t MaxEnd    = std::max(NewRange.offset + NewRange.size, ExistingRange.offset + ExistingRange.size);

                NewRange.offset      = MinOffset;
                NewRange.size        = MaxEnd - MinOffset;
                NewRange.stageFlags |= ExistingRange.stageFlags;

                it = ReflectionInfo_.PushConstants.erase(it);
            }
            else
            {
                ++it;
            }
        }

        ReflectionInfo_.PushConstants.push_back(NewRange);
        std::ranges::sort(ReflectionInfo_.PushConstants, [](const auto& Lhs, const auto& Rhs) -> bool
        {
            return Lhs.offset < Rhs.offset;
        });
    }

    void FShader::AddDescriptorSetBindings(std::uint32_t Set, const vk::DescriptorSetLayoutBinding& LayoutBinding)
    {
        auto it = ReflectionInfo_.DescriptorSetBindings.find(Set);
        if (it == ReflectionInfo_.DescriptorSetBindings.end())
        {
            ReflectionInfo_.DescriptorSetBindings[Set].push_back(LayoutBinding);
            return;
        }

        bool  bMergedStage          = false;
        auto& ExistedLayoutBindings = it->second;
        for (auto& ExistedLayoutBinding : ExistedLayoutBindings)
        {
            if (ExistedLayoutBinding.binding         == LayoutBinding.binding &&
                ExistedLayoutBinding.descriptorType  == LayoutBinding.descriptorType &&
                ExistedLayoutBinding.descriptorCount == LayoutBinding.descriptorCount)
            {
                ExistedLayoutBinding.stageFlags |= LayoutBinding.stageFlags;
                bMergedStage = true;
                break;
            }
        }

        if (!bMergedStage)
        {
            ExistedLayoutBindings.push_back(LayoutBinding);
        }
    }

    void FShader::CreateDescriptorSetLayouts()
    {
        if (ReflectionInfo_.DescriptorSetBindings.empty())
        {
            return;
        }

        std::vector<vk::ShaderStageFlags> Stages;
        for (auto& [Set, Bindings] : ReflectionInfo_.DescriptorSetBindings)
        {
            Stages.clear();
            for (const auto& Binding : Bindings)
            {
                Stages.push_back(Binding.stageFlags);
            }

            vk::ShaderStageFlags CombinedStages = std::accumulate(Stages.begin(), Stages.end(), vk::ShaderStageFlags(),
            [](vk::ShaderStageFlags Lhs, vk::ShaderStageFlags Rhs) -> vk::ShaderStageFlags
            {
                return Lhs | Rhs;
            });

            for (auto& Binding : Bindings)
            {
                Binding.stageFlags |= CombinedStages;
            }

            std::string LayoutName = "DescriptorSetLayout_Set" + std::to_string(Set);
            vk::DescriptorSetLayoutCreateInfo LayoutCreateInfo(vk::DescriptorSetLayoutCreateFlagBits::eDescriptorBufferEXT, Bindings);
            FVulkanDescriptorSetLayout Layout(VulkanContext_->GetDevice(), LayoutName, LayoutCreateInfo);
            DescriptorSetLayoutsMap_.emplace(Set, std::move(Layout));

            NpgsCoreTrace("Created descriptor set layout for set {} with {} bindings", Set, Bindings.size());
        }
    }

    void FShader::GenerateDescriptorInfos()
    {
        if (DescriptorSetLayoutsMap_.empty())
        {
            return;
        }

        vk::Device Device = VulkanContext_->GetDevice();

        for (const auto& [Set, Layout] : DescriptorSetLayoutsMap_)
        {
            vk::DeviceSize LayoutSize = Device.getDescriptorSetLayoutSizeEXT(*Layout);
            FDescriptorSetInfo SetInfo{ .Set = Set, .Size = LayoutSize };

            const auto& Bindings = ReflectionInfo_.DescriptorSetBindings[Set];
            for (const auto& Binding : Bindings)
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

            DescriptorSetInfos_[Set] = std::move(SetInfo);
        }
    }
} // namespace Npgs
