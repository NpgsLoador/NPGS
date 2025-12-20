#pragma once

#include <memory>
#include <string>
#include <string_view>

#include <vulkan/vulkan.hpp>

#include "Engine/Core/Utils/Hash.hpp"
#include "Engine/Runtime/Graphics/Resources/Attachment.hpp"
#include "Engine/Runtime/Graphics/Vulkan/Context.hpp"
#include "Engine/Runtime/Managers/ImageTracker.hpp"

namespace Npgs
{
    enum class EAttachmentType
    {
        kColor,
        kDepthStencil,
        kStencilOnly
    };

    struct FRenderTargetDescription
    {
        std::string             ResolveAttachmentName;
        EAttachmentType         AttachmentType{ EAttachmentType::kColor };
        vk::Extent2D            AttachmentExtent;
        vk::Format              ImageFormat{ vk::Format::eUndefined };
        vk::Format              ResolveImageFormat{ vk::Format::eUndefined };
        vk::SampleCountFlagBits SampleCount{ vk::SampleCountFlagBits::e1 };
        vk::ImageLayout         ImageLayout{ vk::ImageLayout::eUndefined };
        vk::ImageUsageFlags     ImageUsage{ static_cast<vk::ImageUsageFlagBits>(0) };
        vk::ResolveModeFlagBits ResolveMode{ vk::ResolveModeFlagBits::eNone };
        vk::ImageLayout         ResolveImageLayout{ vk::ImageLayout::eUndefined };
        vk::AttachmentLoadOp    LoadOp{ vk::AttachmentLoadOp::eLoad };
        vk::AttachmentStoreOp   StoreOp{ vk::AttachmentStoreOp::eStore };
        vk::ClearValue          ClearValue{};
    };

    struct FManagedTarget
    {
        FRenderTargetDescription     Description;
        vk::RenderingAttachmentInfo  AttachmentInfo;
        std::unique_ptr<FAttachment> Attachment;

        vk::ImageView GetImageView() const;
        vk::ImageLayout GetImageLayout() const;
    };

    class FRenderTargetManager
    {
    public:
        FRenderTargetManager(FVulkanContext* VulkanContext);
        FRenderTargetManager(const FRenderTargetManager&) = delete;
        FRenderTargetManager(FRenderTargetManager&&)      = delete;
        ~FRenderTargetManager()                           = default;

        FRenderTargetManager& operator=(const FRenderTargetManager&) = delete;
        FRenderTargetManager& operator=(FRenderTargetManager&&)      = delete;

        void DeclareAttachment(std::string_view Name, FRenderTargetDescription& Description);
        void CreateAttachments();
        void DestroyAttachment(std::string_view Name);
        void DesctoyAttachments();

        const FManagedTarget& GetManagedTarget(std::string_view Name) const;

    private:
        using FManagedTargetMap = Utils::TStringHeteroHashTable<std::string, FManagedTarget>;

    private:
        FVulkanContext*   VulkanContext_;
        FManagedTargetMap ManagedTargets_;
        bool              bUnifiedImageLayouts_;
    };
} // namespace Npgs
