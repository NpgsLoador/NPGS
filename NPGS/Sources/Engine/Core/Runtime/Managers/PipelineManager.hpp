#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan.hpp>

#include "Engine/Core/Runtime/Graphics/Vulkan/Context.hpp"
#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.hpp"
#include "Engine/Core/Runtime/Managers/AssetManager.hpp"
#include "Engine/Utils/Hash.hpp"

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
        
        std::unordered_map<std::string, FGraphicsPipelineCreateInfoPack,
                           Utils::FStringViewHeteroHash, Utils::FStringViewHeteroEqual> GraphicsPipelineCreateInfoPacks_;

        std::unordered_map<std::string, vk::ComputePipelineCreateInfo,
                           Utils::FStringViewHeteroHash, Utils::FStringViewHeteroEqual> ComputePipelineCreateInfos_;
        
        std::unordered_map<std::string, FVulkanPipelineLayout,
                           Utils::FStringViewHeteroHash, Utils::FStringViewHeteroEqual> PipelineLayouts_;

        std::unordered_map<std::string, FVulkanPipeline,
                           Utils::FStringViewHeteroHash, Utils::FStringViewHeteroEqual> Pipelines_;
    };
} // namespace Npgs

#include "PipelineManager.inl"
