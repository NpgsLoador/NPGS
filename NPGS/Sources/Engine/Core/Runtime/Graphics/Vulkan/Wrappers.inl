#include <utility>
#include "Engine/Core/Base/Base.hpp"

namespace Npgs
{
    // TVulkanHandleNoDestroy
    // ----------------------
    template <typename HandleType>
    requires std::is_class_v<HandleType>
    TVulkanHandleNoDestroy<HandleType>::TVulkanHandleNoDestroy(HandleType Handle)
        : Handle_(Handle)
    {
    }

    template <typename HandleType>
    requires std::is_class_v<HandleType>
    TVulkanHandleNoDestroy<HandleType>::TVulkanHandleNoDestroy(TVulkanHandleNoDestroy&& Other) noexcept
        : Handle_(std::move(Other.Handle_))
    {
    }

    template <typename HandleType>
    requires std::is_class_v<HandleType>
    NPGS_INLINE TVulkanHandleNoDestroy<HandleType>&
    TVulkanHandleNoDestroy<HandleType>::operator=(TVulkanHandleNoDestroy&& Other) noexcept
    {
        if (this != &Other)
        {
            Handle_ = std::move(Other.Handle_);
        }

        return *this;
    }

    template <typename HandleType>
    requires std::is_class_v<HandleType>
    NPGS_INLINE TVulkanHandleNoDestroy<HandleType>&
    TVulkanHandleNoDestroy<HandleType>::operator=(HandleType Handle)
    {
        Handle_ = Handle;
        return *this;
    }

    template <typename HandleType>
    requires std::is_class_v<HandleType>
    NPGS_INLINE HandleType* TVulkanHandleNoDestroy<HandleType>::operator->()
    {
        return &Handle_;
    }

    template <typename HandleType>
    requires std::is_class_v<HandleType>
    NPGS_INLINE const HandleType* TVulkanHandleNoDestroy<HandleType>::operator->() const
    {
        return &Handle_;
    }

    template <typename HandleType>
    requires std::is_class_v<HandleType>
    NPGS_INLINE HandleType& TVulkanHandleNoDestroy<HandleType>::operator*()
    {
        return Handle_;
    }

    template <typename HandleType>
    requires std::is_class_v<HandleType>
    NPGS_INLINE const HandleType& TVulkanHandleNoDestroy<HandleType>::operator*() const
    {
        return Handle_;
    }

    template <typename HandleType>
    requires std::is_class_v<HandleType>
    NPGS_INLINE TVulkanHandleNoDestroy<HandleType>::operator bool() const
    {
        return IsValid();
    }

    template <typename HandleType>
    requires std::is_class_v<HandleType>
    NPGS_INLINE bool TVulkanHandleNoDestroy<HandleType>::IsValid() const
    {
        return static_cast<bool>(Handle_);
    }

    NPGS_INLINE FGraphicsPipelineCreateInfoPack::operator vk::GraphicsPipelineCreateInfo& ()
    {
        return GraphicsPipelineCreateInfo;
    }

    NPGS_INLINE FGraphicsPipelineCreateInfoPack::operator const vk::GraphicsPipelineCreateInfo& () const
    {
        return GraphicsPipelineCreateInfo;
    }

    // TVulkanHandle
    // -------------
    template <typename HandleType, bool bEnableReleaseInfoOutput, EVulkanHandleReleaseMethod ReleaseMethod>
    requires std::is_class_v<HandleType>
    TVulkanHandle<HandleType, bEnableReleaseInfoOutput, ReleaseMethod>::TVulkanHandle(vk::Device Device, HandleType Handle, const std::string& HandleName)
        : Base(Handle)
        , ReleaseInfo_(std::string(HandleName) + " destroyed successfully.")
        , Device_(Device)
        , Status_(vk::Result::eSuccess)
    {
    }

    template <typename HandleType, bool bEnableReleaseInfoOutput, EVulkanHandleReleaseMethod ReleaseMethod>
    requires std::is_class_v<HandleType>
    TVulkanHandle<HandleType, bEnableReleaseInfoOutput, ReleaseMethod>::TVulkanHandle(vk::Device Device)
        : Device_(Device)
        , Status_(vk::Result::eSuccess)
    {
    }

