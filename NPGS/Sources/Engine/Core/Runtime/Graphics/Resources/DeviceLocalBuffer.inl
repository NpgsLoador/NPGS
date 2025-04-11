#include "DeviceLocalBuffer.h"

#include "Engine/Core/Base/Base.h"

namespace Npgs
{
    NPGS_INLINE FDeviceLocalBuffer::operator FVulkanBuffer& ()
    {
        return _BufferMemory->GetResource();
    }

    NPGS_INLINE FDeviceLocalBuffer::operator const FVulkanBuffer& () const
    {
        return _BufferMemory->GetResource();
    }

    NPGS_INLINE void FDeviceLocalBuffer::UpdateData(const FVulkanCommandBuffer& CommandBuffer, vk::DeviceSize Offset,
                                                    vk::DeviceSize Size, const void* Data) const
    {
        CommandBuffer->updateBuffer(*_BufferMemory->GetResource(), Offset, Size, Data);
    }

    template <typename ContainerType>
    requires std::is_class_v<ContainerType>
    NPGS_INLINE void FDeviceLocalBuffer::CopyData(const ContainerType& Data) const
    {
        using ValueType = typename ContainerType::value_type;
        static_assert(std::is_standard_layout_v<ValueType>, "Container value_type must be standard layout type");

        vk::DeviceSize DataSize = Data.size() * sizeof(ValueType);
        return CopyData(0, 0, DataSize, static_cast<const void*>(Data.data()));
    }

    template <typename ContainerType>
    requires std::is_class_v<ContainerType>
    NPGS_INLINE void FDeviceLocalBuffer::UpdateData(const FVulkanCommandBuffer& CommandBuffer, const ContainerType& Data) const
    {
        using ValueType = typename ContainerType::value_type;
        static_assert(std::is_standard_layout_v<ValueType>, "Container value_type must be standard layout type");

        vk::DeviceSize DataSize = Data.size() * sizeof(ValueType);
        return UpdateData(CommandBuffer, 0, DataSize, static_cast<const void*>(Data.data()));
    }

    NPGS_INLINE void FDeviceLocalBuffer::EnablePersistentMapping() const
    {
        _BufferMemory->GetMemory().SetPersistentMapping(true);
    }

    NPGS_INLINE void FDeviceLocalBuffer::DisablePersistentMapping() const
    {
        _BufferMemory->GetMemory().SetPersistentMapping(false);
    }

    NPGS_INLINE FVulkanBuffer& FDeviceLocalBuffer::GetBuffer()
    {
        return _BufferMemory->GetResource();
    }

    NPGS_INLINE const FVulkanBuffer& FDeviceLocalBuffer::GetBuffer() const
    {
        return _BufferMemory->GetResource();
    }

    NPGS_INLINE FVulkanDeviceMemory& FDeviceLocalBuffer::GetMemory()
    {
        return _BufferMemory->GetMemory();
    }

    NPGS_INLINE const FVulkanDeviceMemory& FDeviceLocalBuffer::GetMemory() const
    {
        return _BufferMemory->GetMemory();
    }

    NPGS_INLINE bool FDeviceLocalBuffer::IsUsingVma() const
    {
        return _Allocator != nullptr;
    }
} // namespace Npgs
