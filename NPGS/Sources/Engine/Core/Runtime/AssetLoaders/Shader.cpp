#include "Shader.h"

#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <ios>
#include <numeric>
#include <set>
#include <utility>

#include <spirv_cross/spirv_reflect.hpp>

#include "Engine/Core/Runtime/AssetLoaders/AssetManager.h"
#include "Engine/Utils/Logger.h"

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

        vk::Format GetVectorFormat(spirv_cross::SPIRType::BaseType BaseType, std::uint32_t Components)
        {
            switch (BaseType)
            {
            case spirv_cross::SPIRType::BaseType::Int:
                switch (Components)
                {
                case 1:
                    return vk::Format::eR32Sint;
                case 2:
                    return vk::Format::eR32G32Sint;
                case 3:
                    return vk::Format::eR32G32B32Sint;
                case 4:
                    return vk::Format::eR32G32B32A32Sint;
                default:
                    return vk::Format::eUndefined;
                }
            case spirv_cross::SPIRType::BaseType::UInt:
                switch (Components)
                {
                case 1:
                    return vk::Format::eR32Uint;
                case 2:
                    return vk::Format::eR32G32Uint;
                case 3:
                    return vk::Format::eR32G32B32Uint;
                case 4:
                    return vk::Format::eR32G32B32A32Uint;
                default:
                    return vk::Format::eUndefined;
                }
            case spirv_cross::SPIRType::BaseType::Float:
                switch (Components)
                {
                case 1:
                    return vk::Format::eR32Sfloat;
                case 2:
                    return vk::Format::eR32G32Sfloat;
                case 3:
                    return vk::Format::eR32G32B32Sfloat;
                case 4:
                    return vk::Format::eR32G32B32A32Sfloat;
                default:
                    return vk::Format::eUndefined;
                }
            case spirv_cross::SPIRType::BaseType::Double:
                switch (Components)
                {
                case 1:
                    return vk::Format::eR64Sfloat;
                case 2:
                    return vk::Format::eR64G64Sfloat;
                case 3:
                    return vk::Format::eR64G64B64Sfloat;
                case 4:
                    return vk::Format::eR64G64B64A64Sfloat;
                default:
                    return vk::Format::eUndefined;
                }
            default:
                return vk::Format::eUndefined;
            }
        }

        std::uint32_t GetTypeSize(spirv_cross::SPIRType::BaseType BaseType)
        {
            switch (BaseType)
            {
            case spirv_cross::SPIRType::BaseType::Int:
                return sizeof(int);
            case spirv_cross::SPIRType::BaseType::UInt:
                return sizeof(unsigned int);
            case spirv_cross::SPIRType::BaseType::Float:
                return sizeof(float);
            case spirv_cross::SPIRType::BaseType::Double:
                return sizeof(double);
            default:
                return 0;
            }
        }
    }

    FShader::FShader(FVulkanContext* VulkanContext, const std::vector<std::string>& ShaderFiles, const FResourceInfo& ResourceInfo)
        : VulkanContext_(VulkanContext)
    {
        InitializeShaders(ShaderFiles, ResourceInfo);
        CreateDescriptorSetLayouts();
        GenerateDescriptorInfos();
    }

    FShader::FShader(FShader&& Other) noexcept
        : VulkanContext_(std::exchange(Other.VulkanContext_, nullptr))
        , ReflectionInfo_(std::exchange(Other.ReflectionInfo_, {}))
        , ShaderModules_(std::move(Other.ShaderModules_))
        , PushConstantOffsetsMap_(std::move(Other.PushConstantOffsetsMap_))
        , DescriptorSetLayoutsMap_(std::move(Other.DescriptorSetLayoutsMap_))
        , DescriptorSetInfos_(std::move(Other.DescriptorSetInfos_))
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
            DescriptorSetLayoutsMap_ = std::move(Other.DescriptorSetLayoutsMap_);
            DescriptorSetInfos_      = std::move(Other.DescriptorSetInfos_);
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
        std::vector<vk::DescriptorSetLayout> NativeTypeLayouts;
        for (const auto& [Set, Layout] : DescriptorSetLayoutsMap_)
        {
            NativeTypeLayouts.push_back(*Layout);
        }

        return NativeTypeLayouts;
    }

    void FShader::InitializeShaders(const std::vector<std::string>& ShaderFiles, const FResourceInfo& ResourceInfo)
    {
        for (const auto& Filename : ShaderFiles)
        {
            FShaderInfo ShaderInfo = LoadShader(GetAssetFullPath(EAssetType::kShader, Filename));
            if (ShaderInfo.Code.empty())
            {
                return;
            }

            vk::ShaderModuleCreateInfo ShaderModuleCreateInfo({}, ShaderInfo.Code);
            FVulkanShaderModule ShaderModule(VulkanContext_->GetDevice(), ShaderModuleCreateInfo);
            ShaderModules_.emplace_back(ShaderInfo.Stage, std::move(ShaderModule));

            ReflectShader(ShaderInfo, ResourceInfo);
        }
    }

    FShader::FShaderInfo FShader::LoadShader(const std::string& Filename)
    {
        if (!std::filesystem::exists(Filename))
        {
            NpgsCoreError("Failed to load shader: \"{}\": No such file or directory.", Filename);
            return {};
        }

        std::ifstream ShaderFile(Filename, std::ios::ate | std::ios::binary);
        if (!ShaderFile.is_open())
        {
            NpgsCoreError("Failed to open shader: \"{}\": Access denied.", Filename);
            return {};
        }

        std::size_t FileSize = static_cast<std::size_t>(ShaderFile.tellg());
        std::vector<std::uint32_t> ShaderCode(FileSize / sizeof(std::uint32_t));
        ShaderFile.seekg(0);
        ShaderFile.read(reinterpret_cast<char*>(ShaderCode.data()), static_cast<std::streamsize>(FileSize));
        ShaderFile.close();

        vk::ShaderStageFlagBits Stage = GetShaderStageFromFilename(Filename);
        return { ShaderCode, Stage };
    }

    void FShader::ReflectShader(const FShaderInfo& ShaderInfo, const FResourceInfo& ResourceInfo)
    {
        std::unique_ptr<spirv_cross::CompilerReflection> Reflection;
        std::unique_ptr<spirv_cross::ShaderResources>    Resources;
        try
        {
            Reflection = std::make_unique<spirv_cross::CompilerReflection>(ShaderInfo.Code);
            Resources  = std::make_unique<spirv_cross::ShaderResources>(Reflection->get_shader_resources());
        }
        catch (const spirv_cross::CompilerError& e)
        {
            NpgsCoreError("SPIR-V Cross compiler error: {}", e.what());
            return;
        }
        catch (const std::exception& e)
        {
            NpgsCoreError("Shader reflection failed: {}", e.what());
            return;
        }

        for (const auto& PushConstant : Resources->push_constant_buffers)
        {
            const auto&   Type        = Reflection->get_type(PushConstant.type_id);
            std::size_t   BufferSize  = Reflection->get_declared_struct_size(Type);
            std::uint32_t TotalOffset = 0;

            if (ReflectionInfo_.PushConstants.size() > 0)
            {
                TotalOffset = ReflectionInfo_.PushConstants.back().offset + ReflectionInfo_.PushConstants.back().size;
            }

            const auto& PushConstantNames = ResourceInfo.PushConstantInfos.at(ShaderInfo.Stage);
            for (std::uint32_t i = 0; i != Type.member_types.size(); ++i)
            {
                const std::string& MemberName   = PushConstantNames[i];
                std::uint32_t      MemberOffset = Reflection->get_member_decoration(Type.self, i, spv::DecorationOffset);

                PushConstantOffsetsMap_[MemberName] = MemberOffset;
                NpgsCoreTrace("  Member \"{}\" at offset={}", MemberName, MemberOffset);
            }

            NpgsCoreTrace("Push Constant \"{}\" size={} bytes, offset={}", PushConstant.name, BufferSize - TotalOffset, TotalOffset);
            vk::PushConstantRange PushConstantRange(ShaderInfo.Stage, TotalOffset, static_cast<std::uint32_t>(BufferSize - TotalOffset));
            ReflectionInfo_.PushConstants.push_back(std::move(PushConstantRange));
        }

        std::unordered_map<std::uint64_t, bool> DynemicBufferMap;
        for (const auto& Buffer : ResourceInfo.ShaderBufferInfos)
        {
            std::uint64_t Key = (static_cast<std::uint64_t>(Buffer.Set) << 32) | Buffer.Binding;
            DynemicBufferMap[Key] = Buffer.bIsDynamic;
        }

        auto CheckDynamic = [&DynemicBufferMap](std::uint32_t Set, std::uint32_t Binding) -> bool
        {
            std::uint64_t Key = (static_cast<std::uint64_t>(Set) << 32) | Binding;
            auto it = DynemicBufferMap.find(Key);
            return it != DynemicBufferMap.end() ? it->second : false;
        };

        for (const auto& UniformBuffer : Resources->uniform_buffers)
        {
            std::uint32_t Set     = Reflection->get_decoration(UniformBuffer.id, spv::DecorationDescriptorSet);
            std::uint32_t Binding = Reflection->get_decoration(UniformBuffer.id, spv::DecorationBinding);

            const auto&   Type       = Reflection->get_type(UniformBuffer.type_id);
            std::uint32_t ArraySize  = Type.array.empty() ? 1 : Type.array[0];
            bool          bIsDynamic = CheckDynamic(Set, Binding);

            NpgsCoreTrace("UBO \"{}\" at set={}, binding={} is {}, array_size={}",
                          UniformBuffer.name, Set, Binding, bIsDynamic ? "dynamic" : "static", ArraySize);

            vk::DescriptorSetLayoutBinding LayoutBinding = vk::DescriptorSetLayoutBinding()
                .setBinding(Binding)
                .setDescriptorType(bIsDynamic ? vk::DescriptorType::eUniformBufferDynamic : vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(ArraySize)
                .setStageFlags(ShaderInfo.Stage);

            AddDescriptorSetBindings(Set, LayoutBinding);
        }

        for (const auto& StorageBuffer : Resources->storage_buffers)
        {
            std::uint32_t Set     = Reflection->get_decoration(StorageBuffer.id, spv::DecorationDescriptorSet);
            std::uint32_t Binding = Reflection->get_decoration(StorageBuffer.id, spv::DecorationBinding);

            const auto&   Type       = Reflection->get_type(StorageBuffer.type_id);
            std::uint32_t ArraySize  = Type.array.empty() ? 1 : Type.array[0];
            bool          bIsDynamic = CheckDynamic(Set, Binding);

            NpgsCoreTrace("SSBO \"{}\" at set={}, binding={} is {}, array_size={}",
                          StorageBuffer.name, Set, Binding, bIsDynamic ? "dynamic" : "static", ArraySize);

            vk::DescriptorSetLayoutBinding LayoutBinding = vk::DescriptorSetLayoutBinding()
                .setBinding(Binding)
                .setDescriptorType(bIsDynamic ? vk::DescriptorType::eStorageBufferDynamic : vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(ArraySize)
                .setStageFlags(ShaderInfo.Stage);

            AddDescriptorSetBindings(Set, LayoutBinding);
        }

        for (const auto& SampledImage : Resources->sampled_images)
        {
            std::uint32_t Set     = Reflection->get_decoration(SampledImage.id, spv::DecorationDescriptorSet);
            std::uint32_t Binding = Reflection->get_decoration(SampledImage.id, spv::DecorationBinding);

            const auto&   Type      = Reflection->get_type(SampledImage.type_id);
            std::uint32_t ArraySize = Type.array.empty() ? 1 : Type.array[0];

            NpgsCoreTrace("Sampled Image \"{}\" at set={}, binding={}, array_size={}",
                          SampledImage.name, Set, Binding, ArraySize);

            vk::DescriptorSetLayoutBinding LayoutBinding = vk::DescriptorSetLayoutBinding()
                .setBinding(Binding)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(ArraySize)
                .setStageFlags(ShaderInfo.Stage);

            AddDescriptorSetBindings(Set, LayoutBinding);
        }

        for (const auto& Sampler : Resources->separate_samplers)
        {
            std::uint32_t Set     = Reflection->get_decoration(Sampler.id, spv::DecorationDescriptorSet);
            std::uint32_t Binding = Reflection->get_decoration(Sampler.id, spv::DecorationBinding);

            const auto&   Type      = Reflection->get_type(Sampler.type_id);
            std::uint32_t ArraySize = Type.array.empty() ? 1 : Type.array[0];

            NpgsCoreTrace("Separate Sampler \"{}\" at set={}, binding={}, array_size={}", Sampler.name, Set, Binding, ArraySize);

            vk::DescriptorSetLayoutBinding LayoutBinding = vk::DescriptorSetLayoutBinding()
                .setBinding(Binding)
                .setDescriptorType(vk::DescriptorType::eSampler)
                .setDescriptorCount(ArraySize)
                .setStageFlags(ShaderInfo.Stage);

            AddDescriptorSetBindings(Set, LayoutBinding);
        }

        for (const auto& Image : Resources->separate_images)
        {
            std::uint32_t Set     = Reflection->get_decoration(Image.id, spv::DecorationDescriptorSet);
            std::uint32_t Binding = Reflection->get_decoration(Image.id, spv::DecorationBinding);

            const auto&   Type      = Reflection->get_type(Image.type_id);
            std::uint32_t ArraySize = Type.array.empty() ? 1 : Type.array[0];

            NpgsCoreTrace("Separate Image \"{}\" at set={}, binding={}, array_size={}", Image.name, Set, Binding, ArraySize);

            vk::DescriptorSetLayoutBinding LayoutBinding = vk::DescriptorSetLayoutBinding()
                .setBinding(Binding)
                .setDescriptorType(vk::DescriptorType::eSampledImage)
                .setDescriptorCount(ArraySize)
                .setStageFlags(ShaderInfo.Stage);

            AddDescriptorSetBindings(Set, LayoutBinding);
        }

        for (const auto& Image : Resources->storage_images)
        {
            std::uint32_t Set     = Reflection->get_decoration(Image.id, spv::DecorationDescriptorSet);
            std::uint32_t Binding = Reflection->get_decoration(Image.id, spv::DecorationBinding);

            const auto&   Type      = Reflection->get_type(Image.type_id);
            std::uint32_t ArraySize = Type.array.empty() ? 1 : Type.array[0];

            NpgsCoreTrace("Storage Image \"{}\" at set={}, binding={}, array_size={}", Image.name, Set, Binding, ArraySize);

            vk::DescriptorSetLayoutBinding LayoutBinding = vk::DescriptorSetLayoutBinding()
                .setBinding(Binding)
                .setDescriptorType(vk::DescriptorType::eStorageImage)
                .setDescriptorCount(ArraySize)
                .setStageFlags(ShaderInfo.Stage);

            AddDescriptorSetBindings(Set, LayoutBinding);
        }

        if (ShaderInfo.Stage == vk::ShaderStageFlagBits::eVertex)
        {
            std::unordered_map<std::uint32_t, FVertexBufferInfo> BufferMap;
            for (const auto& Buffer : ResourceInfo.VertexBufferInfos)
            {
                BufferMap[Buffer.Binding] = Buffer;
            }

            std::unordered_map<std::uint32_t, std::pair<std::uint32_t, std::uint32_t>> LocationMap;
            for (const auto& Vertex : ResourceInfo.VertexAttributeInfos)
            {
                LocationMap[Vertex.Location] = std::make_pair(Vertex.Binding, Vertex.Offset);
            }

            std::uint32_t CurrentBinding = 0;
            std::uint32_t CurrentOffset  = 0;
            std::set<vk::VertexInputBindingDescription> UniqueBindings;

            for (const auto& Input : Resources->stage_inputs)
            {
                const auto&   Type     = Reflection->get_type(Input.type_id);
                std::uint32_t Location = Reflection->get_decoration(Input.id, spv::DecorationLocation);

                auto LocationIt = LocationMap.find(Location);
                std::uint32_t Binding = LocationIt != LocationMap.end() ? LocationIt->second.first  : CurrentBinding;
                std::uint32_t Offset  = LocationIt != LocationMap.end() ? LocationIt->second.second : CurrentOffset;

                auto BufferIt = BufferMap.find(Binding);
                std::uint32_t Stride = BufferIt != BufferMap.end() ? BufferIt->second.Stride : GetTypeSize(Type.basetype) * Type.vecsize;
                bool bIsPerInstance  = BufferIt != BufferMap.end() ? BufferIt->second.bIsPerInstance : false;

                UniqueBindings.emplace(Binding, Stride, bIsPerInstance ? vk::VertexInputRate::eInstance : vk::VertexInputRate::eVertex);

                bool bIsMatrix = Type.columns > 1;
                if (bIsMatrix)
                {
                    Stride = GetTypeSize(Type.basetype) * Type.columns * Type.vecsize;
                    for (std::uint32_t Column = 0; Column != Type.columns; ++Column)
                    {
                        ReflectionInfo_.VertexInputAttributes.emplace_back(
                            Location + Column, Binding, GetVectorFormat(Type.basetype, Type.vecsize),
                            Offset + GetTypeSize(Type.basetype) * Column * Type.vecsize);
                    }

                    NpgsCoreTrace("Vertex Attribute \"{}\" at location={}, binding={}, offset={}, stride={}, rate={} (matrix)",
                                  Input.name, Location, Binding, Offset, Stride, bIsPerInstance ? "per instance" : "per vertex");
                }
                else
                {
                    ReflectionInfo_.VertexInputAttributes.emplace_back(
                        Location, Binding, GetVectorFormat(Type.basetype, Type.vecsize), Offset);

                    NpgsCoreTrace("Vertex Attribute \"{}\" at location={}, binding={}, offset={}, stride={}, rate={}",
                                  Input.name, Location, Binding, Offset, Stride, bIsPerInstance ? "per instance" : "per vertex");
                }

                if (LocationIt == LocationMap.end())
                {
                    ++CurrentBinding;
                }
            }

            ReflectionInfo_.VertexInputBindings =
                std::vector<vk::VertexInputBindingDescription>(UniqueBindings.begin(), UniqueBindings.end());
        }

        NpgsCoreTrace("Shader reflection completed.");
    }

    void FShader::AddDescriptorSetBindings(std::uint32_t Set, vk::DescriptorSetLayoutBinding& LayoutBinding)
    {
        auto it = ReflectionInfo_.DescriptorSetBindings.find(Set);
        if (it == ReflectionInfo_.DescriptorSetBindings.end())
        {
            ReflectionInfo_.DescriptorSetBindings[Set].push_back(std::move(LayoutBinding));
            return;
        }

        bool bMergedStage = false;
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
            ExistedLayoutBindings.push_back(std::move(LayoutBinding));
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

            vk::DescriptorSetLayoutCreateInfo LayoutCreateInfo(vk::DescriptorSetLayoutCreateFlagBits::eDescriptorBufferEXT, Bindings);
            FVulkanDescriptorSetLayout Layout(VulkanContext_->GetDevice(), LayoutCreateInfo);
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
                };

                SetInfo.Bindings.push_back(std::move(BindingInfo));
            }

            DescriptorSetInfos_[Set] = std::move(SetInfo);
        }
    }
} // namespace Npgs