    template <typename HandleType, bool bEnableReleaseInfoOutput, EVulkanHandleReleaseMethod ReleaseMethod>
    requires std::is_class_v<HandleType>
    TVulkanHandle<HandleType, bEnableReleaseInfoOutput, ReleaseMethod>::TVulkanHandle(TVulkanHandle&& Other) noexcept
        : Base(std::move(Other))
        , ReleaseInfo_(std::move(Other.ReleaseInfo_))
        , Device_(std::move(Other.Device_))
        , Status_(std::exchange(Other.Status_, {}))
    {
    }

    template <typename HandleType, bool bEnableReleaseInfoOutput, EVulkanHandleReleaseMethod ReleaseMethod>
    requires std::is_class_v<HandleType>
    TVulkanHandle<HandleType, bEnableReleaseInfoOutput, ReleaseMethod>::~TVulkanHandle()
    {
        if (this->Handle_)
        {
            ReleaseHandle();
            if constexpr (bEnableReleaseInfoOutput)
            {
                NpgsCoreTrace(ReleaseInfo_);
            }
        }
    }

    template <typename HandleType, bool bEnableReleaseInfoOutput, EVulkanHandleReleaseMethod ReleaseMethod>
    requires std::is_class_v<HandleType>
    TVulkanHandle<HandleType, bEnableReleaseInfoOutput, ReleaseMethod>&
    TVulkanHandle<HandleType, bEnableReleaseInfoOutput, ReleaseMethod>::operator=(TVulkanHandle&& Other) noexcept
    {
        if (this != &Other)
        {
            if (this->Handle_)
            {
                ReleaseHandle();
            }

            Base::operator=(std::move(Other));

            ReleaseInfo_ = std::move(Other.ReleaseInfo_);
            Device_      = std::move(Other.Device_);
            Status_      = std::exchange(Other.Status_, {});
        }

        return *this;
    }

    template <typename HandleType, bool bEnableReleaseInfoOutput, EVulkanHandleReleaseMethod ReleaseMethod>
    requires std::is_class_v<HandleType>
    NPGS_INLINE vk::Result TVulkanHandle<HandleType, bEnableReleaseInfoOutput, ReleaseMethod>::GetStatus() const
    {
        return Status_;
    }

    template <typename HandleType, bool bEnableReleaseInfoOutput, EVulkanHandleReleaseMethod ReleaseMethod>
    requires std::is_class_v<HandleType>
    void TVulkanHandle<HandleType, bEnableReleaseInfoOutput, ReleaseMethod>::ReleaseHandle()
    {
        if (this->Handle_)
        {
            if constexpr (ReleaseMethod == EVulkanHandleReleaseMethod::kDestroy)
            {
                Device_.destroy(this->Handle_);
            }
            else if constexpr (ReleaseMethod == EVulkanHandleReleaseMethod::kFree)
            {
                Device_.free(this->Handle_);
            }
        }
    }

    // Wrapper for vk::DeviceMemory
    // ----------------------------
    template <typename ContainerType>
    requires std::is_class_v<ContainerType>
    vk::Result FVulkanDeviceMemory::SubmitData(const ContainerType& Data)
    {
        using ValueType = typename ContainerType::value_type;
        static_assert(std::is_standard_layout_v<ValueType>, "Container value_type must be standard layout type");

        vk::DeviceSize DataSize = Data.size() * sizeof(ValueType);
        return SubmitData(0, 0, DataSize, static_cast<const void*>(Data.data()));
    }

    template <typename ContainerType>
    requires std::is_class_v<ContainerType>
    vk::Result FVulkanDeviceMemory::FetchData(ContainerType& Data)
    {
        using ValueType = typename ContainerType::value_type;
        static_assert(std::is_standard_layout_v<ValueType>, "Container value_type must be standard layout type");

        Data.reserve(AllocationInfo_.size / sizeof(ValueType));
        Data.resize(AllocationInfo_.size / sizeof(ValueType));
        return FetchData(0, 0, AllocationInfo_.size, static_cast<void*>(Data.data()));
    }

