#include "stdafx.h"
#include "StandardPbrMaterial.hpp"

#include <cstddef>
#include <future>
#include <string>
#include <vector>

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "Engine/Runtime/AssetLoaders/Shader.hpp"
#include "Engine/Runtime/Graphics/Vulkan/Wrappers.hpp"
#include "Engine/Runtime/Managers/ShaderBufferManager.hpp"
#include "Engine/System/Services/EngineServices.hpp"
#include "Program/Rendering/NameLookup.hpp"

namespace Npgs
{
    void FStandardPbrMaterial::LoadAssets()
    {
        std::vector<std::string> TextureNames
        {
            Materials::StandardPbr::kAlbedoName,
            Materials::StandardPbr::kNormalName,
            Materials::StandardPbr::kArmName
        };

        std::vector<std::string> TextureFiles
        {
            "CliffSide/cliff_side_diff_4k_mipmapped_bc6h_u.ktx2",
            "CliffSide/cliff_side_nor_dx_4k_mipmapped_bc6h_u.ktx2",
            "CliffSide/cliff_side_arm_4k_mipmapped_bc6h_u.ktx2"
        };

        std::vector<vk::Format> InitialTextureFormats
        {
            vk::Format::eBc6HUfloatBlock,
            vk::Format::eBc6HUfloatBlock,
            vk::Format::eBc6HUfloatBlock
        };

        std::vector<vk::Format> FinalTextureFormats
        {
            vk::Format::eBc6HUfloatBlock,
            vk::Format::eBc6HUfloatBlock,
            vk::Format::eBc6HUfloatBlock
        };

        auto Allocator = VulkanContext_->GetVmaAllocator();

        VmaAllocationCreateInfo TextureAllocationCreateInfo
        {
            .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            .usage = VMA_MEMORY_USAGE_GPU_ONLY
        };

        auto* AssetManager = EngineCoreServices->GetAssetManager();
        std::vector<std::future<void>> Futures;

        for (std::size_t i = 0; i != TextureNames.size(); ++i)
        {
            Futures.push_back(ThreadPool_->Submit([&, i]() -> void
            {
                AssetManager->AddAsset<FTexture2D>(TextureNames[i], Allocator, TextureAllocationCreateInfo,
                                                   TextureFiles[i], InitialTextureFormats[i], FinalTextureFormats[i],
                                                   vk::ImageCreateFlagBits::eMutableFormat, true);
            }));
        }

        for (auto& Future : Futures)
        {
            Future.get();
        }

        AlbedoMap_ = AssetManager->AcquireAsset<FTexture2D>(Materials::StandardPbr::kAlbedoName);
        NormalMap_ = AssetManager->AcquireAsset<FTexture2D>(Materials::StandardPbr::kNormalName);
        ArmMap_    = AssetManager->AcquireAsset<FTexture2D>(Materials::StandardPbr::kArmName);
    }

    void FStandardPbrMaterial::BindDescriptors()
    {
        auto* AssetManager = EngineCoreServices->GetAssetManager();
        auto  Shader       = AssetManager->AcquireAsset<FShader>(RenderPasses::GbufferScene::kShaderName);

        FDescriptorBufferCreateInfo DescriptorBufferCreateInfo;
        DescriptorBufferCreateInfo.Name     = Materials::StandardPbr::kDescriptorBufferName;
        DescriptorBufferCreateInfo.SetInfos = Shader->GetDescriptorSetInfos();

        auto Sampler = AssetManager->AcquireAsset<FVulkanSampler>(Public::Samplers::kPbrTextureSamplerName);
        DescriptorBufferCreateInfo.SamplerInfos.emplace_back(0u, 0u, **Sampler);
        auto PbrAlbedoImageInfo = AlbedoMap_->CreateDescriptorImageInfo(nullptr);
        DescriptorBufferCreateInfo.SampledImageInfos.emplace_back(1u, 0u, PbrAlbedoImageInfo);
        auto PbrNormalImageInfo = NormalMap_->CreateDescriptorImageInfo(nullptr);
        DescriptorBufferCreateInfo.SampledImageInfos.emplace_back(1u, 1u, PbrNormalImageInfo);
        auto PbrArmImageInfo = ArmMap_->CreateDescriptorImageInfo(nullptr);
        DescriptorBufferCreateInfo.SampledImageInfos.emplace_back(1u, 2u, PbrArmImageInfo);

        auto        FramebufferSampler  = AssetManager->AcquireAsset<FVulkanSampler>(Public::Samplers::kFramebufferSamplerName);
        auto*       RenderTargetManager = EngineResourceServices->GetRenderTargetManager();
        const auto& DepthMapAttachment  = RenderTargetManager->GetManagedTarget(Public::Attachments::kDepthMapAttachmentName);
        
        vk::DescriptorImageInfo DepthMapImageInfo(
            **FramebufferSampler, DepthMapAttachment.GetImageView(), DepthMapAttachment.GetImageLayout());
        DescriptorBufferCreateInfo.CombinedImageSamplerInfos.emplace_back(2u, 0u, DepthMapImageInfo);

        auto* ShaderBufferManager = EngineResourceServices->GetShaderBufferManager();
        ShaderBufferManager->AllocateDescriptorBuffer(DescriptorBufferCreateInfo);
    }
} // namespace Npgs
