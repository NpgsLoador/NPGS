#include "stdafx.h"
#include "MaterialManager.hpp"

#include <vma/vk_mem_alloc.h>

#include "Engine/Core/Runtime/AssetLoaders/Shader.hpp"
#include "Engine/Core/Runtime/AssetLoaders/Texture.hpp"
#include "Engine/Core/Runtime/Managers/ShaderBufferManager.hpp"
#include "Engine/Core/System/Services/EngineServices.hpp"

namespace Npgs
{
    void FMaterialManager::BakeMaterialDescriptors(const FDescriptorBindInfo& DescriptorBindInfo)
    {
        auto* AssetManager        = EngineCoreServices->GetAssetManager();
        auto* RenderTargetManager = EngineResourceServices->GetRenderTargetManager();
        auto* Shader              = AssetManager->GetAsset<FShader>(DescriptorBindInfo.ShaderName);

        FDescriptorBufferCreateInfo DescriptorBufferCreateInfo;
        DescriptorBufferCreateInfo.Name               = DescriptorBindInfo.DescriptorBufferName;
        DescriptorBufferCreateInfo.SetInfos           = Shader->GetDescriptorSetInfos();
        DescriptorBufferCreateInfo.UniformBufferNames = DescriptorBindInfo.UniformBufferNames;
        DescriptorBufferCreateInfo.StorageBufferNames = DescriptorBindInfo.StorageBufferNames;

        for (const auto& [Name, Set, Binding] : DescriptorBindInfo.SamplerNames)
        {
            auto* Sampler = AssetManager->GetAsset<FVulkanSampler>(Name);
            DescriptorBufferCreateInfo.SamplerInfos.emplace_back(Set, Binding, **Sampler);
        }

        for (const auto& [Type, Usage, Name, Sampler, Set, Binding] : DescriptorBindInfo.ImageInfos)
        {
            vk::DescriptorImageInfo ImageInfo;
            auto* Sampler = Name.empty() ? nullptr : AssetManager->GetAsset<FVulkanSampler>(Name);
            if (Type == FImageInfoCreateInfo::EImageType::kAttachment)
            {
                const auto& ManagedTarget = RenderTargetManager->GetManagedTarget(Name);
                ImageInfo = vk::DescriptorImageInfo(
                    Sampler ? **Sampler : vk::Sampler(), ManagedTarget.GetImageView(), ManagedTarget.GetImageLayout());
            }
            else
            {
                auto* Texture = AssetManager->GetAsset<FTexture2D>(Name);
                ImageInfo = Texture->CreateDescriptorImageInfo(Sampler ? **Sampler : vk::Sampler());
            }

            if (ImageInfo.sampler)
            {
                DescriptorBufferCreateInfo.CombinedImageSamplerInfos.emplace_back(Set, Binding, ImageInfo);
            }
            else
            {
                if (Usage == vk::DescriptorType::eSampledImage)
                {
                    DescriptorBufferCreateInfo.SampledImageInfos.emplace_back(Set, Binding, ImageInfo);
                }
                else if (Usage == vk::DescriptorType::eStorageImage)
                {
                    DescriptorBufferCreateInfo.StorageImageInfos.emplace_back(Set, Binding, ImageInfo);
                }
            }
        }

        VmaAllocationCreateInfo AllocationCreateInfo
        {
            .flags         = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage         = VMA_MEMORY_USAGE_CPU_TO_GPU,
            .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        };

        auto* ShaderBufferManager = EngineResourceServices->GetShaderBufferManager();
        ShaderBufferManager->CreateDescriptorBuffer(DescriptorBufferCreateInfo, AllocationCreateInfo);
    }
} // namespace Npgs
