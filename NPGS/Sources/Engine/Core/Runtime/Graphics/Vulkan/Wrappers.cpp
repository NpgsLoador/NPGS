#include "Wrappers.h"

#include <algorithm>
#include <limits>
#include <utility>

#include <vulkan/vulkan_format_traits.hpp>

#include "Engine/Core/Base/Assert.h"
#include "Engine/Core/Base/Base.h"
#include "Engine/Utils/Logger.h"
#include "Engine/Utils/VulkanCheck.h"

namespace Npgs
{
    namespace
    {
        std::uint32_t GetMemoryTypeIndex(const vk::PhysicalDeviceMemoryProperties& PhysicalDeviceMemoryProperties,
                                         const vk::MemoryRequirements& MemoryRequirements, vk::MemoryPropertyFlags MemoryPropertyFlags)
        {
            for (std::size_t i = 0; i != PhysicalDeviceMemoryProperties.memoryTypeCount; ++i)
            {
                if (MemoryRequirements.memoryTypeBits & static_cast<std::uint32_t>(Bit(i)) &&
                    (PhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & MemoryPropertyFlags) == MemoryPropertyFlags)
                {
                    return static_cast<std::uint32_t>(i);
                }
            }

            return std::numeric_limits<std::uint32_t>::max();
        }
    }

    FGraphicsPipelineCreateInfoPack::FGraphicsPipelineCreateInfoPack()
    {
        LinkToGraphicsPipelineCreateInfo();
        GraphicsPipelineCreateInfo.setBasePipelineIndex(-1);
    }

    FGraphicsPipelineCreateInfoPack::FGraphicsPipelineCreateInfoPack(FGraphicsPipelineCreateInfoPack&& Other) noexcept
        : GraphicsPipelineCreateInfo(std::exchange(Other.GraphicsPipelineCreateInfo, {}))
        , VertexInputStateCreateInfo(std::exchange(Other.VertexInputStateCreateInfo, {}))
        , InputAssemblyStateCreateInfo(std::exchange(Other.InputAssemblyStateCreateInfo, {}))
        , TessellationStateCreateInfo(std::exchange(Other.TessellationStateCreateInfo, {}))
        , ViewportStateCreateInfo(std::exchange(Other.ViewportStateCreateInfo, {}))
        , RasterizationStateCreateInfo(std::exchange(Other.RasterizationStateCreateInfo, {}))
        , MultisampleStateCreateInfo(std::exchange(Other.MultisampleStateCreateInfo, {}))
        , DepthStencilStateCreateInfo(std::exchange(Other.DepthStencilStateCreateInfo, {}))
        , ColorBlendStateCreateInfo(std::exchange(Other.ColorBlendStateCreateInfo, {}))
        , DynamicStateCreateInfo(std::exchange(Other.DynamicStateCreateInfo, {}))

        , ShaderStages(std::move(Other.ShaderStages))
        , VertexInputBindings(std::move(Other.VertexInputBindings))
        , VertexInputAttributes(std::move(Other.VertexInputAttributes))
        , Viewports(std::move(Other.Viewports))
        , Scissors(std::move(Other.Scissors))
        , ColorBlendAttachmentStates(std::move(Other.ColorBlendAttachmentStates))
        , DynamicStates(std::move(Other.DynamicStates))

        , DynamicViewportCount(std::exchange(Other.DynamicViewportCount, 1))
        , DynamicScissorCount(std::exchange(Other.DynamicScissorCount, 1))
    {
        LinkToGraphicsPipelineCreateInfo();
        UpdateAllInfoData();
    }

    FGraphicsPipelineCreateInfoPack& FGraphicsPipelineCreateInfoPack::operator=(FGraphicsPipelineCreateInfoPack&& Other) noexcept
    {
        if (this != &Other)
        {
            GraphicsPipelineCreateInfo   = std::exchange(Other.GraphicsPipelineCreateInfo, {});

            VertexInputStateCreateInfo   = std::exchange(Other.VertexInputStateCreateInfo, {});
            InputAssemblyStateCreateInfo = std::exchange(Other.InputAssemblyStateCreateInfo, {});
            TessellationStateCreateInfo  = std::exchange(Other.TessellationStateCreateInfo, {});
            ViewportStateCreateInfo      = std::exchange(Other.ViewportStateCreateInfo, {});
            RasterizationStateCreateInfo = std::exchange(Other.RasterizationStateCreateInfo, {});
            MultisampleStateCreateInfo   = std::exchange(Other.MultisampleStateCreateInfo, {});
            DepthStencilStateCreateInfo  = std::exchange(Other.DepthStencilStateCreateInfo, {});
            ColorBlendStateCreateInfo    = std::exchange(Other.ColorBlendStateCreateInfo, {});
            DynamicStateCreateInfo       = std::exchange(Other.DynamicStateCreateInfo, {});

            ShaderStages                 = std::move(Other.ShaderStages);
            VertexInputBindings          = std::move(Other.VertexInputBindings);
            VertexInputAttributes        = std::move(Other.VertexInputAttributes);
            Viewports                    = std::move(Other.Viewports);
            Scissors                     = std::move(Other.Scissors);
            ColorBlendAttachmentStates   = std::move(Other.ColorBlendAttachmentStates);
            DynamicStates                = std::move(Other.DynamicStates);

            DynamicViewportCount         = std::exchange(Other.DynamicViewportCount, 1);
            DynamicScissorCount          = std::exchange(Other.DynamicScissorCount, 1);

            LinkToGraphicsPipelineCreateInfo();
            UpdateAllInfoData();
        }

        return *this;
    }

    void FGraphicsPipelineCreateInfoPack::Update()
    {
        ViewportStateCreateInfo.setViewportCount(
            Viewports.size() ? static_cast<std::uint32_t>(Viewports.size()) : DynamicViewportCount);
        ViewportStateCreateInfo.setScissorCount(
            Scissors.size() ? static_cast<std::uint32_t>(Scissors.size()) : DynamicScissorCount);

        UpdateAllInfoData();
    }

    void FGraphicsPipelineCreateInfoPack::LinkToGraphicsPipelineCreateInfo()
    {
        GraphicsPipelineCreateInfo.setPVertexInputState(&VertexInputStateCreateInfo)
                                  .setPInputAssemblyState(&InputAssemblyStateCreateInfo)
                                  .setPTessellationState(&TessellationStateCreateInfo)
                                  .setPViewportState(&ViewportStateCreateInfo)
                                  .setPRasterizationState(&RasterizationStateCreateInfo)
                                  .setPMultisampleState(&MultisampleStateCreateInfo)
                                  .setPDepthStencilState(&DepthStencilStateCreateInfo)
                                  .setPColorBlendState(&ColorBlendStateCreateInfo)
                                  .setPDynamicState(&DynamicStateCreateInfo);
    }

    void FGraphicsPipelineCreateInfoPack::UpdateAllInfoData()
    {
        if (Viewports.empty())
        {
            ViewportStateCreateInfo.setPViewports(nullptr);
        }
        else
        {
            ViewportStateCreateInfo.setViewports(Viewports);
        }

        if (Scissors.empty())
        {
            ViewportStateCreateInfo.setPScissors(nullptr);
        }
        else
        {
            ViewportStateCreateInfo.setScissors(Scissors);
        }

        GraphicsPipelineCreateInfo.setStages(ShaderStages);
        VertexInputStateCreateInfo.setVertexBindingDescriptions(VertexInputBindings);
        VertexInputStateCreateInfo.setVertexAttributeDescriptions(VertexInputAttributes);
        ColorBlendStateCreateInfo.setAttachments(ColorBlendAttachmentStates);
        DynamicStateCreateInfo.setDynamicStates(DynamicStates);

        LinkToGraphicsPipelineCreateInfo();
    }

