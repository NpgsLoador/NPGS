#include "Engine/Core/Base/Base.h"

namespace Npgs
{
    NPGS_INLINE FDeviceLocalBuffer::operator FVulkanBuffer& ()
    {
        return BufferMemory_->GetResource();
    }

    NPGS_INLINE FDeviceLocalBuffer::operator const FVulkanBuffer& () const
    {
        return BufferMemory_->GetResource();
    }

    NPGS_INLINE void FDeviceLocalBuffer::UpdateData(const FVulkanCommandBuffer& CommandBuffer, vk::DeviceSize Offset,
                                                    vk::DeviceSize Size, const void* Data) const
    {
        CommandBuffer->updateBuffer(*BufferMemory_->GetResource(), Offset, Size, Data);
    }

    template <typename ContainerType>
    requires std::is_class_v<ContainerType>
    void FDeviceLocalBuffer::CopyData(const ContainerType& Data) const
    {
        using ValueType = typename ContainerType::value_type;
        static_assert(std::is_standard_layout_v<ValueType>, "Container value_type must be standard layout type");

        vk::DeviceSize DataSize = Data.size() * sizeof(ValueType);
        return CopyData(0, 0, DataSize, static_cast<const void*>(Data.data()));
    }

    template <typename ContainerType>
    requires std::is_class_v<ContainerType>
    void FDeviceLocalBuffer::UpdateData(const FVulkanCommandBuffer& CommandBuffer, const ContainerType& Data) const
    {
        using ValueType = typename ContainerType::value_type;
        static_assert(std::is_standard_layout_v<ValueType>, "Container value_type must be standard layout type");

        vk::DeviceSize DataSize = Data.size() * sizeof(ValueType);
        return UpdateData(CommandBuffer, 0, DataSize, static_cast<const void*>(Data.data()));
    }

    NPGS_INLINE void FDeviceLocalBuffer::EnablePersistentMapping() const
    {
        BufferMemory_->GetMemory().SetPersistentMapping(true);
    }

    NPGS_INLINE void FDeviceLocalBuffer::DisablePersistentMapping() const
    {
        BufferMemory_->GetMemory().SetPersistentMapping(false);
    }

    NPGS_INLINE FVulkanBuffer& FDeviceLocalBuffer::GetBuffer()
    {
        return BufferMemory_->GetResource();
    }

    NPGS_INLINE const FVulkanBuffer& FDeviceLocalBuffer::GetBuffer() const
    {
        return BufferMemory_->GetResource();
    }

    NPGS_INLINE FVulkanDeviceMemory& FDeviceLocalBuffer::GetMemory()
    {
        return BufferMemory_->GetMemory();
    }

    NPGS_INLINE const FVulkanDeviceMemory& FDeviceLocalBuffer::GetMemory() const
    {
        return BufferMemory_->GetMemory();
    }

    NPGS_INLINE bool FDeviceLocalBuffer::IsUsingVma() const
    {
        return Allocator_ != nullptr;
    }
} // namespace Npgs
