#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan.hpp>
#include "Engine/Core/Runtime/AssetLoaders/AssetManager.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"

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

        void CreateGraphicsPipeline(const std::string& PipelineName, const std::string& ShaderName,
                                    FGraphicsPipelineCreateInfoPack& GraphicsPipelineCreateInfoPack);

        void CreateComputePipeline(const std::string& PipelineName, const std::string& ShaderName,
                                   vk::ComputePipelineCreateInfo* ComputePipelineCreateInfo = nullptr);

        void RemovePipeline(const std::string& Name);
        vk::PipelineLayout GetPipelineLayout(const std::string& Name) const;
        vk::Pipeline GetPipeline(const std::string& Name) const;

    private:
        void RegisterCallback(const std::string& Name, EPipelineType Type);

    private:
        FVulkanContext*                                                  _VulkanContext;
        FAssetManager*                                                   _AssetManager;
        std::unordered_map<std::string, FGraphicsPipelineCreateInfoPack> _GraphicsPipelineCreateInfoPacks;
        std::unordered_map<std::string, vk::ComputePipelineCreateInfo>   _ComputePipelineCreateInfos;
        std::unordered_map<std::string, FVulkanPipelineLayout>           _PipelineLayouts;
        std::unordered_map<std::string, FVulkanPipeline>                 _Pipelines;
    };
} // namespace Npgs

#include "PipelineManager.inl"
