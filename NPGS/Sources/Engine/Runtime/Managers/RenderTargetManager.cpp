#include "stdafx.h"
#include "RenderTargetManager.hpp"

#include <format>
#include <stdexcept>
#include <utility>
#include <vector>

#include <vma/vk_mem_alloc.h>
#include "Engine/Core/Utils/VulkanUtils.hpp"

namespace Npgs
{
    vk::ImageView FManagedTarget::GetImageView() const
    {
        return *Attachment->GetImageView();
    }

    vk::ImageLayout FManagedTarget::GetImageLayout() const
    {
        return Description.ImageLayout;
    }

    FRenderTargetManager::FRenderTargetManager(FVulkanContext* VulkanContext)
        : VulkanContext_(VulkanContext)
        , bUnifiedImageLayouts_(VulkanContext_->CheckDeviceExtensionsSupported(vk::KHRUnifiedImageLayoutsExtensionName))
    {
        // Temp
        bUnifiedImageLayouts_ = false;
    }

    void FRenderTargetManager::DeclareAttachment(std::string_view Name, FRenderTargetDescription& Description)
    {
        if (bUnifiedImageLayouts_)
        {
            if (!Utils::IsSpecialLayout(Description.ImageLayout))
            {
                Description.ImageLayout = vk::ImageLayout::eGeneral;
            }
            if (!Utils::IsSpecialLayout(Description.ResolveImageLayout))
            {
                Description.ResolveImageLayout = vk::ImageLayout::eGeneral;
            }
        }

        vk::RenderingAttachmentInfo AttachmentInfo(
            {},
            Description.ImageLayout,
            Description.ResolveMode,
            {},
            Description.ResolveImageLayout,
            Description.LoadOp,
            Description.StoreOp,
            Description.ClearValue
        );

        FManagedTarget Target
        {
            .Description    = Description,
            .AttachmentInfo = AttachmentInfo,
            .Attachment     = nullptr
        };

        ManagedTargets_.try_emplace(std::string(Name), std::move(Target));
    }

    void FRenderTargetManager::CreateAttachments()
    {
        VmaAllocationCreateInfo AllocationCreateInfo
        {
            .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        };

        VulkanContext_->WaitIdle();
        auto Allocator = VulkanContext_->GetVmaAllocator();

        std::vector<std::pair<std::string, std::string>> NeedResolveAttachments;

        for (auto& [Name, ManagedTarget] : ManagedTargets_)
        {
            auto Type = ManagedTarget.Description.AttachmentType;
            if (Type == EAttachmentType::kColor)
            {
                ManagedTarget.Attachment = std::make_unique<FColorAttachment>(
                    VulkanContext_,
                    Name,
                    Allocator,
                    AllocationCreateInfo,
                    ManagedTarget.Description.ImageFormat,
                    ManagedTarget.Description.AttachmentExtent,
                    1,
                    ManagedTarget.Description.SampleCount,
                    ManagedTarget.Description.ImageUsage
                );
            }
            else
            {
                ManagedTarget.Attachment = std::make_unique<FDepthStencilAttachment>(
                    VulkanContext_,
                    Name,
                    Allocator,
                    AllocationCreateInfo,
                    ManagedTarget.Description.ImageFormat,
                    ManagedTarget.Description.AttachmentExtent,
                    1,
                    ManagedTarget.Description.SampleCount,
                    ManagedTarget.Description.ImageUsage,
                    Type == EAttachmentType::kStencilOnly ? true : false
                );
            }

            ManagedTarget.AttachmentInfo.setImageView(*ManagedTarget.Attachment->GetImageView());
            
            if (!ManagedTarget.Description.ResolveAttachmentName.empty())
            {
                NeedResolveAttachments.emplace_back(Name, ManagedTarget.Description.ResolveAttachmentName);
            }
        }

        for (auto& [Name, ResolveAttachmentName] : NeedResolveAttachments)
        {
            auto&       ManagedTarget    = ManagedTargets_.at(Name);
            const auto& ResolveImageView = ManagedTargets_.at(ResolveAttachmentName).Attachment->GetImageView();
            ManagedTarget.AttachmentInfo.setResolveImageView(*ResolveImageView);
        }
    }

    void FRenderTargetManager::DestroyAttachment(std::string_view Name)
    {
        VulkanContext_->WaitIdle();
        ManagedTargets_.erase(Name);
    }

    void FRenderTargetManager::DesctoyAttachments()
    {
        VulkanContext_->WaitIdle();
    }

    const FManagedTarget& FRenderTargetManager::GetManagedTarget(std::string_view Name) const
    {
        auto it = ManagedTargets_.find(Name);
        if (it == ManagedTargets_.end())
        {
            throw std::out_of_range(std::format(R"(Managed target for "{}" not found.)", Name));
        }

        return it->second;
    }
} // namespace Npgs
