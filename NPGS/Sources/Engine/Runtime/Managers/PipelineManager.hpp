#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <vulkan/vulkan.hpp>

#include "Engine/Core/Utils/Hash.hpp"
#include "Engine/Runtime/Graphics/Vulkan/Context.hpp"
#include "Engine/Runtime/Graphics/Vulkan/Wrappers.hpp"
#include "Engine/Runtime/Managers/AssetManager.hpp"

namespace Npgs
{
    class FPipelineManager
    {
    private:
        enum class EPipelineType : std::uint8_t
        {
            kGraphics,
            kCompute
        };

    public:
        FPipelineManager(FVulkanContext* VulkanContext, FAssetManager* AssetManager);
        FPipelineManager(const FPipelineManager&) = delete;
        FPipelineManager(FPipelineManager&&)      = delete;
        ~FPipelineManager()                       = default;

        FPipelineManager& operator=(const FPipelineManager&) = delete;
        FPipelineManager& operator=(FPipelineManager&&)      = delete;

        void CreateGraphicsPipeline(std::string_view PipelineName, std::string_view ShaderName,
                                    FGraphicsPipelineCreateInfoPack& GraphicsPipelineCreateInfoPack);

        void CreateComputePipeline(std::string_view PipelineName, std::string_view ShaderName,
                                   vk::ComputePipelineCreateInfo* ComputePipelineCreateInfo = nullptr);

        void RemovePipeline(std::string_view Name);
        vk::PipelineLayout GetPipelineLayout(const std::string& Name) const;
        vk::Pipeline GetPipeline(const std::string& Name) const;

    private:
        void RegisterCallback(std::string_view Name, EPipelineType Type);

    private:
        FVulkanContext*                                                  VulkanContext_;
        FAssetManager*                                                   AssetManager_;
        
        Utils::FStringHeteroHashTable<std::string, FGraphicsPipelineCreateInfoPack> GraphicsPipelineCreateInfoPacks_;
        Utils::FStringHeteroHashTable<std::string, vk::ComputePipelineCreateInfo>   ComputePipelineCreateInfos_;
        Utils::FStringHeteroHashTable<std::string, FVulkanPipelineLayout>           PipelineLayouts_;
        Utils::FStringHeteroHashTable<std::string, FVulkanPipeline>                 Pipelines_;
    };
} // namespace Npgs

#include "PipelineManager.inl"
