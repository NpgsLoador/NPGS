#include "stdafx.h"
#include "PipelineManager.hpp"

#include <functional>
#include <utility>

#include "Engine/Core/Runtime/AssetLoaders/Shader.hpp"

namespace Npgs
{
    FPipelineManager::FPipelineManager(FVulkanContext* VulkanContext, FAssetManager* AssetManager)
        : VulkanContext_(VulkanContext)
        , AssetManager_(AssetManager)
    {
    }

    void FPipelineManager::CreateGraphicsPipeline(const std::string& PipelineName, const std::string& ShaderName,
                                                  FGraphicsPipelineCreateInfoPack& GraphicsPipelineCreateInfoPack)
    {
        auto* Shader = AssetManager_->GetAsset<FShader>(ShaderName);

        VulkanContext_->WaitIdle();
        vk::Device Device = VulkanContext_->GetDevice();

        if (ShaderName == "")
        {
            GraphicsPipelineCreateInfoPack.Update();
            FVulkanPipelineLayout PipelineLayout(Device, GraphicsPipelineCreateInfoPack.GraphicsPipelineCreateInfo.layout, "Pipeline layout");
            PipelineLayouts_.emplace(PipelineName, std::move(PipelineLayout));

            FVulkanPipeline Pipeline(Device, GraphicsPipelineCreateInfoPack);
            Pipelines_.emplace(PipelineName, std::move(Pipeline));

            RegisterCallback(PipelineName, EPipelineType::kGraphics);

            return;
        }

        vk::PipelineLayoutCreateInfo PipelineLayoutCreateInfo;
        auto NativeArray = Shader->GetDescriptorSetLayouts();
        PipelineLayoutCreateInfo.setSetLayouts(NativeArray);
        auto PushConstantRanges = Shader->GetPushConstantRanges();
        PipelineLayoutCreateInfo.setPushConstantRanges(PushConstantRanges);

        FVulkanPipelineLayout PipelineLayout(Device, PipelineLayoutCreateInfo);
        GraphicsPipelineCreateInfoPack.GraphicsPipelineCreateInfo.setLayout(*PipelineLayout);
        GraphicsPipelineCreateInfoPack.ShaderStages = Shader->CreateShaderStageCreateInfo();
        PipelineLayouts_.emplace(PipelineName, std::move(PipelineLayout));

        GraphicsPipelineCreateInfoPack.VertexInputBindings.clear();
        GraphicsPipelineCreateInfoPack.VertexInputBindings.append_range(Shader->GetVertexInputBindings());
        GraphicsPipelineCreateInfoPack.VertexInputAttributes.clear();
        GraphicsPipelineCreateInfoPack.VertexInputAttributes.append_range(Shader->GetVertexInputAttributes());
        GraphicsPipelineCreateInfoPack.Update();
        GraphicsPipelineCreateInfoPacks_.emplace(PipelineName, GraphicsPipelineCreateInfoPack);

        FVulkanPipeline Pipeline(Device, GraphicsPipelineCreateInfoPack);
        Pipelines_.emplace(PipelineName, std::move(Pipeline));

        RegisterCallback(PipelineName, EPipelineType::kGraphics);
    }