    NPGS_INLINE const void* FVulkanDeviceMemory::GetMappedDataMemory() const
    {
        return MappedDataMemory_;
    }

    NPGS_INLINE void* FVulkanDeviceMemory::GetMappedTargetMemory()
    {
        return MappedTargetMemory_;
    }

    NPGS_INLINE vk::DeviceSize FVulkanDeviceMemory::GetAllocationSize() const
    {
        return AllocationInfo_.size;
    }

    NPGS_INLINE vk::MemoryPropertyFlags FVulkanDeviceMemory::GetMemoryPropertyFlags() const
    {
        return MemoryPropertyFlags_;
    }

    NPGS_INLINE bool FVulkanDeviceMemory::IsPereistentlyMapped() const
    {
        return bPersistentlyMapped_;
    }

    // Wrapper fo vk::Buffer
    // ---------------------
    NPGS_INLINE vk::DeviceSize FVulkanBuffer::GetDeviceAddress() const
    {
        vk::BufferDeviceAddressInfo AddressInfo(Handle_);
        return Device_.getBufferAddress(AddressInfo);
    }

    NPGS_INLINE VmaAllocator FVulkanBuffer::GetAllocator() const
    {
        return Allocator_;
    }

    NPGS_INLINE VmaAllocation FVulkanBuffer::GetAllocation() const
    {
        return Allocation_;
    }

    NPGS_INLINE const VmaAllocationInfo& FVulkanBuffer::GetAllocationInfo() const
    {
        return AllocationInfo_;
    }

    // Wrapper for vk::DescriptorSet
    // -----------------------------
    NPGS_INLINE void
    FVulkanDescriptorSet::Write(const vk::ArrayProxy<const vk::DescriptorImageInfo>& ImageInfos, vk::DescriptorType Type,
                                std::uint32_t BindingPoint, std::uint32_t ArrayElement)
    {
        vk::WriteDescriptorSet WriteDescriptorSet(Handle_, BindingPoint, ArrayElement, Type, ImageInfos);
        Update(WriteDescriptorSet);
    }

    NPGS_INLINE void
    FVulkanDescriptorSet::Write(const vk::ArrayProxy<const vk::DescriptorBufferInfo>& BufferInfos, vk::DescriptorType Type,
                                std::uint32_t BindingPoint, std::uint32_t ArrayElement)
    {
        vk::WriteDescriptorSet WriteDescriptorSet(Handle_, BindingPoint, ArrayElement, Type, {}, BufferInfos);
        Update(WriteDescriptorSet);
    }

    NPGS_INLINE void
    FVulkanDescriptorSet::Write(const vk::ArrayProxy<const vk::BufferView>& BufferViews, vk::DescriptorType Type,
                                std::uint32_t BindingPoint, std::uint32_t ArrayElement)
    {
        vk::WriteDescriptorSet WriteDescriptorSet(Handle_, BindingPoint, ArrayElement, Type, {}, {}, BufferViews);
        Update(WriteDescriptorSet);
    }

    NPGS_INLINE void FVulkanDescriptorSet::Update(const vk::ArrayProxy<const vk::WriteDescriptorSet>& Writes,
                                                  const vk::ArrayProxy<const vk::CopyDescriptorSet>& Copies)
    {
        Device_.updateDescriptorSets(Writes, Copies);
    }

    // Wrapper for vk::Image
    // ---------------------
    NPGS_INLINE VmaAllocator FVulkanImage::GetAllocator() const
    {
        return Allocator_;
    }

    NPGS_INLINE VmaAllocation FVulkanImage::GetAllocation() const
    {
        return Allocation_;
    }

    NPGS_INLINE const VmaAllocationInfo& FVulkanImage::GetAllocationInfo() const
    {
        return AllocationInfo_;
    }

