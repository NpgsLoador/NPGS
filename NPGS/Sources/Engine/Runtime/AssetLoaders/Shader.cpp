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

    FShader::FShader(FVulkanContext* VulkanContext, std::string_view Filename, const FResourceInfo& ResourceInfo)
        : VulkanContext_(VulkanContext)
    {
        InitializeShaders(Filename, ResourceInfo);
    }

    FShader::FShader(FShader&& Other) noexcept
        : VulkanContext_(std::exchange(Other.VulkanContext_, nullptr))
        , ReflectionInfo_(std::exchange(Other.ReflectionInfo_, {}))
        , ShaderCode_(std::move(Other.ShaderCode_))
        , PushConstantOffsetsMap_(std::move(Other.PushConstantOffsetsMap_))
    {
    }

    FShader& FShader::operator=(FShader&& Other) noexcept
    {
        if (this != &Other)
        {
            VulkanContext_          = std::exchange(Other.VulkanContext_, nullptr);
            ReflectionInfo_         = std::exchange(Other.ReflectionInfo_, {});
            ShaderCode_             = std::move(Other.ShaderCode_);
            PushConstantOffsetsMap_ = std::move(Other.PushConstantOffsetsMap_);
        }

        return *this;
    }

    void FShader::InitializeShaders(std::string_view Filename, const FResourceInfo& ResourceInfo)
    {
        LoadShader(GetAssetFullPath(EAssetType::kShader, Filename));
        if (ShaderCode_.empty())
        {
            return;
        }

        ReflectShader(ResourceInfo);
    }

    void FShader::LoadShader(std::string Filename)
    {
        if (!std::filesystem::exists(Filename))
        {
            NpgsCoreError("Failed to load shader: \"{}\": No such file or directory.", Filename);
            return;
        }

        FFileLoader ShaderCode;
        if (!ShaderCode.Load(Filename))
        {
            NpgsCoreError("Failed to open shader: \"{}\": Access denied.", Filename);
            return;
        }

        ReflectionInfo_.Stage = GetShaderStageFromFilename(Filename);
        Filename_             = std::move(Filename);
        ShaderCode_           = ShaderCode.StripData<std::uint32_t>();
    }

    void FShader::ReflectShader(const FResourceInfo& ResourceInfo)
    {
        spv_reflect::ShaderModule ShaderModule(ShaderCode_, SPV_REFLECT_MODULE_FLAG_NO_COPY);
        if (ShaderModule.GetResult() != SPV_REFLECT_RESULT_SUCCESS)
        {
            NpgsCoreError("Failed to reflect shader.");
            return;
        }

        ReflectPushConstants(ResourceInfo, ShaderModule);
        ReflectSpecializationConstants(ResourceInfo, ShaderModule);
        ReflectDescriptorSets(ResourceInfo, ShaderModule);
        if (ReflectionInfo_.Stage == vk::ShaderStageFlagBits::eVertex)
        {
            ReflectVertexInput(ResourceInfo, ShaderModule);
        }

        NpgsCoreTrace("Shader reflection completed.");
    }

    void FShader::ReflectPushConstants(const FResourceInfo& ResourceInfo, const spv_reflect::ShaderModule& ShaderModule)
    {
        std::uint32_t Count = 0;
        ShaderModule.EnumeratePushConstantBlocks(&Count, nullptr);
        if (Count == 0)
        {
            return;
        }

        std::vector<SpvReflectBlockVariable*> PushConstants(Count);
        ShaderModule.EnumeratePushConstantBlocks(&Count, PushConstants.data());

        for (const auto* Block : PushConstants)
        {
            for (std::uint32_t i = 0; i != Block->member_count; ++i)
            {
                const auto& Member = Block->members[i];
                std::string_view MemberName = ResourceInfo.PushConstantNames[i];

                if (!MemberName.empty())
                {
                    PushConstantOffsetsMap_[std::string(MemberName)] = Member.offset;
                    NpgsCoreTrace("  Member \"{}\" at offset={}", MemberName, Member.offset);
                }
            }

            NpgsCoreTrace("Push Constant \"{}\" size={} bytes, offset={}",
                          Block->name ? Block->name : "unnamed", Block->size, Block->offset);

            vk::PushConstantRange PushConstantRange(ReflectionInfo_.Stage, Block->offset, Block->size);
            ReflectionInfo_.PushConstants.push_back(PushConstantRange);
        }

    }

    void FShader::ReflectSpecializationConstants(const FResourceInfo& ResourceInfo, const spv_reflect::ShaderModule& ShaderModule)
    {
        std::uint32_t Count = 0;
        ShaderModule.EnumerateSpecializationConstants(&Count, nullptr);
        if (Count == 0)
        {
            return;
        }

        std::vector<SpvReflectSpecializationConstant*> SpecializationConstants(Count);
        ShaderModule.EnumerateSpecializationConstants(&Count, SpecializationConstants.data());

        for (const auto* Constant : SpecializationConstants)
        {
            std::uint32_t Id = Constant->constant_id;
            const auto& Name = ResourceInfo.SpecializationConstantInfos.at(Id);
            ReflectionInfo_.SpecializationConstants.emplace(Name, Id);

            NpgsCoreTrace("Specialization Constants \"{}\" id={})", Name, Id);
        }
    }

    void FShader::ReflectDescriptorSets(const FResourceInfo& ResourceInfo, const spv_reflect::ShaderModule& ShaderModule)
    {
        std::uint32_t Count = 0;
        ShaderModule.EnumerateDescriptorSets(&Count, nullptr);
        if (Count == 0)
        {
            return;
        }

        std::vector<SpvReflectDescriptorSet*> DescriptorSets(Count);
        ShaderModule.EnumerateDescriptorSets(&Count, DescriptorSets.data());

        for (const auto* Set : DescriptorSets)
        {
            for (std::uint32_t i = 0; i != Set->binding_count; ++i)
            {
                const auto*     Binding = Set->bindings[i];
                vk::DescriptorType Type = static_cast<vk::DescriptorType>(Binding->descriptor_type);
                std::uint32_t ArraySize = Binding->count;

                NpgsCoreTrace("Descriptor \"{}\" at set={}, binding={}, type={}, array_size={}",
                              Binding->name ? Binding->name : "unnamed", Set->set,
                              Binding->binding, std::to_underlying(Type), ArraySize);

                vk::DescriptorSetLayoutBinding LayoutBinding = vk::DescriptorSetLayoutBinding()
                    .setBinding(Binding->binding)
                    .setDescriptorType(Type)
                    .setDescriptorCount(ArraySize)
                    .setStageFlags(ReflectionInfo_.Stage);

                ReflectionInfo_.SetLayoutBindings[Set->set].push_back(LayoutBinding);
            }
        }
    }

    void FShader::ReflectVertexInput(const FResourceInfo& ResourceInfo, const spv_reflect::ShaderModule& ShaderModule)
    {
        std::uint32_t Count = 0;
        ShaderModule.EnumerateInputVariables(&Count, nullptr);
        if (Count == 0)
        {
            return;
        }

        std::vector<SpvReflectInterfaceVariable*> InputVariables(Count);
        ShaderModule.EnumerateInputVariables(&Count, InputVariables.data());

        std::ranges::sort(InputVariables, [](const auto* Lhs, const auto* Rhs) -> bool
        {
            return Lhs->location < Rhs->location;
        });

        ankerl::unordered_dense::map<std::uint32_t, FVertexBufferInfo> BufferMap;
        for (const auto& Buffer : ResourceInfo.VertexBufferInfos)
        {
            BufferMap[Buffer.Binding] = Buffer;
        }

        // [Location, [Binding, Offset]]
        ankerl::unordered_dense::map<std::uint32_t, std::pair<std::uint32_t, std::uint32_t>> LocationMap;
        for (const auto& Vertex : ResourceInfo.VertexAttributeInfos)
        {
            LocationMap[Vertex.Location] = std::make_pair(Vertex.Binding, Vertex.Offset);
        }

        std::uint32_t CurrentBinding = 0;
        std::uint32_t CurrentOffset  = 0;
        std::set<vk::VertexInputBindingDescription2EXT> UniqueBindings;

        for (const auto* Input : InputVariables)
        {
            if (Input->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN)
            {
                continue;
            }

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
            VariableSize = MatrixColumns > 1 ? (ScalarSize * MatrixRows * MatrixColumns) : (ScalarSize * VectorSize);

            std::uint32_t Stride  = 0;
            bool bIsPerInstance   = false;
            std::uint32_t Divisor = 0;

            auto BufferIt = BufferMap.find(Binding);
            if (BufferIt != BufferMap.end())
            {
                Stride         = BufferIt->second.Stride;
                bIsPerInstance = BufferIt->second.bIsPerInstance;
                Divisor        = BufferIt->second.Divisor;
            }
            else
            {
                Stride = VariableSize;
            }

            auto InputRate = bIsPerInstance ? vk::VertexInputRate::eInstance : vk::VertexInputRate::eVertex;
            UniqueBindings.emplace(Binding, Stride, InputRate, Divisor);

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
            std::vector<vk::VertexInputBindingDescription2EXT>(UniqueBindings.begin(), UniqueBindings.end());
    }
} // namespace Npgs