    FFormatInfo::FFormatInfo(vk::Format Format)
    {
        ComponentCount = vk::componentCount(Format);
        ComponentSize  = vk::componentBits(Format, 0) / 8;
        PixelSize      = vk::blockSize(Format);
        bIsCompressed  = vk::componentsAreCompressed(Format);

        if (Format == vk::Format::eD16UnormS8Uint)
        {
            PixelSize = 4;
        }
        else if (Format == vk::Format::eD32SfloatS8Uint)
        {
            PixelSize = 8;
        }

        if (Format == vk::Format::eUndefined)
        {
            RawDataType = ERawDataType::kOther;
        }
        else
        {
            const char* NumericFormat = vk::componentNumericFormat(Format, 0);
            if (Util::Equal(NumericFormat, "SINT")    || Util::Equal(NumericFormat, "UINT")  ||
                Util::Equal(NumericFormat, "SNORM")   || Util::Equal(NumericFormat, "UNORM") ||
                Util::Equal(NumericFormat, "SSCALED") || Util::Equal(NumericFormat, "USCALED"))
            {
                RawDataType = ERawDataType::kInteger;
            }
            else if (Util::Equal(NumericFormat, "SFLOAT") || Util::Equal(NumericFormat, "UFLOAT"))
            {
                RawDataType = ERawDataType::kFloatingPoint;
            }
            else
            {
                RawDataType = ERawDataType::kInteger;
            }
        }
    }

    FFormatInfo GetFormatInfo(vk::Format Format)
    {
        return FFormatInfo(Format);
    }

    vk::Format ConvertToFloat16(vk::Format Float32Format)
    {
        switch (Float32Format)
        {
        case vk::Format::eR32Sfloat:
            return vk::Format::eR16Sfloat;
        case vk::Format::eR32G32Sfloat:
            return vk::Format::eR16G16Sfloat;
        case vk::Format::eR32G32B32Sfloat:
            return vk::Format::eR16G16B16Sfloat;
        case vk::Format::eR32G32B32A32Sfloat:
            return vk::Format::eR16G16B16A16Sfloat;
        default:
            return vk::Format::eUndefined;
        }
    }