    // Wrapper for vk::QueryPool
    // -------------------------
    template <typename DataType>
    std::vector<DataType>
    FVulkanQueryPool::GetResult(std::uint32_t FirstQuery, std::uint32_t QueryCount, std::size_t DataSize, vk::DeviceSize Stride, vk::QueryResultFlags Flags)
    {
        try
        {
            std::vector<DataType> Results = Device_.getQueryPoolResults(Handle_, FirstQuery, QueryCount, DataSize, Stride, Flags).value;
            return Results;
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to get query pool results: {}", e.code().value());
            return {};
        }
    }

    // Wrapper for vk::RenderPass
    // --------------------------
    NPGS_INLINE void FVulkanRenderPass::CommandBegin(const FVulkanCommandBuffer& CommandBuffer,
                                                     const vk::RenderPassBeginInfo& BeginInfo,
                                                     const vk::SubpassContents& SubpassContents) const
    {
        CommandBuffer->beginRenderPass(BeginInfo, SubpassContents);
    }

    NPGS_INLINE void FVulkanRenderPass::CommandNext(const FVulkanCommandBuffer& CommandBuffer,
                                                    const vk::SubpassContents& SubpassContents) const
    {
        CommandBuffer->nextSubpass(SubpassContents);
    }

    NPGS_INLINE void FVulkanRenderPass::CommandEnd(const FVulkanCommandBuffer& CommandBuffer) const
    {
        CommandBuffer->endRenderPass();
    }
    // -------------------
    // Native wrappers end

    // TVulkanResourceMemory
    // ---------------------
    template <typename ResourceType, typename MemoryType>
    requires std::is_class_v<ResourceType>&& std::is_class_v<MemoryType>
    TVulkanResourceMemory<ResourceType, MemoryType>::TVulkanResourceMemory(std::unique_ptr<ResourceType>&& Resource, std::unique_ptr<MemoryType>&& Memory)
        : Resource_(std::move(Resource))
        , Memory_(std::move(Memory))
    {
    }

    template <typename ResourceType, typename MemoryType>
    requires std::is_class_v<ResourceType>&& std::is_class_v<MemoryType>
    TVulkanResourceMemory<ResourceType, MemoryType>::TVulkanResourceMemory(TVulkanResourceMemory&& Other) noexcept
        : Resource_(std::move(Other.Resource_))
        , Memory_(std::move(Other.Memory_))
    {
    }

    template <typename ResourceType, typename MemoryType>
    requires std::is_class_v<ResourceType>&& std::is_class_v<MemoryType>
    TVulkanResourceMemory<ResourceType, MemoryType>& TVulkanResourceMemory<ResourceType, MemoryType>::operator=(TVulkanResourceMemory&& Other) noexcept
    {
        if (this != &Other)
        {
            Resource_ = std::move(Other.Resource_);
            Memory_   = std::move(Other.Memory_);
        }

        return *this;
    }

    template <typename ResourceType, typename MemoryType>
    requires std::is_class_v<ResourceType>&& std::is_class_v<MemoryType>
    NPGS_INLINE TVulkanResourceMemory<ResourceType, MemoryType>::operator MemoryType& ()
    {
        return GetMemory();
    }

    template <typename ResourceType, typename MemoryType>
    requires std::is_class_v<ResourceType>&& std::is_class_v<MemoryType>
    NPGS_INLINE TVulkanResourceMemory<ResourceType, MemoryType>::operator const MemoryType& () const
    {
        return GetMemory();
    }

    template <typename ResourceType, typename MemoryType>
    requires std::is_class_v<ResourceType>&& std::is_class_v<MemoryType>
    NPGS_INLINE TVulkanResourceMemory<ResourceType, MemoryType>::operator ResourceType& ()
    {
        return GetResource();
    }

    template <typename ResourceType, typename MemoryType>
    requires std::is_class_v<ResourceType>&& std::is_class_v<MemoryType>
    NPGS_INLINE TVulkanResourceMemory<ResourceType, MemoryType>::operator const ResourceType& () const
    {
        return GetResource();
    }

