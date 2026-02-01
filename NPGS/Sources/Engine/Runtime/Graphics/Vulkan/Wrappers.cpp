#include "stdafx.h"
#include "Wrappers.hpp"

#include <algorithm>
#include <limits>
#include <utility>

#include <vulkan/vulkan_format_traits.hpp>

#include "Engine/Core/Base/Assert.hpp"
#include "Engine/Core/Base/Base.hpp"
#include "Engine/Core/Logger.hpp"
#include "Engine/Core/Utils/Utils.hpp"
#include "Engine/Core/Utils/VulkanUtils.hpp"

namespace Npgs
{
    namespace
    {
        template <typename Ty>
        void SetDebugUtilsObjectName(vk::Device Device, vk::ObjectType ObjectType, Ty Handle, std::string_view Name)
        {
#ifdef _DEBUG
            vk::DebugUtilsObjectNameInfoEXT NameInfo(
                ObjectType, reinterpret_cast<std::uint64_t>(static_cast<typename Ty::NativeType>(Handle)), Name.data());
            Device.setDebugUtilsObjectNameEXT(NameInfo);
#endif
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
            if (Utils::Equal(NumericFormat, "SINT")    || Utils::Equal(NumericFormat, "UINT")  ||
                Utils::Equal(NumericFormat, "SNORM")   || Utils::Equal(NumericFormat, "UNORM") ||
                Utils::Equal(NumericFormat, "SSCALED") || Utils::Equal(NumericFormat, "USCALED"))
            {
                RawDataType = ERawDataType::kInteger;
            }
            else if (Utils::Equal(NumericFormat, "SFLOAT") || Utils::Equal(NumericFormat, "UFLOAT"))
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
            NpgsCoreError("Failed to begin command buffer \"{}\": {}", HandleName_, e.what());
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
            NpgsCoreError("Failed to begin command buffer \"{}\": {}", HandleName_, e.what());
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
            NpgsCoreError("Failed to end command buffer \"{}\": {}", HandleName_, e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        return vk::Result::eSuccess;
    }

    // Wrapper for vk::CommandPool
    // ---------------------------
    FVulkanCommandPool::FVulkanCommandPool(vk::Device Device, std::string_view Name, const vk::CommandPoolCreateInfo& CreateInfo)
        : Base(Device, Name)
    {
        CreateCommandPool(CreateInfo);
    }

    FVulkanCommandPool::FVulkanCommandPool(vk::Device Device, std::string_view Name,
                                           std::uint32_t QueueFamilyIndex, vk::CommandPoolCreateFlags Flags)
        : Base(Device, Name)
    {
        Status_ = CreateCommandPool(QueueFamilyIndex, Flags);
    }

    vk::Result FVulkanCommandPool::AllocateBuffer(vk::CommandBufferLevel Level, std::string_view Name, vk::CommandBuffer& Buffer) const
    {
        vk::CommandBufferAllocateInfo CommandBufferAllocateInfo(Handle_, Level, 1);
        try
        {
            Buffer = Device_.allocateCommandBuffers(CommandBufferAllocateInfo)[0];
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to allocate command buffer \"{}\": {}", Name, e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        SetDebugUtilsObjectName(Device_, vk::ObjectType::eCommandBuffer, Buffer, Name);

        NpgsCoreTrace("Command buffer \"{}\" allocated successfully.", Name);
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCommandPool::AllocateBuffer(vk::CommandBufferLevel Level, std::string_view Name, FVulkanCommandBuffer& Buffer) const
    {
        vk::CommandBufferAllocateInfo CommandBufferAllocateInfo(Handle_, Level, 1);
        try
        {
            *Buffer = Device_.allocateCommandBuffers(CommandBufferAllocateInfo)[0];
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to allocate command buffer \"{}\": {}", Name, e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        SetDebugUtilsObjectName(Device_, vk::ObjectType::eCommandBuffer, *Buffer, Name);
        Buffer.SetHandleName(Name);

        NpgsCoreTrace("Command buffer \"{}\" allocated successfully.", Name);
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCommandPool::AllocateBuffers(vk::CommandBufferLevel Level, std::string_view Name, std::vector<vk::CommandBuffer>& Buffers) const
    {
        vk::CommandBufferAllocateInfo CommandBufferAllocateInfo(Handle_, Level, static_cast<std::uint32_t>(Buffers.size()));
        try
        {
            Buffers = Device_.allocateCommandBuffers(CommandBufferAllocateInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to allocate command buffer array \"{}\": {}", Name, e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        std::size_t Index = 0;
        for (const auto& Buffer : Buffers)
        {
            std::string BufferName = std::string(Name) + std::to_string(Index++);
            SetDebugUtilsObjectName(Device_, vk::ObjectType::eCommandBuffer, Buffer, BufferName);
        }

        NpgsCoreTrace("Command buffer array \"{}\" allocated successfully.", Name);
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCommandPool::AllocateBuffers(vk::CommandBufferLevel Level, std::string_view Name, std::vector<FVulkanCommandBuffer>& Buffers) const
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
            NpgsCoreError("Failed to allocate command buffer array \"{}\": {}", Name, e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        Buffers.resize(CommandBuffers.size());
        for (std::size_t i = 0; i != CommandBuffers.size(); ++i)
        {
            *Buffers[i] = CommandBuffers[i];
            std::string BufferName = std::string(Name) + std::to_string(i);
            SetDebugUtilsObjectName(Device_, vk::ObjectType::eCommandBuffer, CommandBuffers[i], BufferName);
            Buffers[i].SetHandleName(BufferName);
        }

        NpgsCoreTrace("Command buffer array \"{}\" allocated successfully.", Name);
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

    vk::Result FVulkanCommandPool::FreeBuffers(const vk::ArrayProxy<const vk::CommandBuffer>& Buffers) const
    {
        Device_.freeCommandBuffers(Handle_, Buffers);
        NpgsCoreTrace("Command buffers freed successfully.");
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCommandPool::FreeBuffers(const vk::ArrayProxy<const FVulkanCommandBuffer>& Buffers) const
    {
        std::vector<vk::CommandBuffer> CommandBuffers;
        CommandBuffers.reserve(Buffers.size());

        for (auto& Buffer : Buffers)
        {
            CommandBuffers.push_back(*Buffer);
        }

        return FreeBuffers(CommandBuffers);
    }

    vk::Result FVulkanCommandPool::Reset(vk::CommandPoolResetFlags Flags) const
    {
        try
        {
            Device_.resetCommandPool(Handle_, Flags);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to reset command pool \"{}\": {}", HandleName_, e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCommandPool::CreateCommandPool(const vk::CommandPoolCreateInfo& CreateInfo)
    {
        try
        {
            Handle_ = Device_.createCommandPool(CreateInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to create command pool \"{}\": {}", HandleName_, e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        SetDebugUtilsObjectName(Device_, vk::ObjectType::eCommandPool, Handle_, HandleName_);

        NpgsCoreTrace("Command pool \"{}\" created successfully.", HandleName_);
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCommandPool::CreateCommandPool(std::uint32_t QueueFamilyIndex, vk::CommandPoolCreateFlags Flags)
    {
        vk::CommandPoolCreateInfo CreateInfo(Flags, QueueFamilyIndex);
        return CreateCommandPool(CreateInfo);
    }

    // Wrapper for vk::DeviceMemory
    // ----------------------------
    FVulkanDeviceMemory::FVulkanDeviceMemory(vk::Device Device, std::string_view Name,
                                             VmaAllocator Allocator, VmaAllocation Allocation,
                                             const VmaAllocationInfo& AllocationInfo, vk::DeviceMemory Handle)
        : Base(Handle, Name)
        , Allocator_(Allocator)
        , Allocation_(Allocation)
        , AllocationInfo_(AllocationInfo)
    {
        SetDebugUtilsObjectName(Device, vk::ObjectType::eDeviceMemory, Handle, Name);

        vmaGetMemoryTypeProperties(Allocator_, AllocationInfo_.memoryType,
                                   reinterpret_cast<VkMemoryPropertyFlags*>(&MemoryPropertyFlags_));
    }

    FVulkanDeviceMemory::FVulkanDeviceMemory(FVulkanDeviceMemory&& Other) noexcept
        : Base(std::move(Other))
        , Allocator_(std::exchange(Other.Allocator_, nullptr))
        , Allocation_(std::exchange(Other.Allocation_, nullptr))
        , AllocationInfo_(std::exchange(Other.AllocationInfo_, {}))
        , MemoryPropertyFlags_(std::exchange(Other.MemoryPropertyFlags_, {}))
        , MappedDataMemory_(std::exchange(Other.MappedDataMemory_, nullptr))
        , MappedTargetMemory_(std::exchange(Other.MappedTargetMemory_, nullptr))
        , bPersistentlyMapped_(std::exchange(Other.bPersistentlyMapped_, false))
    {
    }

    FVulkanDeviceMemory::~FVulkanDeviceMemory()
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

    FVulkanDeviceMemory& FVulkanDeviceMemory::operator=(FVulkanDeviceMemory&& Other) noexcept
    {
        if (this != &Other)
        {
            Base::operator=(std::move(Other));

            Allocator_           = std::exchange(Other.Allocator_, nullptr);
            Allocation_          = std::exchange(Other.Allocation_, nullptr);
            AllocationInfo_      = std::exchange(Other.AllocationInfo_, {});
            MemoryPropertyFlags_ = std::exchange(Other.MemoryPropertyFlags_, {});
            MappedDataMemory_    = std::exchange(Other.MappedDataMemory_, nullptr);
            MappedTargetMemory_  = std::exchange(Other.MappedTargetMemory_, nullptr);
            bPersistentlyMapped_ = std::exchange(Other.bPersistentlyMapped_, false);
        }

        return *this;
    }

    vk::Result FVulkanDeviceMemory::MapMemoryForSubmit(vk::DeviceSize Offset, vk::DeviceSize Size, void*& Target)
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

    vk::Result FVulkanDeviceMemory::MapMemoryForFetch(vk::DeviceSize Offset, vk::DeviceSize Size, void*& Data)
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

    vk::Result FVulkanDeviceMemory::UnmapMemory(vk::DeviceSize Offset, vk::DeviceSize Size)
    {
        vmaUnmapMemory(Allocator_, Allocation_);
        vmaGetAllocationInfo(Allocator_, Allocation_, &AllocationInfo_);

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

        std::ranges::copy_n(static_cast<const std::byte*>(Data), Size, static_cast<std::byte*>(Target) + SubmitOffset);

        if (!(MemoryPropertyFlags_ & vk::MemoryPropertyFlagBits::eHostCoherent))
        {
            VulkanCheckWithMessage(vmaFlushAllocation(Allocator_, Allocation_, SubmitOffset, Size),
                                   "Failed to flush allocation");
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

        std::ranges::copy_n(static_cast<const std::byte*>(Data) + FetchOffset, Size, static_cast<std::byte*>(Target));

        if (!(MemoryPropertyFlags_ & vk::MemoryPropertyFlagBits::eHostCoherent))
        {
            VulkanCheckWithMessage(vmaInvalidateAllocation(Allocator_, Allocation_, FetchOffset, Size),
                                   "Failed to invalidate allocation");
        }

        if (!bPersistentlyMapped_)
        {
            return UnmapMemory(MapOffset, Size);
        }

        return vk::Result::eSuccess;
    }

    // Wrapper for vk::Buffer
    // ----------------------
    FVulkanBuffer::FVulkanBuffer(vk::Device Device, std::string_view Name, VmaAllocator Allocator,
                                 const VmaAllocationCreateInfo& AllocationCreateInfo, const vk::BufferCreateInfo& CreateInfo)
        : Base(Device, Name)
        , Allocator_(Allocator)
    {
        Status_ = CreateBuffer(AllocationCreateInfo, CreateInfo);
    }

    FVulkanBuffer::FVulkanBuffer(FVulkanBuffer&& Other) noexcept
        : Base(std::move(Other))
		, Allocator_(std::exchange(Other.Allocator_, nullptr))
        , Allocation_(std::exchange(Other.Allocation_, nullptr))
		, AllocationInfo_(std::exchange(Other.AllocationInfo_, {}))
    {
    }

    FVulkanBuffer::~FVulkanBuffer()
    {
        if (Allocator_ != nullptr && Allocation_ != nullptr)
        {
            vmaDestroyBuffer(Allocator_, Handle_, Allocation_);
            Handle_ = vk::Buffer();
            NpgsCoreTrace("Buffer \"{}\" destroyed successfully.", HandleName_);
        }
    }

    FVulkanBuffer& FVulkanBuffer::operator=(FVulkanBuffer&& Other) noexcept
    {
		if (this != &Other)
		{
			Base::operator=(std::move(Other));

			Allocator_      = std::exchange(Other.Allocator_, nullptr);
			Allocation_     = std::exchange(Other.Allocation_, nullptr);
			AllocationInfo_ = std::exchange(Other.AllocationInfo_, {});
		}

		return *this;
	}

    vk::Result FVulkanBuffer::BindMemory(const FVulkanDeviceMemory& DeviceMemory, vk::DeviceSize Offset) const
    {
        try
        {
            Device_.bindBufferMemory(Handle_, *DeviceMemory, Offset);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to bind buffer \"{}\" to memory \"{}\": {}", HandleName_, DeviceMemory.GetHandleName(), e.what());
            return static_cast<vk::Result>(e.code().value());
        }
    
        SetDebugUtilsObjectName(Device_, vk::ObjectType::eBuffer, Handle_, HandleName_);

        NpgsCoreTrace("Buffer \"{}\" successfully bind to memory \"{}\".", HandleName_, DeviceMemory.GetHandleName());
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanBuffer::CreateBuffer(const VmaAllocationCreateInfo& AllocationCreateInfo, const vk::BufferCreateInfo& CreateInfo)
    {
        VkBuffer Buffer = nullptr;
        VulkanCheckWithMessage(vmaCreateBuffer(Allocator_, reinterpret_cast<const VkBufferCreateInfo*>(&CreateInfo),
                                               &AllocationCreateInfo, &Buffer, &Allocation_, &AllocationInfo_), "Failed to create buffer");
        Handle_ = Buffer;

        SetDebugUtilsObjectName(Device_, vk::ObjectType::eBuffer, Handle_, HandleName_);

        NpgsCoreTrace("Buffer \"{}\" created successfully.", HandleName_);
        return vk::Result::eSuccess;
    }

    // Wrapper for vk::BufferView
    // --------------------------
    FVulkanBufferView::FVulkanBufferView(vk::Device Device, std::string_view Name, const vk::BufferViewCreateInfo& CreateInfo)
        : Base(Device, Name)
    {
        Status_ = CreateBufferView(CreateInfo);
    }

    vk::Result FVulkanBufferView::CreateBufferView(const vk::BufferViewCreateInfo& CreateInfo)
    {
        try
        {
            Handle_ = Device_.createBufferView(CreateInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to create buffer view \"{}\": {}", HandleName_, e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        SetDebugUtilsObjectName(Device_, vk::ObjectType::eBufferView, Handle_, HandleName_);

        NpgsCoreTrace("Buffer view \"{}\" created successfully.", HandleName_);
        return vk::Result::eSuccess;
    }

    // Wrapper for vk::DescriptorSetLayout
    // -----------------------------------
    FVulkanDescriptorSetLayout::FVulkanDescriptorSetLayout(vk::Device Device, std::string_view Name,
                                                           const vk::DescriptorSetLayoutCreateInfo& CreateInfo)
        : Base(Device, Name)
    {
        Status_ = CreateDescriptorSetLayout(CreateInfo);
    }

    std::vector<vk::DescriptorSetLayout>
    FVulkanDescriptorSetLayout::GetNativeTypeArray(const vk::ArrayProxy<const FVulkanDescriptorSetLayout>& WrappedTypeArray)
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
            NpgsCoreError("Failed to create descriptor set layout \"{}\": {}", HandleName_, e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        SetDebugUtilsObjectName(Device_, vk::ObjectType::eDescriptorSetLayout, Handle_, HandleName_);

        NpgsCoreTrace("Descriptor set layout \"{}\" created successfully.", HandleName_);
        return vk::Result::eSuccess;
    }

    // Wrapper for vk::Fence
    // ---------------------
    FVulkanFence::FVulkanFence(vk::Device Device, std::string_view Name, const vk::FenceCreateInfo& CreateInfo)
        : Base(Device, Name)
    {
        Status_ = CreateFence(CreateInfo);
    }

    FVulkanFence::FVulkanFence(vk::Device Device, std::string_view Name, vk::FenceCreateFlags Flags)
        : Base(Device, Name)
    {
        Status_ = CreateFence(Flags);
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
            NpgsCoreError("Failed to wait for fence \"{}\": {}", HandleName_, e.what());
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
            NpgsCoreError("Failed to reset fence \"{}\": {}", HandleName_, e.what());
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
            NpgsCoreError("Failed to create fence \"{}\": {}", HandleName_, e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        SetDebugUtilsObjectName(Device_, vk::ObjectType::eFence, Handle_, HandleName_);

        NpgsCoreTrace("Fence \"{}\" created successfully.", HandleName_);
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanFence::CreateFence(vk::FenceCreateFlags Flags)
    {
        vk::FenceCreateInfo FenceCreateInfo(Flags);
        return CreateFence(FenceCreateInfo);
    }

    // Wrapper for vk::Image
    // ---------------------
    FVulkanImage::FVulkanImage(vk::Device Device, std::string_view Name, VmaAllocator Allocator,
                               const VmaAllocationCreateInfo& AllocationCreateInfo,
                               const vk::ImageCreateInfo& CreateInfo)
        : Base(Device, Name)
        , Allocator_(Allocator)
    {
        Status_ = CreateImage(AllocationCreateInfo, CreateInfo);
    }

    FVulkanImage::FVulkanImage(FVulkanImage&& Other) noexcept
		: Base(std::move(Other))
        , Allocator_(std::exchange(Other.Allocator_, nullptr))
		, Allocation_(std::exchange(Other.Allocation_, nullptr))
    {
    }

    FVulkanImage::~FVulkanImage()
    {
        if (Allocator_ != nullptr && Allocation_ != nullptr)
        {
            vmaDestroyImage(Allocator_, Handle_, Allocation_);
            Handle_ = vk::Image();
            NpgsCoreTrace("Image \"{}\" desctoyed successfully.", HandleName_);
        }
    }

	FVulkanImage& FVulkanImage::operator=(FVulkanImage&& Other) noexcept
	{
		if (this != &Other)
		{
			Base::operator=(std::move(Other));

			Allocator_  = std::exchange(Other.Allocator_, nullptr);
			Allocation_ = std::exchange(Other.Allocation_, nullptr);
		}

		return *this;
	}

    vk::Result FVulkanImage::BindMemory(const FVulkanDeviceMemory& DeviceMemory, vk::DeviceSize Offset) const
    {
        try
        {
            Device_.bindImageMemory(Handle_, *DeviceMemory, Offset);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to bind image \"{}\" to memory \"{}\": {}", HandleName_, DeviceMemory.GetHandleName(), e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        SetDebugUtilsObjectName(Device_, vk::ObjectType::eImage, Handle_, HandleName_);
    
        NpgsCoreTrace("Image \"{}\" successfully bind to memory \"{}\".", HandleName_, DeviceMemory.GetHandleName());
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanImage::CreateImage(const VmaAllocationCreateInfo& AllocationCreateInfo, const vk::ImageCreateInfo& CreateInfo)
    {
        VkImage Image = nullptr;
        VulkanCheckWithMessage(vmaCreateImage(Allocator_, reinterpret_cast<const VkImageCreateInfo*>(&CreateInfo),
                                              &AllocationCreateInfo, &Image, &Allocation_, &AllocationInfo_), "Failed to create image");
        Handle_ = Image;

        SetDebugUtilsObjectName(Device_, vk::ObjectType::eImage, Handle_, HandleName_);

        NpgsCoreTrace("Image \"{}\" created successfully.", HandleName_);
        return vk::Result::eSuccess;
    }

    // Wrapper for vk::ImageView
    // -------------------------
    FVulkanImageView::FVulkanImageView(vk::Device Device, std::string_view Name, const vk::ImageViewCreateInfo& CreateInfo)
        : Base(Device, Name)
    {
        Status_ = CreateImageView(CreateInfo);
    }

    vk::Result FVulkanImageView::CreateImageView(const vk::ImageViewCreateInfo& CreateInfo)
    {
        try
        {
            Handle_ = Device_.createImageView(CreateInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to create image view \"{}\": {}", HandleName_, e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        SetDebugUtilsObjectName(Device_, vk::ObjectType::eImageView, Handle_, HandleName_);

        NpgsCoreTrace("Image view \"{}\" created successfully.", HandleName_);
        return vk::Result::eSuccess;
    }

    // Wrapper for vk::PipelineCache
    // -----------------------------
    FVulkanPipelineCache::FVulkanPipelineCache(vk::Device Device, std::string_view Name, vk::PipelineCacheCreateFlags Flags)
        : Base(Device, Name)
    {
        Status_ = CreatePipelineCache(Flags);
    }

    FVulkanPipelineCache::FVulkanPipelineCache(vk::Device Device, std::string_view Name, vk::PipelineCacheCreateFlags Flags,
                                               const vk::ArrayProxyNoTemporaries<const std::byte>& InitialData)
        : Base(Device, Name)
    {
        Status_ = CreatePipelineCache(Flags, InitialData);
    }

    FVulkanPipelineCache::FVulkanPipelineCache(vk::Device Device, std::string_view Name, const vk::PipelineCacheCreateInfo& CreateInfo)
        : Base(Device, Name)
    {
        Status_ = CreatePipelineCache(CreateInfo);
    }

    vk::Result FVulkanPipelineCache::CreatePipelineCache(vk::PipelineCacheCreateFlags Flags)
    {
        vk::PipelineCacheCreateInfo CreateInfo(Flags);
        return CreatePipelineCache(CreateInfo);
    }

    vk::Result FVulkanPipelineCache::CreatePipelineCache(vk::PipelineCacheCreateFlags Flags,
                                                         const vk::ArrayProxyNoTemporaries<const std::byte>& InitialData)
    {
        vk::PipelineCacheCreateInfo CreateInfo(Flags, InitialData);
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
            NpgsCoreError("Failed to create pipeline cache \"{}\": {}", HandleName_, e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        SetDebugUtilsObjectName(Device_, vk::ObjectType::ePipelineCache, Handle_, HandleName_);

        NpgsCoreTrace("Pipeline cache \"{}\" created successfully.", HandleName_);
        return vk::Result::eSuccess;
    }

    // Wrapper for vk::Pipeline
    // ------------------------
    FVulkanPipeline::FVulkanPipeline(vk::Device Device, std::string_view Name,
                                     const vk::GraphicsPipelineCreateInfo& CreateInfo,
                                     const FVulkanPipelineCache* Cache)
        : Base(Device, Name)
    {
        Status_ = CreateGraphicsPipeline(CreateInfo, Cache);
    }

    FVulkanPipeline::FVulkanPipeline(vk::Device Device, std::string_view Name,
                                     const vk::ComputePipelineCreateInfo& CreateInfo,
                                     const FVulkanPipelineCache* Cache)
        : Base(Device, Name)
    {
        Status_ = CreateComputePipeline(CreateInfo, Cache);
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
            NpgsCoreError("Failed to create graphics pipeline \"{}\": {}", HandleName_, e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        SetDebugUtilsObjectName(Device_, vk::ObjectType::ePipeline, Handle_, HandleName_);

        NpgsCoreTrace("Graphics pipeline \"{}\" created successfully.", HandleName_);
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
            NpgsCoreError("Failed to create compute pipeline \"{}\": {}", HandleName_, e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        SetDebugUtilsObjectName(Device_, vk::ObjectType::ePipeline, Handle_, HandleName_);

        NpgsCoreTrace("Compute pipeline \"{}\" created successfully.", HandleName_);
        return vk::Result::eSuccess;
    }

    // Wrapper for vk::PipelineLayout
    // ------------------------------
    FVulkanPipelineLayout::FVulkanPipelineLayout(vk::Device Device, std::string_view Name,
                                                 const vk::PipelineLayoutCreateInfo& CreateInfo)
        : Base(Device, Name)
    {
        Status_ = CreatePipelineLayout(CreateInfo);
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
            NpgsCoreError("Failed to create pipeline layout \"{}\": {}", HandleName_, e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        SetDebugUtilsObjectName(Device_, vk::ObjectType::ePipelineLayout, Handle_, HandleName_);

        NpgsCoreTrace("Pipeline layout \"{}\" created successfully.", HandleName_);
        return vk::Result::eSuccess;
    }

    // Wrapper for vk::QueryPool
    // -------------------------
    FVulkanQueryPool::FVulkanQueryPool(vk::Device Device, std::string_view Name, const vk::QueryPoolCreateInfo& CreateInfo)
        : Base(Device, Name)
    {
        Status_ = CreateQueryPool(CreateInfo);
    }

    FVulkanQueryPool::FVulkanQueryPool(vk::Device Device, std::string_view Name, vk::QueryType QueryType, std::uint32_t QueryCount,
                                       vk::QueryPoolCreateFlags Flags, vk::QueryPipelineStatisticFlags PipelineStatisticsFlags)
        : Base(Device, Name)
    {
        vk::QueryPoolCreateInfo CreateInfo(Flags, QueryType, QueryCount, PipelineStatisticsFlags);
        Status_ = CreateQueryPool(CreateInfo);
    }

    vk::Result FVulkanQueryPool::Reset(std::uint32_t FirstQuery, std::uint32_t QueryCount)
    {
        try
        {
            Device_.resetQueryPool(Handle_, FirstQuery, QueryCount);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to reset query pool \"{}\": {}", HandleName_, e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        NpgsCoreTrace("Query pool \"{}\" reset successfully.", HandleName_);
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
            NpgsCoreError("Failed to create query pool \"{}\": {}", HandleName_, e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        SetDebugUtilsObjectName(Device_, vk::ObjectType::eQueryPool, Handle_, HandleName_);

        NpgsCoreTrace("Query pool \"{}\" created successfully.", HandleName_);
        return vk::Result::eSuccess;
    }

    // Wrapper for vk::Sampler
    // -----------------------
    FVulkanSampler::FVulkanSampler(vk::Device Device, std::string_view Name, const vk::SamplerCreateInfo& CreateInfo)
        : Base(Device, Name)
    {
        Status_ = CreateSampler(CreateInfo);
    }

    vk::Result FVulkanSampler::CreateSampler(const vk::SamplerCreateInfo& CreateInfo)
    {
        try
        {
            Handle_ = Device_.createSampler(CreateInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to create sampler \"{}\": {}", HandleName_, e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        SetDebugUtilsObjectName(Device_, vk::ObjectType::eSampler, Handle_, HandleName_);

        NpgsCoreTrace("Sampler \"{}\" created successfully.", HandleName_);
        return vk::Result::eSuccess;
    }

    // Wrapper for vk::Semaphore
    // -------------------------
    FVulkanSemaphore::FVulkanSemaphore(vk::Device Device, std::string_view Name, const vk::SemaphoreCreateInfo& CreateInfo)
        : Base(Device, Name)
    {
        Status_ = CreateSemaphore(CreateInfo);
    }

    FVulkanSemaphore::FVulkanSemaphore(vk::Device Device, std::string_view Name, vk::SemaphoreCreateFlags Flags)
        : Base(Device, Name)
    {
        Status_ = CreateSemaphore(Flags);
    }

    vk::Result FVulkanSemaphore::CreateSemaphore(const vk::SemaphoreCreateInfo& CreateInfo)
    {
        try
        {
            Handle_ = Device_.createSemaphore(CreateInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to create semaphore \"{}\": {}", HandleName_, e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        SetDebugUtilsObjectName(Device_, vk::ObjectType::eSemaphore, Handle_, HandleName_);

        NpgsCoreTrace("Semaphore \"{}\" created successfully.", HandleName_);
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanSemaphore::CreateSemaphore(vk::SemaphoreCreateFlags Flags)
    {
        vk::SemaphoreCreateInfo SemaphoreCreateInfo(Flags);
        return CreateSemaphore(SemaphoreCreateInfo);
    }

    // Wrapper for vk::ShaderEXT
    // -------------------------
    FVulkanShader::FVulkanShader(vk::Device Device, std::string_view Name, const vk::ShaderCreateInfoEXT& CreateInfo)
        : Base(Device, Name)
    {
        Status_ = CreateShader(CreateInfo);
    }

    vk::Result FVulkanShader::CreateShader(const vk::ShaderCreateInfoEXT& CreateInfo)
    {
        try
        {
            Handle_ = Device_.createShaderEXT(CreateInfo).value;
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to create shader \"{}\": {}", HandleName_, e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        SetDebugUtilsObjectName(Device_, vk::ObjectType::eShaderEXT, Handle_, HandleName_);

        NpgsCoreTrace("Shader \"{}\" created successfully.", HandleName_);
        return vk::Result::eSuccess;
    }

    // Wrapper for vk::ShaderModule
    // ----------------------------
    FVulkanShaderModule::FVulkanShaderModule(vk::Device Device, std::string_view Name,
                                             const vk::ShaderModuleCreateInfo& CreateInfo)
        : Base(Device, Name)
    {
        Status_ = CreateShaderModule(CreateInfo);
    }

    vk::Result FVulkanShaderModule::CreateShaderModule(const vk::ShaderModuleCreateInfo& CreateInfo)
    {
        try
        {
            Handle_ = Device_.createShaderModule(CreateInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to create shader module \"{}\": {}", HandleName_, e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        SetDebugUtilsObjectName(Device_, vk::ObjectType::eShaderModule, Handle_, HandleName_);

        NpgsCoreTrace("Shader module \"{}\" created successfully.", HandleName_);
        return vk::Result::eSuccess;
    }
    // -------------------
    // Native wrappers end

    FVulkanBufferMemory::FVulkanBufferMemory(vk::Device Device, std::string_view BufferName, std::string_view MemoryName,
                                             VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo,
                                             const vk::BufferCreateInfo& BufferCreateInfo)
        : Base(std::make_unique<FVulkanBuffer>(Device, BufferName, Allocator, AllocationCreateInfo, BufferCreateInfo), nullptr)
    {
        auto  Allocation     = Resource_->GetAllocation();
        auto& AllocationInfo = Resource_->GetAllocationInfo();

        Memory_ = std::make_unique<FVulkanDeviceMemory>(
            Device, MemoryName, Allocator, Allocation, AllocationInfo, AllocationInfo.deviceMemory);
    }

    FVulkanImageMemory::FVulkanImageMemory(vk::Device Device, std::string_view ImageName, std::string_view MemoryName,
                                           VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo,
                                           const vk::ImageCreateInfo& ImageCreateInfo)
        : Base(std::make_unique<FVulkanImage>(Device, ImageName, Allocator, AllocationCreateInfo, ImageCreateInfo), nullptr)
    {
        auto  Allocation     = Resource_->GetAllocation();
        auto& AllocationInfo = Resource_->GetAllocationInfo();

        Memory_ = std::make_unique<FVulkanDeviceMemory>(
            Device, MemoryName, Allocator, Allocation, AllocationInfo, AllocationInfo.deviceMemory);
    }
} // namespace Npgs