    // Wrapper for vk::CommandBuffer
    // -----------------------------
    vk::Result FVulkanCommandBuffer::Begin(const vk::CommandBufferInheritanceInfo& InheritanceInfo,
                                           vk::CommandBufferUsageFlags Flags) const
    {
        vk::CommandBufferBeginInfo CommandBufferBeginInfo(Flags, &InheritanceInfo);
        try
        {
            Handle_.begin(CommandBufferBeginInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to begin command buffer: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCommandBuffer::Begin(vk::CommandBufferUsageFlags Flags) const
    {
        vk::CommandBufferBeginInfo CommandBufferBeginInfo(Flags);
        try
        {
            Handle_.begin(CommandBufferBeginInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to begin command buffer: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCommandBuffer::End() const
    {
        try
        {
            Handle_.end();
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to end command buffer: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        return vk::Result::eSuccess;
    }

    // Wrapper for vk::CommandPool
    // ---------------------------
    FVulkanCommandPool::FVulkanCommandPool(vk::Device Device, const vk::CommandPoolCreateInfo& CreateInfo)
        : Base(Device)
    {
        CreateCommandPool(CreateInfo);
    }

    FVulkanCommandPool::FVulkanCommandPool(vk::Device Device, std::uint32_t QueueFamilyIndex, vk::CommandPoolCreateFlags Flags)
        : Base(Device)
    {
        ReleaseInfo_ = "Command pool destroyed successfully.";
        Status_      = CreateCommandPool(QueueFamilyIndex, Flags);
    }

    vk::Result FVulkanCommandPool::AllocateBuffer(vk::CommandBufferLevel Level, vk::CommandBuffer& Buffer) const
    {
        vk::CommandBufferAllocateInfo CommandBufferAllocateInfo(Handle_, Level, 1);
        try
        {
            Buffer = Device_.allocateCommandBuffers(CommandBufferAllocateInfo)[0];
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to allocate command buffer: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        NpgsCoreTrace("Command buffer allocated successfully.");
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCommandPool::AllocateBuffer(vk::CommandBufferLevel Level, FVulkanCommandBuffer& Buffer) const
    {
        vk::CommandBufferAllocateInfo CommandBufferAllocateInfo(Handle_, Level, 1);
        try
        {
            *Buffer = Device_.allocateCommandBuffers(CommandBufferAllocateInfo)[0];
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to allocate command buffer: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        NpgsCoreTrace("Command buffer allocated successfully.");
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCommandPool::AllocateBuffers(vk::CommandBufferLevel Level, std::vector<vk::CommandBuffer>& Buffers) const
    {
        vk::CommandBufferAllocateInfo CommandBufferAllocateInfo(Handle_, Level, static_cast<std::uint32_t>(Buffers.size()));
        try
        {
            Buffers = Device_.allocateCommandBuffers(CommandBufferAllocateInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to allocate command buffers: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        NpgsCoreTrace("Command buffers allocated successfully.");
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCommandPool::AllocateBuffers(vk::CommandBufferLevel Level, std::vector<FVulkanCommandBuffer>& Buffers) const
    {
        vk::CommandBufferAllocateInfo CommandBufferAllocateInfo(Handle_, Level, static_cast<std::uint32_t>(Buffers.size()));
        std::vector<vk::CommandBuffer> CommandBuffers;
        CommandBuffers.reserve(Buffers.size());
        try
        {
            CommandBuffers = Device_.allocateCommandBuffers(CommandBufferAllocateInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to allocate command buffers: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        Buffers.resize(CommandBuffers.size());
        for (std::size_t i = 0; i != CommandBuffers.size(); ++i)
        {
            *Buffers[i] = CommandBuffers[i];
        }

        NpgsCoreTrace("Command buffers allocated successfully.");
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCommandPool::FreeBuffer(vk::CommandBuffer& Buffer) const
    {
        Device_.freeCommandBuffers(Handle_, Buffer);
        Buffer = vk::CommandBuffer();
        NpgsCoreTrace("Command buffer freed successfully.");
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCommandPool::FreeBuffer(FVulkanCommandBuffer& Buffer) const
    {
        return FreeBuffer(*Buffer);
    }

    vk::Result FVulkanCommandPool::FreeBuffers(const vk::ArrayProxy<vk::CommandBuffer>& Buffers) const
    {
        Device_.freeCommandBuffers(Handle_, Buffers);
        NpgsCoreTrace("Command buffers freed successfully.");
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCommandPool::FreeBuffers(const vk::ArrayProxy<FVulkanCommandBuffer>& Buffers) const
    {
        std::vector<vk::CommandBuffer> CommandBuffers;
        CommandBuffers.reserve(Buffers.size());

        for (auto& Buffer : Buffers)
        {
            CommandBuffers.push_back(*Buffer);
        }

        return FreeBuffers(CommandBuffers);
    }

    vk::Result FVulkanCommandPool::CreateCommandPool(const vk::CommandPoolCreateInfo& CreateInfo)
    {
        try
        {
            Handle_ = Device_.createCommandPool(CreateInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to create command pool: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        NpgsCoreTrace("Command pool created successfully.");
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCommandPool::CreateCommandPool(std::uint32_t QueueFamilyIndex, vk::CommandPoolCreateFlags Flags)
    {
        vk::CommandPoolCreateInfo CreateInfo(Flags, QueueFamilyIndex);
        return CreateCommandPool(CreateInfo);
    }

    // Wrapper for vk::DeviceMemory
    // ----------------------------
    FVulkanDeviceMemory::FVulkanDeviceMemory(vk::Device Device, const vk::PhysicalDeviceProperties& PhysicalDeviceProperties,
                                             const vk::PhysicalDeviceMemoryProperties& PhysicalDeviceMemoryProperties,
                                             const vk::MemoryAllocateInfo& AllocateInfo)
        : Base(Device)
        , Allocator_(nullptr)
        , Allocation_(nullptr)
        , AllocationInfo_{}
        , PhysicalDeviceProperties_(PhysicalDeviceProperties)
        , PhysicalDeviceMemoryProperties_(PhysicalDeviceMemoryProperties)
        , AllocationSize_{}
        , bHostingVma_(false)
    {
        ReleaseInfo_ = "Device memory freed successfully.";
        Status_      = AllocateDeviceMemory(AllocateInfo);
    }

    FVulkanDeviceMemory::FVulkanDeviceMemory(vk::Device Device, VmaAllocator Allocator,
                                             const VmaAllocationCreateInfo& AllocationCreateInfo,
                                             const vk::PhysicalDeviceProperties& PhysicalDeviceProperties,
                                             const vk::PhysicalDeviceMemoryProperties& PhysicalDeviceMemoryProperties,
                                             const vk::MemoryRequirements& MemoryRequirements)
        : Base(Device)
        , Allocator_(Allocator)
        , Allocation_(nullptr)
        , AllocationInfo_{}
        , PhysicalDeviceProperties_(PhysicalDeviceProperties)
        , PhysicalDeviceMemoryProperties_(PhysicalDeviceMemoryProperties)
        , AllocationSize_{}
        , bHostingVma_(false)
    {
        ReleaseInfo_ = "Device memory freed successfully.";
        Status_      = AllocateDeviceMemory(AllocationCreateInfo, MemoryRequirements);
    }

    FVulkanDeviceMemory::FVulkanDeviceMemory(vk::Device Device, VmaAllocator Allocator, VmaAllocation Allocation,
                                             const VmaAllocationInfo& AllocationInfo, vk::DeviceMemory Handle)
        : Base(Device)
        , Allocator_(Allocator)
        , Allocation_(Allocation)
        , AllocationInfo_(AllocationInfo)
        , PhysicalDeviceProperties_{}
        , PhysicalDeviceMemoryProperties_{}
        , AllocationSize_(AllocationInfo.size)
        , bHostingVma_(true)
    {
        ReleaseInfo_ = "Device memory freed successfully.";
        Handle_      = Handle;
        Status_      = vk::Result::eSuccess;

        vmaGetMemoryTypeProperties(Allocator_, AllocationInfo_.memoryType,
                                   reinterpret_cast<VkMemoryPropertyFlags*>(&MemoryPropertyFlags_));
    }

    FVulkanDeviceMemory::FVulkanDeviceMemory(FVulkanDeviceMemory&& Other) noexcept
        : Base(std::move(Other))
        , Allocator_(std::exchange(Other.Allocator_, nullptr))
        , Allocation_(std::exchange(Other.Allocation_, nullptr))
        , AllocationInfo_(std::exchange(Other.AllocationInfo_, {}))
        , PhysicalDeviceProperties_(std::exchange(Other.PhysicalDeviceProperties_, {}))
        , PhysicalDeviceMemoryProperties_(std::exchange(Other.PhysicalDeviceMemoryProperties_, {}))
        , AllocationSize_(std::exchange(Other.AllocationSize_, 0))
        , MemoryPropertyFlags_(std::exchange(Other.MemoryPropertyFlags_, {}))
        , MappedDataMemory_(std::exchange(Other.MappedDataMemory_, nullptr))
        , MappedTargetMemory_(std::exchange(Other.MappedTargetMemory_, nullptr))
        , bPersistentlyMapped_(std::exchange(Other.bPersistentlyMapped_, false))
        , bHostingVma_(std::exchange(Other.bHostingVma_, false))
    {
    }

    FVulkanDeviceMemory::~FVulkanDeviceMemory()
    {
        if (bHostingVma_)
        {
            if (bPersistentlyMapped_ && AllocationInfo_.pMappedData != nullptr)
            {
                vmaUnmapMemory(Allocator_, Allocation_);
            }

            Handle_              = vk::DeviceMemory();
            MappedDataMemory_    = nullptr;
            MappedTargetMemory_  = nullptr;
            bPersistentlyMapped_ = false;

            return;
        }

        if (bPersistentlyMapped_ && (MappedDataMemory_ != nullptr || MappedTargetMemory_ != nullptr))
        {
            if (Allocator_ != nullptr && Allocation_ != nullptr)
            {
                vmaUnmapMemory(Allocator_, Allocation_);
            }
            else
            {
                UnmapMemory(0, AllocationSize_);
            }

            MappedDataMemory_    = nullptr;
            MappedTargetMemory_  = nullptr;
            bPersistentlyMapped_ = false;
        }

        if (Allocator_ != nullptr && Allocation_ != nullptr)
        {
            if (Handle_ == AllocationInfo_.deviceMemory)
            {
                auto it = HandleTracker_.find(Handle_);
                if (it != HandleTracker_.end())
                {
                    if (--it->second == 0)
                    {
                        HandleTracker_.erase(it);
                    }
                    else
                    {
                        Handle_ = vk::DeviceMemory();
                    }
                }
            }

            vmaFreeMemory(Allocator_, Allocation_);
        }
    }

    FVulkanDeviceMemory& FVulkanDeviceMemory::operator=(FVulkanDeviceMemory&& Other) noexcept
    {
        if (this != &Other)
        {
            Base::operator=(std::move(Other));

            Allocator_                      = std::exchange(Other.Allocator_, nullptr);
            Allocation_                     = std::exchange(Other.Allocation_, nullptr);
            AllocationInfo_                 = std::exchange(Other.AllocationInfo_, {});
            PhysicalDeviceProperties_       = std::exchange(Other.PhysicalDeviceProperties_, {});
            PhysicalDeviceMemoryProperties_ = std::exchange(Other.PhysicalDeviceMemoryProperties_, {});
            AllocationSize_                 = std::exchange(Other.AllocationSize_, 0);
            MemoryPropertyFlags_            = std::exchange(Other.MemoryPropertyFlags_, {});
            MappedDataMemory_               = std::exchange(Other.MappedDataMemory_, nullptr);
            MappedTargetMemory_             = std::exchange(Other.MappedTargetMemory_, nullptr);
            bPersistentlyMapped_            = std::exchange(Other.bPersistentlyMapped_, false);
            bHostingVma_                    = std::exchange(Other.bHostingVma_, false);
        }

        return *this;
    }

    vk::Result FVulkanDeviceMemory::MapMemoryForSubmit(vk::DeviceSize Offset, vk::DeviceSize Size, void*& Target)
    {
        if (Allocator_ != nullptr && Allocation_ != nullptr)
        {
            VulkanCheck(vmaMapMemory(Allocator_, Allocation_, &Target));
            vmaGetAllocationInfo(Allocator_, Allocation_, &AllocationInfo_);
            if (Offset > 0)
            {
                Target = static_cast<std::byte*>(Target) + Offset;
            }

            MappedTargetMemory_ = Target;
            return vk::Result::eSuccess;
        }

        vk::DeviceSize AdjustedOffset = 0;
        if (!(MemoryPropertyFlags_ & vk::MemoryPropertyFlagBits::eHostCoherent))
        {
            AdjustedOffset = AlignNonCoherentMemoryRange(Offset, Size);
        }

        vk::Result Result = MapMemory(Offset, Size, Target);
        if (Result == vk::Result::eSuccess && !(MemoryPropertyFlags_ & vk::MemoryPropertyFlagBits::eHostCoherent))
        {
            Target = static_cast<std::byte*>(Target) + AdjustedOffset;
        }

        MappedTargetMemory_ = Target;
        return Result;
    }

    vk::Result FVulkanDeviceMemory::MapMemoryForFetch(vk::DeviceSize Offset, vk::DeviceSize Size, void*& Data)
    {
        if (Allocator_ != nullptr && Allocation_ != nullptr)
        {
            VulkanCheck(vmaMapMemory(Allocator_, Allocation_, &Data));
            vmaGetAllocationInfo(Allocator_, Allocation_, &AllocationInfo_);
            if (Offset > 0)
            {
                Data = static_cast<std::byte*>(Data) + Offset;
            }

            MappedDataMemory_ = Data;
            return vk::Result::eSuccess;
        }

        vk::DeviceSize AdjustedOffset = 0;
        if (!(MemoryPropertyFlags_ & vk::MemoryPropertyFlagBits::eHostCoherent))
        {
            AdjustedOffset = AlignNonCoherentMemoryRange(Offset, Size);
        }

        vk::Result Result = MapMemory(Offset, Size, Data);
        if (Result == vk::Result::eSuccess && !(MemoryPropertyFlags_ & vk::MemoryPropertyFlagBits::eHostCoherent))
        {
            Data = static_cast<std::byte*>(Data) + AdjustedOffset;
            vk::MappedMemoryRange MappedMemoryRange(Handle_, Offset, Size);
            try
            {
                Device_.invalidateMappedMemoryRanges(MappedMemoryRange);
            }
            catch (const vk::SystemError& e)
            {
                NpgsCoreError("Failed to invalidate mapped memory range: {}", e.what());
                return static_cast<vk::Result>(e.code().value());
            }
        }

        MappedDataMemory_ = Data;
        return Result;
    }

    vk::Result FVulkanDeviceMemory::UnmapMemory(vk::DeviceSize Offset, vk::DeviceSize Size)
    {
        if (Allocator_ != nullptr && Allocation_ != nullptr)
        {
            vmaUnmapMemory(Allocator_, Allocation_);
            vmaGetAllocationInfo(Allocator_, Allocation_, &AllocationInfo_);
        }
        else
        {
            Device_.unmapMemory(Handle_);
        }

        MappedDataMemory_   = nullptr;
        MappedTargetMemory_ = nullptr;
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanDeviceMemory::SubmitData(vk::DeviceSize MapOffset, vk::DeviceSize SubmitOffset, vk::DeviceSize Size, const void* Data)
    {
        void* Target = nullptr;
        if (!bPersistentlyMapped_ || MappedTargetMemory_ == nullptr)
        {
            if (bPersistentlyMapped_)
            {
                NpgsAssert(MapOffset == 0, "when enable persistently mapped, MapOffset must be 0.");
            }

            VulkanHppCheck(MapMemoryForSubmit(MapOffset, Size, Target));
        }
        else
        {
            Target = MappedTargetMemory_;
        }

        std::copy_n(static_cast<const std::byte*>(Data), Size, static_cast<std::byte*>(Target) + SubmitOffset);

        if (!(MemoryPropertyFlags_ & vk::MemoryPropertyFlagBits::eHostCoherent))
        {
            if (Allocator_ != nullptr && Allocation_ != nullptr)
            {
                VulkanCheckWithMessage(vmaFlushAllocation(Allocator_, Allocation_, SubmitOffset, Size), "Failed to flush allocation");
            }
            else
            {
                // TODO
                AlignNonCoherentMemoryRange(SubmitOffset, Size);
                vk::MappedMemoryRange MappedMemoryRange(Handle_, SubmitOffset, Size);
                try
                {
                    Device_.flushMappedMemoryRanges(MappedMemoryRange);
                }
                catch (const vk::SystemError& e)
                {
                    NpgsCoreError("Failed to flush mapped memory range: {}", e.what());
                    return static_cast<vk::Result>(e.code().value());
                }
            }
        }

        if (!bPersistentlyMapped_)
        {
            return UnmapMemory(MapOffset, Size);
        }

        return vk::Result::eSuccess;
    }

    vk::Result FVulkanDeviceMemory::FetchData(vk::DeviceSize MapOffset, vk::DeviceSize FetchOffset, vk::DeviceSize Size, void* Target)
    {
        void* Data = nullptr;
        if (!bPersistentlyMapped_ || MappedTargetMemory_ == nullptr)
        {
            if (bPersistentlyMapped_)
            {
                NpgsAssert(MapOffset == 0, "when enable persistently mapped, MapOffset must be 0.");
            }

            VulkanHppCheck(MapMemoryForFetch(MapOffset, Size, Data));
        }
        else
        {
            Data = MappedDataMemory_;
        }

        std::copy_n(static_cast<const std::byte*>(Data) + FetchOffset, Size, static_cast<std::byte*>(Target));

        if (!(MemoryPropertyFlags_ & vk::MemoryPropertyFlagBits::eHostCoherent))
        {
            if (Allocator_ != nullptr && Allocation_ != nullptr)
            {
                VulkanCheckWithMessage(vmaInvalidateAllocation(Allocator_, Allocation_, FetchOffset, Size), "Failed to invalidate allocation");
            }
            else
            {
                // TODO
                AlignNonCoherentMemoryRange(FetchOffset, Size);
                vk::MappedMemoryRange MappedMemoryRange(Handle_, FetchOffset, Size);
                try
                {
                    Device_.invalidateMappedMemoryRanges(MappedMemoryRange);
                }
                catch (const vk::SystemError& e)
                {
                    NpgsCoreError("Failed to invalidate mapped memory range: {}", e.what());
                    return static_cast<vk::Result>(e.code().value());
                }
            }
        }

        if (!bPersistentlyMapped_)
        {
            return UnmapMemory(MapOffset, Size);
        }

        return vk::Result::eSuccess;
    }

    vk::Result FVulkanDeviceMemory::AllocateDeviceMemory(const vk::MemoryAllocateInfo& AllocateInfo)
    {
        if (AllocateInfo.memoryTypeIndex >= PhysicalDeviceMemoryProperties_.memoryTypeCount)
        {
            NpgsCoreError("Invalid memory type index: {}.", AllocateInfo.memoryTypeIndex);
            return vk::Result::eErrorMemoryMapFailed;
        }

        try
        {
            Handle_ = Device_.allocateMemory(AllocateInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to allocate memory: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }
        AllocationSize_      = AllocateInfo.allocationSize;
        MemoryPropertyFlags_ = PhysicalDeviceMemoryProperties_.memoryTypes[AllocateInfo.memoryTypeIndex].propertyFlags;

        NpgsCoreTrace("Device memory allocated successfully.");
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanDeviceMemory::AllocateDeviceMemory(const VmaAllocationCreateInfo& AllocationCreateInfo,
                                                         const vk::MemoryRequirements& MemoryRequirements)
    {
        VulkanCheckWithMessage(vmaAllocateMemory(Allocator_, reinterpret_cast<const VkMemoryRequirements*>(&MemoryRequirements),
                                                 &AllocationCreateInfo, &Allocation_, &AllocationInfo_), "Failed to allocate memory");

        Handle_         = AllocationInfo_.deviceMemory;
        AllocationSize_ = MemoryRequirements.size;

        ++HandleTracker_[Handle_];

        VmaAllocationInfo AllocationInfo;
        vmaGetAllocationInfo(Allocator_, Allocation_, &AllocationInfo);
        vmaGetMemoryTypeProperties(Allocator_, AllocationInfo.memoryType,
                                   reinterpret_cast<VkMemoryPropertyFlags*>(&MemoryPropertyFlags_));

        if (AllocationInfo_.pMappedData != nullptr)
        {
            NpgsAssert(false, "Don't use VMA_ALLOCATION_CREATE_MAPPED_BIT, try to use SetPersistentMapping");
        }

        NpgsCoreTrace("Device memory allocated successfully.");
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanDeviceMemory::MapMemory(vk::DeviceSize Offset, vk::DeviceSize Size, void*& Data) const
    {
        try
        {
            Data = Device_.mapMemory(Handle_, Offset, Size, {});
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to map memory: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        // NpgsCoreTrace("Memory mapped successfully.");
        return vk::Result::eSuccess;
    }

    vk::DeviceSize FVulkanDeviceMemory::AlignNonCoherentMemoryRange(vk::DeviceSize& Offset, vk::DeviceSize& Size) const
    {
        vk::DeviceSize NonCoherentAtomSize = PhysicalDeviceProperties_.limits.nonCoherentAtomSize;
        vk::DeviceSize OriginalOffset      = Offset;
        vk::DeviceSize RangeBegin          = Offset;
        vk::DeviceSize RangeEnd            = Offset + Size;

        RangeBegin = RangeBegin                           / NonCoherentAtomSize * NonCoherentAtomSize;
        RangeEnd   = (RangeEnd + NonCoherentAtomSize - 1) / NonCoherentAtomSize * NonCoherentAtomSize;
        RangeEnd   = std::min(RangeEnd, AllocationSize_);

        Offset = RangeBegin;
        Size   = RangeEnd - RangeBegin;

        return OriginalOffset - RangeBegin;
    }

    std::unordered_map<vk::DeviceMemory, std::size_t, FVulkanDeviceMemory::FVulkanDeviceMemoryHash> FVulkanDeviceMemory::HandleTracker_;

    // Wrapper for vk::Buffer
    // ----------------------
    FVulkanBuffer::FVulkanBuffer(vk::Device Device, const vk::PhysicalDeviceMemoryProperties& PhysicalDeviceMemoryProperties,
                                 const vk::BufferCreateInfo& CreateInfo)
        : Base(Device)
        , PhysicalDeviceMemoryProperties_(PhysicalDeviceMemoryProperties)
        , Allocator_(nullptr)
    {
        ReleaseInfo_ = "Buffer destroyed successfully.";
        Status_      = CreateBuffer(CreateInfo);
    }

    FVulkanBuffer::FVulkanBuffer(vk::Device Device, VmaAllocator Allocator,
                                 const VmaAllocationCreateInfo& AllocationCreateInfo,
                                 const vk::BufferCreateInfo& CreateInfo)
        : Base(Device)
        , PhysicalDeviceMemoryProperties_{}
        , Allocator_(Allocator)
    {
        ReleaseInfo_ = "Buffer destroyed successfully.";
        Status_      = CreateBuffer(AllocationCreateInfo, CreateInfo);
    }

    FVulkanBuffer::~FVulkanBuffer()
    {
        if (Allocator_ != nullptr && Allocation_ != nullptr)
        {
            vmaDestroyBuffer(Allocator_, Handle_, Allocation_);
            Handle_ = vk::Buffer();
            NpgsCoreTrace(ReleaseInfo_);
        }
    }

    vk::MemoryAllocateInfo FVulkanBuffer::CreateMemoryAllocateInfo(vk::MemoryPropertyFlags Flags) const
    {
        vk::MemoryRequirements MemoryRequirements = Device_.getBufferMemoryRequirements(Handle_);
        std::uint32_t MemoryTypeIndex = GetMemoryTypeIndex(PhysicalDeviceMemoryProperties_, MemoryRequirements, Flags);
        vk::MemoryAllocateInfo MemoryAllocateInfo(MemoryRequirements.size, MemoryTypeIndex);

        return MemoryAllocateInfo;
    }

    vk::Result FVulkanBuffer::BindMemory(const FVulkanDeviceMemory& DeviceMemory, vk::DeviceSize Offset) const
    {
        try
        {
            Device_.bindBufferMemory(Handle_, *DeviceMemory, Offset);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to bind buffer memory: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        NpgsCoreTrace("Buffer memory bound successfully.");
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanBuffer::CreateBuffer(const vk::BufferCreateInfo& CreateInfo)
    {
        try
        {
            Handle_ = Device_.createBuffer(CreateInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to create buffer: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        NpgsCoreTrace("Buffer created successfully.");
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanBuffer::CreateBuffer(const VmaAllocationCreateInfo& AllocationCreateInfo, const vk::BufferCreateInfo& CreateInfo)
    {
        VkBuffer Buffer = nullptr;
        VulkanCheckWithMessage(vmaCreateBuffer(Allocator_, reinterpret_cast<const VkBufferCreateInfo*>(&CreateInfo),
                                               &AllocationCreateInfo, &Buffer, &Allocation_, &AllocationInfo_), "Failed to create buffer");
        Handle_ = Buffer;

        NpgsCoreTrace("Buffer created successfully.");
        return vk::Result::eSuccess;
    }

    // Wrapper for vk::BufferView
    // --------------------------
    FVulkanBufferView::FVulkanBufferView(vk::Device Device, const vk::BufferViewCreateInfo& CreateInfo)
        : Base(Device)
    {
        ReleaseInfo_ = "Buffer view destroyed successfully.";
        Status_      = CreateBufferView(CreateInfo);
    }

    FVulkanBufferView::FVulkanBufferView(vk::Device Device, const FVulkanBuffer& Buffer, vk::Format Format,
                                         vk::DeviceSize Offset, vk::DeviceSize Range, vk::BufferViewCreateFlags Flags)
        : Base(Device)
    {
        ReleaseInfo_ = "Buffer view destroyed successfully.";
        Status_      = CreateBufferView(Buffer, Format, Offset, Range, Flags);
    }

    vk::Result FVulkanBufferView::CreateBufferView(const vk::BufferViewCreateInfo& CreateInfo)
    {
        try
        {
            Handle_ = Device_.createBufferView(CreateInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to create buffer view: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        NpgsCoreTrace("Buffer view created successfully.");
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanBufferView::CreateBufferView(const FVulkanBuffer& Buffer, vk::Format Format, vk::DeviceSize Offset,
                                                   vk::DeviceSize Range, vk::BufferViewCreateFlags Flags)
    {
        vk::BufferViewCreateInfo CreateInfo(Flags, *Buffer, Format, Offset, Range);
        return CreateBufferView(CreateInfo);
    }

    // Wrapper for vk::DescriptorSet
    // -----------------------------
    Npgs::FVulkanDescriptorSet::FVulkanDescriptorSet(vk::Device Device)
        : Device_(Device)
    {
    }

    // Wrapper for vk::DescriptorSetLayout
    // -----------------------------------
    FVulkanDescriptorSetLayout::FVulkanDescriptorSetLayout(vk::Device Device, const vk::DescriptorSetLayoutCreateInfo& CreateInfo)
        : Base(Device)
    {
        ReleaseInfo_ = "Descriptor set layout destroyed successfully.";
        Status_      = CreateDescriptorSetLayout(CreateInfo);
    }

    std::vector<vk::DescriptorSetLayout>
        FVulkanDescriptorSetLayout::GetNativeTypeArray(const vk::ArrayProxy<FVulkanDescriptorSetLayout>& WrappedTypeArray)
    {
        std::vector<vk::DescriptorSetLayout> NativeArray(WrappedTypeArray.size());
        for (std::size_t i = 0; i != WrappedTypeArray.size(); ++i)
        {
            NativeArray[i] = **(WrappedTypeArray.data() + i);
        }

        return NativeArray;
    }

    vk::Result FVulkanDescriptorSetLayout::CreateDescriptorSetLayout(const vk::DescriptorSetLayoutCreateInfo& CreateInfo)
    {
        try
        {
            Handle_ = Device_.createDescriptorSetLayout(CreateInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to create descriptor set layout: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        NpgsCoreTrace("Descriptor set layout created successfully.");
        return vk::Result::eSuccess;
    }

    // Wrapper for vk::DescriptorPool
    // ------------------------------
    FVulkanDescriptorPool::FVulkanDescriptorPool(vk::Device Device, const vk::DescriptorPoolCreateInfo& CreateInfo)
        : Base(Device)
    {
        ReleaseInfo_ = "Descriptor pool destroyed successfully.";
        Status_      = CreateDescriptorPool(CreateInfo);
    }

    FVulkanDescriptorPool::FVulkanDescriptorPool(vk::Device Device, std::uint32_t MaxSets, const vk::ArrayProxy<vk::DescriptorPoolSize>& PoolSizes,
                                                 vk::DescriptorPoolCreateFlags Flags)
        : Base(Device)
    {
        ReleaseInfo_ = "Descriptor pool destroyed successfully.";
        Status_      = CreateDescriptorPool(MaxSets, PoolSizes, Flags);
    }

    vk::Result FVulkanDescriptorPool::AllocateSets(const vk::ArrayProxy<vk::DescriptorSetLayout>& Layouts,
                                                   std::vector<vk::DescriptorSet>& Sets) const
    {
        if (Layouts.size() > Sets.size())
        {
            NpgsCoreError("Descriptor set layout count ({}) is larger than descriptor set count ({}).", Layouts.size(), Sets.size());
            return vk::Result::eErrorInitializationFailed;
        }

        vk::DescriptorSetAllocateInfo DescriptorSetAllocateInfo(Handle_, Layouts);
        try
        {
            Sets = Device_.allocateDescriptorSets(DescriptorSetAllocateInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to allocate descriptor sets: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        NpgsCoreTrace("Descriptor sets allocated successfully.");
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanDescriptorPool::AllocateSets(const vk::ArrayProxy<vk::DescriptorSetLayout>& Layouts,
                                                   std::vector<FVulkanDescriptorSet>& Sets) const
    {
        std::vector<vk::DescriptorSet> DescriptorSets(Layouts.size());
        VulkanHppCheck(AllocateSets(Layouts, DescriptorSets));

        Sets.resize(DescriptorSets.size());
        for (std::size_t i = 0; i != DescriptorSets.size(); ++i)
        {
            *Sets[i] = DescriptorSets[i];
        }

        return vk::Result::eSuccess;
    }

    vk::Result FVulkanDescriptorPool::AllocateSets(const vk::ArrayProxy<FVulkanDescriptorSetLayout>& Layouts,
                                                   std::vector<vk::DescriptorSet>& Sets) const
    {
        std::vector<vk::DescriptorSetLayout> DescriptorSetLayouts;
        DescriptorSetLayouts.reserve(Layouts.size());
        for (auto& Layout : Layouts)
        {
            DescriptorSetLayouts.push_back(*Layout);
        }

        return AllocateSets(DescriptorSetLayouts, Sets);
    }

    vk::Result FVulkanDescriptorPool::AllocateSets(const vk::ArrayProxy<FVulkanDescriptorSetLayout>& Layouts,
                                                   std::vector<FVulkanDescriptorSet>& Sets) const
    {
        std::vector<vk::DescriptorSetLayout> DescriptorSetLayouts;
        DescriptorSetLayouts.reserve(Layouts.size());
        for (auto& Layout : Layouts)
        {
            DescriptorSetLayouts.push_back(*Layout);
        }

        std::vector<vk::DescriptorSet> DescriptorSets(Layouts.size());
        VulkanHppCheck(AllocateSets(DescriptorSetLayouts, DescriptorSets));

        Sets.resize(DescriptorSets.size());
        for (std::size_t i = 0; i != DescriptorSets.size(); ++i)
        {
            *Sets[i] = DescriptorSets[i];
        }

        return vk::Result::eSuccess;
    }

    vk::Result FVulkanDescriptorPool::FreeSets(const vk::ArrayProxy<vk::DescriptorSet>& Sets) const
    {
        if (!Sets.empty())
        {
            Device_.freeDescriptorSets(Handle_, Sets);
        }

        return vk::Result::eSuccess;
    }

    vk::Result FVulkanDescriptorPool::FreeSets(const vk::ArrayProxy<FVulkanDescriptorSet>& Sets) const
    {
        if (!Sets.empty())
        {
            std::vector<vk::DescriptorSet> DescriptorSets;
            DescriptorSets.reserve(Sets.size());
            for (auto& Set : Sets)
            {
                DescriptorSets.push_back(*Set);
            }

            return FreeSets(DescriptorSets);
        }

        return vk::Result::eSuccess;
    }

    vk::Result FVulkanDescriptorPool::CreateDescriptorPool(const vk::DescriptorPoolCreateInfo& CreateInfo)
    {
        try
        {
            Handle_ = Device_.createDescriptorPool(CreateInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to create descriptor pool: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        NpgsCoreTrace("Descriptor pool created successfully.");
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanDescriptorPool::CreateDescriptorPool(std::uint32_t MaxSets, const vk::ArrayProxy<vk::DescriptorPoolSize>& PoolSizes,
                                                           vk::DescriptorPoolCreateFlags Flags)
    {
        vk::DescriptorPoolCreateInfo CreateInfo(Flags, MaxSets, PoolSizes);
        return CreateDescriptorPool(CreateInfo);
    }

    // Wrapper for vk::Fence
    // ---------------------
    FVulkanFence::FVulkanFence(vk::Device Device, const vk::FenceCreateInfo& CreateInfo)
        : Base(Device)
    {
        // ReleaseInfo_ = "Fence destroyed successfully.";
        Status_      = CreateFence(CreateInfo);
    }

    FVulkanFence::FVulkanFence(vk::Device Device, vk::FenceCreateFlags Flags)
        : Base(Device)
    {
        // ReleaseInfo_ = "Fence destroyed successfully.";
        Status_      = CreateFence(Flags);
    }

    vk::Result FVulkanFence::Wait() const
    {
        vk::Result Result;
        try
        {
            Result = Device_.waitForFences(Handle_, vk::True, std::numeric_limits<std::uint64_t>::max());
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to wait for fence: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        return Result;
    }

    vk::Result FVulkanFence::Reset() const
    {
        try
        {
            Device_.resetFences(Handle_);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to reset fence: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        return vk::Result::eSuccess;
    }

    vk::Result FVulkanFence::WaitAndReset() const
    {
        VulkanHppCheck(Wait());
        return Reset();
    }

    vk::Result FVulkanFence::GetStatus() const
    {
        VulkanHppCheckWithMessage(Device_.getFenceStatus(Handle_), "Failed to get fence status");
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanFence::CreateFence(const vk::FenceCreateInfo& CreateInfo)
    {
        try
        {
            Handle_ = Device_.createFence(CreateInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to create fence: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        // NpgsCoreTrace("Fence created successfully.");
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanFence::CreateFence(vk::FenceCreateFlags Flags)
    {
        vk::FenceCreateInfo FenceCreateInfo(Flags);
        return CreateFence(FenceCreateInfo);
    }

    // Wrapper for vk::Framebuffer
    // ---------------------------
    FVulkanFramebuffer::FVulkanFramebuffer(vk::Device Device, const vk::FramebufferCreateInfo& CreateInfo)
        : Base(Device)
    {
        ReleaseInfo_ = "Framebuffer destroyed successfully.";
        Status_      = CreateFramebuffer(CreateInfo);
    }

    vk::Result FVulkanFramebuffer::CreateFramebuffer(const vk::FramebufferCreateInfo& CreateInfo)
    {
        try
        {
            Handle_ = Device_.createFramebuffer(CreateInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to create framebuffer: {}.", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        NpgsCoreTrace("Framebuffer created successfully.");
        return vk::Result::eSuccess;
    }

    // Wrapper for vk::Image
    // ---------------------
    FVulkanImage::FVulkanImage(vk::Device Device, const vk::PhysicalDeviceMemoryProperties& PhysicalDeviceMemoryProperties,
                               const vk::ImageCreateInfo& CreateInfo)
        : Base(Device)
        , PhysicalDeviceMemoryProperties_(PhysicalDeviceMemoryProperties)
        , Allocator_(nullptr)
    {
        ReleaseInfo_ = "Image destroyed successfully.";
        Status_      = CreateImage(CreateInfo);
    }

    FVulkanImage::FVulkanImage(vk::Device Device, VmaAllocator Allocator,
                               const VmaAllocationCreateInfo& AllocationCreateInfo,
                               const vk::ImageCreateInfo& CreateInfo)
        : Base(Device)
        , PhysicalDeviceMemoryProperties_{}
        , Allocator_(Allocator)
    {
        ReleaseInfo_ = "Image destroyed successfully.";
        Status_      = CreateImage(AllocationCreateInfo, CreateInfo);
    }

    FVulkanImage::~FVulkanImage()
    {
        if (Allocator_ != nullptr && Allocation_ != nullptr)
        {
            vmaDestroyImage(Allocator_, Handle_, Allocation_);
            Handle_ = vk::Image();
            NpgsCoreTrace(ReleaseInfo_);
        }
    }

    vk::MemoryAllocateInfo FVulkanImage::CreateMemoryAllocateInfo(vk::MemoryPropertyFlags Flags) const
    {
        vk::MemoryRequirements MemoryRequirements = Device_.getImageMemoryRequirements(Handle_);
        std::uint32_t MemoryTypeIndex = GetMemoryTypeIndex(PhysicalDeviceMemoryProperties_, MemoryRequirements, Flags);

        if (MemoryTypeIndex == std::numeric_limits<std::uint32_t>::max() &&
            Flags & vk::MemoryPropertyFlagBits::eLazilyAllocated)
        {
            Flags &= ~vk::MemoryPropertyFlagBits::eLazilyAllocated;
            MemoryTypeIndex = GetMemoryTypeIndex(PhysicalDeviceMemoryProperties_, MemoryRequirements, Flags);
        }

        vk::MemoryAllocateInfo MemoryAllocateInfo(MemoryRequirements.size, MemoryTypeIndex);

        return MemoryAllocateInfo;
    }

    vk::Result FVulkanImage::BindMemory(const FVulkanDeviceMemory& DeviceMemory, vk::DeviceSize Offset) const
    {
        try
        {
            Device_.bindImageMemory(Handle_, *DeviceMemory, Offset);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to bind image memory: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        NpgsCoreTrace("Image memory bound successfully.");
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanImage::CreateImage(const vk::ImageCreateInfo& CreateInfo)
    {
        try
        {
            Handle_ = Device_.createImage(CreateInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to create image: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        NpgsCoreTrace("Image created successfully.");
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanImage::CreateImage(const VmaAllocationCreateInfo& AllocationCreateInfo, const vk::ImageCreateInfo& CreateInfo)
    {
        VkImage Image = nullptr;
        VulkanCheckWithMessage(vmaCreateImage(Allocator_, reinterpret_cast<const VkImageCreateInfo*>(&CreateInfo),
                                              &AllocationCreateInfo, &Image, &Allocation_, &AllocationInfo_), "Failed to create image");
        Handle_ = Image;

        NpgsCoreTrace("Image created successfully.");
        return vk::Result::eSuccess;
    }

    // Wrapper for vk::ImageView
    // -------------------------
    FVulkanImageView::FVulkanImageView(vk::Device Device, const vk::ImageViewCreateInfo& CreateInfo)
        : Base(Device)
    {
        ReleaseInfo_ = "Image view destroyed successfully.";
        Status_      = CreateImageView(CreateInfo);
    }

    FVulkanImageView::FVulkanImageView(vk::Device Device, const FVulkanImage& Image, vk::ImageViewType ViewType,
                                       vk::Format Format, vk::ComponentMapping Components,
                                       vk::ImageSubresourceRange SubresourceRange, vk::ImageViewCreateFlags Flags)
        : Base(Device)
    {
        ReleaseInfo_ = "Image view destroyed successfully.";
        Status_      = CreateImageView(Image, ViewType, Format, Components, SubresourceRange, Flags);
    }

    vk::Result FVulkanImageView::CreateImageView(const vk::ImageViewCreateInfo& CreateInfo)
    {
        try
        {
            Handle_ = Device_.createImageView(CreateInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to create image view: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        NpgsCoreTrace("Image view created successfully.");
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanImageView::CreateImageView(const FVulkanImage& Image, vk::ImageViewType ViewType, vk::Format Format,
                                                 vk::ComponentMapping Components, vk::ImageSubresourceRange SubresourceRange,
                                                 vk::ImageViewCreateFlags Flags)
    {
        vk::ImageViewCreateInfo CreateInfo(Flags, *Image, ViewType, Format, Components, SubresourceRange);
        return CreateImageView(CreateInfo);
    }

    // Wrapper for vk::PipelineCache
    // -----------------------------
    FVulkanPipelineCache::FVulkanPipelineCache(vk::Device Device, vk::PipelineCacheCreateFlags Flags)
        : Base(Device)
    {
        ReleaseInfo_ = "Pipeline cache destroyed successfully.";
        Status_      = CreatePipelineCache(Flags);
    }

    FVulkanPipelineCache::FVulkanPipelineCache(vk::Device Device, vk::PipelineCacheCreateFlags Flags,
                                               const vk::ArrayProxy<std::byte>& InitialData)
        : Base(Device)
    {
        ReleaseInfo_ = "Pipeline cache destroyed successfully.";
        Status_      = CreatePipelineCache(Flags, InitialData);
    }

    FVulkanPipelineCache::FVulkanPipelineCache(vk::Device Device, const vk::PipelineCacheCreateInfo& CreateInfo)
        : Base(Device)
    {
        ReleaseInfo_ = "Pipeline cache destroyed successfully.";
        Status_      = CreatePipelineCache(CreateInfo);
    }

    vk::Result FVulkanPipelineCache::CreatePipelineCache(vk::PipelineCacheCreateFlags Flags)
    {
        vk::PipelineCacheCreateInfo CreateInfo(Flags);
        return CreatePipelineCache(CreateInfo);
    }

    vk::Result FVulkanPipelineCache::CreatePipelineCache(vk::PipelineCacheCreateFlags Flags, const vk::ArrayProxy<std::byte>& InitialData)
    {
        vk::PipelineCacheCreateInfo CreateInfo = vk::PipelineCacheCreateInfo()
            .setFlags(Flags)
            .setInitialData<std::byte>(InitialData);

        return CreatePipelineCache(CreateInfo);
    }

    vk::Result FVulkanPipelineCache::CreatePipelineCache(const vk::PipelineCacheCreateInfo& CreateInfo)
    {
        try
        {
            Handle_ = Device_.createPipelineCache(CreateInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to create pipeline cache: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        NpgsCoreTrace("Pipeline cache created successfully.");
        return vk::Result::eSuccess;
    }

    // Wrapper for vk::Pipeline
    // ------------------------
    FVulkanPipeline::FVulkanPipeline(vk::Device Device, const vk::GraphicsPipelineCreateInfo& CreateInfo,
                                     const FVulkanPipelineCache* Cache)
        : Base(Device)
    {
        ReleaseInfo_ = "Graphics pipeline destroyed successfully.";
        Status_      = CreateGraphicsPipeline(CreateInfo, Cache);
    }

    FVulkanPipeline::FVulkanPipeline(vk::Device Device, const vk::ComputePipelineCreateInfo& CreateInfo,
                                     const FVulkanPipelineCache* Cache)
        : Base(Device)
    {
        ReleaseInfo_ = "Compute pipeline destroyed successfully.";
        Status_      = CreateComputePipeline(CreateInfo, Cache);
    }

    vk::Result FVulkanPipeline::CreateGraphicsPipeline(const vk::GraphicsPipelineCreateInfo& CreateInfo,
                                                       const FVulkanPipelineCache* Cache)
    {
        try
        {
            vk::PipelineCache PipelineCache = Cache ? **Cache : vk::PipelineCache();
            Handle_ = Device_.createGraphicsPipeline(PipelineCache, CreateInfo).value;
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to create graphics pipeline: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        NpgsCoreTrace("Graphics pipeline created successfully");
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanPipeline::CreateComputePipeline(const vk::ComputePipelineCreateInfo& CreateInfo,
                                                      const FVulkanPipelineCache* Cache)
    {
        try
        {
            vk::PipelineCache PipelineCache = Cache ? **Cache : vk::PipelineCache();
            Handle_ = Device_.createComputePipeline(PipelineCache, CreateInfo).value;
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to create compute pipeline: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        NpgsCoreTrace("Compute pipeline created successfully");
        return vk::Result::eSuccess;
    }

    // Wrapper for vk::PipelineLayout
    // ------------------------------
    FVulkanPipelineLayout::FVulkanPipelineLayout(vk::Device Device, const vk::PipelineLayoutCreateInfo& CreateInfo)
        : Base(Device)
    {
        ReleaseInfo_ = "Pipeline layout destroyed successfully.";
        Status_      = CreatePipelineLayout(CreateInfo);
    }

    vk::Result FVulkanPipelineLayout::CreatePipelineLayout(const vk::PipelineLayoutCreateInfo& CreateInfo)
    {
        try
        {
            Handle_ = Device_.createPipelineLayout(CreateInfo);
            return vk::Result::eSuccess;
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to create pipeline layout: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        NpgsCoreTrace("Pipeline layout created successfully");
        return vk::Result::eSuccess;
    }

    // Wrapper for vk::QueryPool
    // -------------------------
    FVulkanQueryPool::FVulkanQueryPool(vk::Device Device, const vk::QueryPoolCreateInfo& CreateInfo)
        : Base(Device)
    {
        ReleaseInfo_ = "Query pool destroyed successfully.";
        Status_      = CreateQueryPool(CreateInfo);
    }

    FVulkanQueryPool::FVulkanQueryPool(vk::Device Device, vk::QueryType QueryType, std::uint32_t QueryCount,
                                       vk::QueryPoolCreateFlags Flags, vk::QueryPipelineStatisticFlags PipelineStatisticsFlags)
        : Base(Device)
    {
        vk::QueryPoolCreateInfo CreateInfo(Flags, QueryType, QueryCount, PipelineStatisticsFlags);
        ReleaseInfo_ = "Query pool destroyed successfully.";
        Status_      = CreateQueryPool(CreateInfo);
    }

    vk::Result FVulkanQueryPool::Reset(std::uint32_t FirstQuery, std::uint32_t QueryCount)
    {
        try
        {
            Device_.resetQueryPool(Handle_, FirstQuery, QueryCount);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to reset query pool: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        NpgsCoreTrace("Query pool reset successfully.");
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanQueryPool::CreateQueryPool(const vk::QueryPoolCreateInfo& CreateInfo)
    {
        try
        {
            Handle_ = Device_.createQueryPool(CreateInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to create query pool: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        NpgsCoreTrace("Query pool created successfully.");
        return vk::Result::eSuccess;
    }

    // Wrapper for vk::RenderPass
    // --------------------------
    FVulkanRenderPass::FVulkanRenderPass(vk::Device Device, const vk::RenderPassCreateInfo& CreateInfo)
        : Base(Device)
    {
        ReleaseInfo_ = "Render pass destroyed successfully.";
        Status_      = CreateRenderPass(CreateInfo);
    }

    void FVulkanRenderPass::CommandBegin(const FVulkanCommandBuffer& CommandBuffer, const FVulkanFramebuffer& Framebuffer,
                                         const vk::Rect2D& RenderArea, const vk::ArrayProxy<vk::ClearValue>& ClearValues,
                                         const vk::SubpassContents& SubpassContents) const
    {
        vk::RenderPassBeginInfo RenderPassBeginInfo(Handle_, *Framebuffer, RenderArea, ClearValues);
        CommandBegin(CommandBuffer, RenderPassBeginInfo, SubpassContents);
    }

    vk::Result FVulkanRenderPass::CreateRenderPass(const vk::RenderPassCreateInfo& CreateInfo)
    {
        try
        {
            Handle_ = Device_.createRenderPass(CreateInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to create render pass: {}.", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        NpgsCoreTrace("Render pass created successfully.");
        return vk::Result::eSuccess;
    }

    // Wrapper for vk::Sampler
    // -----------------------
    FVulkanSampler::FVulkanSampler(vk::Device Device, const vk::SamplerCreateInfo& CreateInfo)
        : Base(Device)
    {
        ReleaseInfo_ = "Sampler destroyed successfully.";
        Status_      = CreateSampler(CreateInfo);
    }

    vk::Result FVulkanSampler::CreateSampler(const vk::SamplerCreateInfo& CreateInfo)
    {
        try
        {
            Handle_ = Device_.createSampler(CreateInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to create sampler: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        NpgsCoreTrace("Sampler created successfully.");
        return vk::Result::eSuccess;
    }

    // Wrapper for vk::Semaphore
    // -------------------------
    FVulkanSemaphore::FVulkanSemaphore(vk::Device Device, const vk::SemaphoreCreateInfo& CreateInfo)
        : Base(Device)
    {
        ReleaseInfo_ = "Semaphore destroyed successfully.";
        Status_      = CreateSemaphore(CreateInfo);
    }

    FVulkanSemaphore::FVulkanSemaphore(vk::Device Device, vk::SemaphoreCreateFlags Flags)
        : Base(Device)
    {
        ReleaseInfo_ = "Semaphore destroyed successfully.";
        Status_      = CreateSemaphore(Flags);
    }

    vk::Result FVulkanSemaphore::CreateSemaphore(const vk::SemaphoreCreateInfo& CreateInfo)
    {
        try
        {
            Handle_ = Device_.createSemaphore(CreateInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to create semaphore: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        NpgsCoreTrace("Semaphore created successfully.");
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanSemaphore::CreateSemaphore(vk::SemaphoreCreateFlags Flags)
    {
        vk::SemaphoreCreateInfo SemaphoreCreateInfo(Flags);
        return CreateSemaphore(SemaphoreCreateInfo);
    }

    // Wrapper for vk::ShaderModule
    // ----------------------------
    FVulkanShaderModule::FVulkanShaderModule(vk::Device Device, const vk::ShaderModuleCreateInfo& CreateInfo)
        : Base(Device)
    {
        ReleaseInfo_ = "Shader module destroyed successfully.";
        Status_      = CreateShaderModule(CreateInfo);
    }

    vk::Result FVulkanShaderModule::CreateShaderModule(const vk::ShaderModuleCreateInfo& CreateInfo)
    {
        try
        {
            Handle_ = Device_.createShaderModule(CreateInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to create shader module: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        NpgsCoreTrace("Shader module created successfully.");
        return vk::Result::eSuccess;
    }
    // -------------------
    // Native wrappers end

    FVulkanBufferMemory::FVulkanBufferMemory(vk::Device Device, const vk::PhysicalDeviceProperties& PhysicalDeviceProperties,
                                             const vk::PhysicalDeviceMemoryProperties& PhysicalDeviceMemoryProperties,
                                             const vk::BufferCreateInfo& BufferCreateInfo, vk::MemoryPropertyFlags MemoryPropertyFlags)
        : Base(std::make_unique<FVulkanBuffer>(Device, PhysicalDeviceMemoryProperties, BufferCreateInfo), nullptr)
    {
        auto MemoryAllocateInfo = Resource_->CreateMemoryAllocateInfo(MemoryPropertyFlags);
        vk::MemoryAllocateFlagsInfo MemoryAllocateFlagsInfo;
        if (BufferCreateInfo.usage & vk::BufferUsageFlagBits::eShaderDeviceAddress)
        {
            MemoryAllocateFlagsInfo.setFlags(vk::MemoryAllocateFlagBits::eDeviceAddress);
            MemoryAllocateInfo.setPNext(&MemoryAllocateFlagsInfo);
        }

        Memory_ = std::make_unique<FVulkanDeviceMemory>(
            Device, PhysicalDeviceProperties, PhysicalDeviceMemoryProperties, MemoryAllocateInfo);
        bMemoryBound_ = Memory_->IsValid() && (Resource_->BindMemory(*Memory_) == vk::Result::eSuccess);
    }

    FVulkanBufferMemory::FVulkanBufferMemory(vk::Device Device, VmaAllocator Allocator,
                                             const VmaAllocationCreateInfo& AllocationCreateInfo,
                                             const vk::BufferCreateInfo& BufferCreateInfo)
        : Base(std::make_unique<FVulkanBuffer>(Device, Allocator, AllocationCreateInfo, BufferCreateInfo), nullptr)
    {
        auto  Allocation     = Resource_->GetAllocation();
        auto& AllocationInfo = Resource_->GetAllocationInfo();
        Memory_ = std::make_unique<FVulkanDeviceMemory>(Device, Allocator, Allocation, AllocationInfo, AllocationInfo.deviceMemory);
        bMemoryBound_ = true;
    }

    FVulkanImageMemory::FVulkanImageMemory(vk::Device Device, const vk::PhysicalDeviceProperties& PhysicalDeviceProperties,
                                           const vk::PhysicalDeviceMemoryProperties& PhysicalDeviceMemoryProperties,
                                           const vk::ImageCreateInfo& ImageCreateInfo, vk::MemoryPropertyFlags MemoryPropertyFlags)
        : Base(std::make_unique<FVulkanImage>(Device, PhysicalDeviceMemoryProperties, ImageCreateInfo), nullptr)
    {
        auto MemoryAllocateInfo = Resource_->CreateMemoryAllocateInfo(MemoryPropertyFlags);
        Memory_ = std::make_unique<FVulkanDeviceMemory>(
            Device, PhysicalDeviceProperties, PhysicalDeviceMemoryProperties, MemoryAllocateInfo);
        bMemoryBound_ = Memory_->IsValid() && (Resource_->BindMemory(*Memory_) == vk::Result::eSuccess);
    }

    FVulkanImageMemory::FVulkanImageMemory(vk::Device Device, VmaAllocator Allocator,
                                           const VmaAllocationCreateInfo& AllocationCreateInfo,
                                           const vk::ImageCreateInfo& ImageCreateInfo)
        : Base(std::make_unique<FVulkanImage>(Device, Allocator, AllocationCreateInfo, ImageCreateInfo), nullptr)
    {
        auto  Allocation     = Resource_->GetAllocation();
        auto& AllocationInfo = Resource_->GetAllocationInfo();
        Memory_ = std::make_unique<FVulkanDeviceMemory>(Device, Allocator, Allocation, AllocationInfo, AllocationInfo.deviceMemory);
        bMemoryBound_ = true;
    }
} // namespace Npgs