    template <typename ResourceType, typename MemoryType>
    requires std::is_class_v<ResourceType>&& std::is_class_v<MemoryType>
    NPGS_INLINE TVulkanResourceMemory<ResourceType, MemoryType>::operator bool() const
    {
        return IsValid();
    }

    template <typename ResourceType, typename MemoryType>
    requires std::is_class_v<ResourceType>&& std::is_class_v<MemoryType>
    NPGS_INLINE MemoryType& TVulkanResourceMemory<ResourceType, MemoryType>::GetMemory()
    {
        return *Memory_;
    }

    template <typename ResourceType, typename MemoryType>
    requires std::is_class_v<ResourceType>&& std::is_class_v<MemoryType>
    NPGS_INLINE const MemoryType& TVulkanResourceMemory<ResourceType, MemoryType>::GetMemory() const
    {
        return *Memory_;
    }

    template <typename ResourceType, typename MemoryType>
    requires std::is_class_v<ResourceType>&& std::is_class_v<MemoryType>
    NPGS_INLINE ResourceType& TVulkanResourceMemory<ResourceType, MemoryType>::GetResource()
    {
        return *Resource_;
    }

    template <typename ResourceType, typename MemoryType>
    requires std::is_class_v<ResourceType>&& std::is_class_v<MemoryType>
    NPGS_INLINE const ResourceType& TVulkanResourceMemory<ResourceType, MemoryType>::GetResource() const
    {
        return *Resource_;
    }

    template <typename ResourceType, typename MemoryType>
    requires std::is_class_v<ResourceType>&& std::is_class_v<MemoryType>
    NPGS_INLINE bool TVulkanResourceMemory<ResourceType, MemoryType>::IsValid() const
    {
        return Resource_ && Memory_ && Resource_->operator bool() && Memory_->operator bool();
    }

    // FVulkanDeviceMemory
    // -------------------
    NPGS_INLINE void FVulkanDeviceMemory::SetPersistentMapping(bool bFlag)
    {
        bPersistentlyMapped_ = bFlag;
    }

    // FVulkanBufferMemory
    // -------------------
    NPGS_INLINE vk::Result FVulkanBufferMemory::MapMemoryForSubmit(vk::DeviceSize Offset, vk::DeviceSize Size, void*& Target) const
    {
        return Memory_->MapMemoryForSubmit(Offset, Size, Target);
    }

    NPGS_INLINE vk::Result FVulkanBufferMemory::MapMemoryForFetch(vk::DeviceSize Offset, vk::DeviceSize Size, void*& Data) const
    {
        return Memory_->MapMemoryForFetch(Offset, Size, Data);
    }

    NPGS_INLINE vk::Result FVulkanBufferMemory::UnmapMemory(vk::DeviceSize Offset, vk::DeviceSize Size) const
    {
        return Memory_->UnmapMemory(Offset, Size);
    }

    NPGS_INLINE vk::Result FVulkanBufferMemory::SubmitBufferData(vk::DeviceSize MapOffset, vk::DeviceSize SubmitOffset,
                                                                 vk::DeviceSize Size, const void* Data) const
    {
        return Memory_->SubmitData(MapOffset, SubmitOffset, Size, Data);
    }

    NPGS_INLINE vk::Result FVulkanBufferMemory::FetchBufferData(vk::DeviceSize MapOffset, vk::DeviceSize FetchOffset,
                                                                vk::DeviceSize Size, void* Target) const
    {
        return Memory_->FetchData(MapOffset, FetchOffset, Size, Target);
    }

    template <typename ContainerType>
    requires std::is_class_v<ContainerType>
    NPGS_INLINE vk::Result FVulkanBufferMemory::SubmitBufferData(const ContainerType& Data) const
    {
        return Memory_->SubmitData(Data);
    }

    template <typename ContainerType>
    requires std::is_class_v<ContainerType>
    NPGS_INLINE vk::Result FVulkanBufferMemory::FetchBufferData(ContainerType& Data) const
    {
        return Memory_->FetchData(Data);
    }
} // namespace Npgs