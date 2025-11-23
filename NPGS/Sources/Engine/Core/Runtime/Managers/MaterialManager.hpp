#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <vulkan/vulkan.hpp>

namespace Npgs
{
    struct FSamplerInfoCreateInfo
    {
        std::string   Name;
        std::uint32_t Set{};
        std::uint32_t Binding{};
    };

    struct FImageInfoCreateInfo
    {
        enum class EImageType
        {
            kAttachment,
            kCommon
        };

        EImageType         Type;
        vk::DescriptorType Usage;
        std::string        ImageName;
        std::string        SamplerName;
        std::uint32_t      Set{};
        std::uint32_t      Binding{};
    };

    struct FDescriptorBindInfo
    {
        std::string                         DescriptorBufferName;
        std::string                         ShaderName;
        std::vector<std::string>            UniformBufferNames;
        std::vector<std::string>            StorageBufferNames;
        std::vector<FSamplerInfoCreateInfo> SamplerInfos;
        std::vector<FImageInfoCreateInfo>   ImageInfos;
    };

    class FMaterialManager
    {
    public:
        FMaterialManager()                        = default;
        FMaterialManager(const FMaterialManager&) = delete;
        FMaterialManager(FMaterialManager&&)      = delete;
        ~FMaterialManager()                       = default;

        FMaterialManager& operator=(const FMaterialManager&) = delete;
        FMaterialManager& operator=(FMaterialManager&&)      = delete;

        void BakeMaterialDescriptors(const FDescriptorBindInfo& DescriptorBindInfo);
    };
} // namespace Npgs