    void FPipelineManager::CreateComputePipeline(const std::string& PipelineName, const std::string& ShaderName,
                                                 vk::ComputePipelineCreateInfo* ComputePipelineCreateInfo)
    {
        auto* Shader = AssetManager_->GetAsset<FShader>(ShaderName);

        VulkanContext_->WaitIdle();
        vk::Device Device = VulkanContext_->GetDevice();

        if (ShaderName == "" && ComputePipelineCreateInfo != nullptr)
        {
            FVulkanPipelineLayout PipelineLayout(Device, ComputePipelineCreateInfo->layout, "Pipeline layout");
            PipelineLayouts_.emplace(PipelineName, std::move(PipelineLayout));

            FVulkanPipeline Pipeline(Device, *ComputePipelineCreateInfo);
            Pipelines_.emplace(PipelineName, std::move(Pipeline));

            RegisterCallback(PipelineName, EPipelineType::kCompute);

            return;
        }

        ComputePipelineCreateInfo = ComputePipelineCreateInfo == nullptr
            ? new vk::ComputePipelineCreateInfo() : ComputePipelineCreateInfo;

        vk::PipelineLayoutCreateInfo PipelineLayoutCreateInfo;
        auto NativeArray = Shader->GetDescriptorSetLayouts();
        PipelineLayoutCreateInfo.setSetLayouts(NativeArray);
        auto PushConstantRanges = Shader->GetPushConstantRanges();
        PipelineLayoutCreateInfo.setPushConstantRanges(PushConstantRanges);

        FVulkanPipelineLayout PipelineLayout(Device, PipelineLayoutCreateInfo);
        ComputePipelineCreateInfo->setLayout(*PipelineLayout);
        ComputePipelineCreateInfo->setStage(Shader->CreateShaderStageCreateInfo().front());
        PipelineLayouts_.emplace(PipelineName, std::move(PipelineLayout));

        ComputePipelineCreateInfos_.emplace(PipelineName, *ComputePipelineCreateInfo);

        FVulkanPipeline Pipeline(Device, *ComputePipelineCreateInfo);
        Pipelines_.emplace(PipelineName, std::move(Pipeline));

        RegisterCallback(PipelineName, EPipelineType::kCompute);
    }

    void FPipelineManager::RemovePipeline(const std::string& Name)
    {
        Pipelines_.erase(Name);
    }

    void FPipelineManager::RegisterCallback(const std::string& Name, EPipelineType Type)
    {
        std::function<void()> CreatePipeline;
        std::function<void()> DestroyPipeline;

        if (Type == EPipelineType::kGraphics)
        {
            // CreatePipeline = [this, Name, VulkanContext]() -> void
            // {
            //     VulkanContext_->WaitIdle();
            //     vk::Device Device = VulkanContext_->GetDevice();
            // 
            //     auto& SwapchainExtent = VulkanContext_->GetSwapchainCreateInfo().imageExtent;
            //     auto& PipelineCreateInfoPack = GraphicsPipelineCreateInfoPacks_.at(Name);
            // 
            //     if (PipelineCreateInfoPack.DynamicStates.empty())
            //     {
            //         vk::Viewport Viewport(0.0f, static_cast<float>(SwapchainExtent.height),
            //                               static_cast<float>(SwapchainExtent.width), -static_cast<float>(SwapchainExtent.height),
            //                               0.0f, 1.0f);
            // 
            //         PipelineCreateInfoPack.Viewports.clear();
            //         PipelineCreateInfoPack.Viewports.push_back(Viewport);
            // 
            //         vk::Rect2D Scissor(vk::Offset2D(), SwapchainExtent);
            // 
            //         PipelineCreateInfoPack.Scissors.clear();
            //         PipelineCreateInfoPack.Scissors.push_back(Scissor);
            //     }
            // 
            //     PipelineCreateInfoPack.Update();
            // 
            //     auto it = Pipelines_.find(Name);
            //     Pipelines_.erase(it);
            //     Pipelines_.emplace(Name, std::move(FVulkanPipeline(Device, PipelineCreateInfoPack)));
            // };

            // DestroyPipeline = [this, Name]() -> void
            // {
            //     FVulkanPipeline Pipeline = std::move(Pipelines_.at(Name));
            // };

            CreatePipeline  = []() -> void {};
            DestroyPipeline = []() -> void {};
        }
        else
        {
            CreatePipeline  = []() -> void {};
            DestroyPipeline = []() -> void {};
        }

        VulkanContext_->RegisterAutoRemovedCallbacks(FVulkanContext::ECallbackType::kCreateSwapchain,  Name, CreatePipeline);
        VulkanContext_->RegisterAutoRemovedCallbacks(FVulkanContext::ECallbackType::kDestroySwapchain, Name, DestroyPipeline);
    }
} // namespace Npgs
