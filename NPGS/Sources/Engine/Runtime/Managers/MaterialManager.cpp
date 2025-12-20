#include "stdafx.h"
#include "MaterialManager.hpp"

#include "Engine/Runtime/AssetLoaders/Shader.hpp"
#include "Engine/Runtime/AssetLoaders/Texture.hpp"
#include "Engine/Runtime/Managers/ShaderBufferManager.hpp"
#include "Engine/System/Services/EngineServices.hpp"

namespace Npgs
{
    void FMaterialManager::BakeMaterialDescriptors(const FDescriptorBindInfo& DescriptorBindInfo)
    {
        //auto* AssetManager        = EngineCoreServices->GetAssetManager();
        //auto* RenderTargetManager = EngineResourceServices->GetRenderTargetManager();
        //auto  Shader              = AssetManager->AcquireAsset<FShader>(DescriptorBindInfo.ShaderName);

        //FDescriptorBufferCreateInfo DescriptorBufferCreateInfo;
        //DescriptorBufferCreateInfo.Name               = DescriptorBindInfo.DescriptorBufferName;
        //DescriptorBufferCreateInfo.SetInfos           = Shader->GetDescriptorSetInfos();
        //DescriptorBufferCreateInfo.UniformBufferNames = DescriptorBindInfo.UniformBufferNames;
        //DescriptorBufferCreateInfo.StorageBufferNames = DescriptorBindInfo.StorageBufferNames;

        //for (const auto& [Name, Set, Binding] : DescriptorBindInfo.SamplerInfos)
        //{
        //    auto Sampler = AssetManager->AcquireAsset<FVulkanSampler>(Name);
        //    DescriptorBufferCreateInfo.SamplerInfos.emplace_back(Set, Binding, **Sampler);
        //}

        //for (const auto& [Type, Usage, ImageName, SamplerName, Set, Binding] : DescriptorBindInfo.ImageInfos)
        //{
        //    vk::DescriptorImageInfo ImageInfo;
        //    auto Sampler = SamplerName.empty() ? TAssetHandle<FVulkanSampler>()
        //                                       : AssetManager->AcquireAsset<FVulkanSampler>(SamplerName);

        //    if (Type == FImageInfoCreateInfo::EImageType::kAttachment)
        //    {
        //        const auto& ManagedTarget = RenderTargetManager->GetManagedTarget(ImageName);
        //        ImageInfo = vk::DescriptorImageInfo(
        //            Sampler ? **Sampler : vk::Sampler(), ManagedTarget.GetImageView(), ManagedTarget.GetImageLayout());
        //    }
        //    else
        //    {
        //        auto Texture = AssetManager->AcquireAsset<FTexture2D>(ImageName);
        //        ImageInfo = Texture->CreateDescriptorImageInfo(Sampler ? **Sampler : vk::Sampler());
        //    }

        //    if (ImageInfo.sampler)
        //    {
        //        DescriptorBufferCreateInfo.CombinedImageSamplerInfos.emplace_back(Set, Binding, ImageInfo);
        //    }
        //    else
        //    {
        //        if (Usage == vk::DescriptorType::eSampledImage)
        //        {
        //            DescriptorBufferCreateInfo.SampledImageInfos.emplace_back(Set, Binding, ImageInfo);
        //        }
        //        else if (Usage == vk::DescriptorType::eStorageImage)
        //        {
        //            DescriptorBufferCreateInfo.StorageImageInfos.emplace_back(Set, Binding, ImageInfo);
        //        }
        //    }
        //}

        //auto* ShaderBufferManager = EngineResourceServices->GetShaderBufferManager();
        //ShaderBufferManager->AllocateDescriptorBuffer(DescriptorBufferCreateInfo);
    }
} // namespace Npgs
