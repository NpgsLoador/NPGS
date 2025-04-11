#include "StagingBuffer.h"

#include "Engine/Core/Base/Base.h"

namespace Npgs
{
    NPGS_INLINE FStagingBuffer::operator FVulkanBuffer& ()
    {
        return _BufferMemory->GetResource();
    }

    NPGS_INLINE FStagingBuffer::operator const FVulkanBuffer& () const
    {
        return _BufferMemory->GetResource();
    }

    NPGS_INLINE FStagingBuffer::operator FVulkanDeviceMemory& ()
    {
        return _BufferMemory->GetMemory();
    }

    NPGS_INLINE FStagingBuffer::operator const FVulkanDeviceMemory& () const
    {
        return _BufferMemory->GetMemory();
    }

    NPGS_INLINE void* FStagingBuffer::MapMemory(vk::DeviceSize Size)
    {
        Expand(Size);
        void* Target = nullptr;
        _BufferMemory->MapMemoryForSubmit(0, Size, Target);
        _MemoryUsage = Size;
        return Target;
    }

    NPGS_INLINE void FStagingBuffer::UnmapMemory()
    {
        _BufferMemory->UnmapMemory(0, _MemoryUsage);
        _MemoryUsage = 0;
    }

    NPGS_INLINE void FStagingBuffer::SubmitBufferData(vk::DeviceSize MapOffset, vk::DeviceSize SubmitOffset, vk::DeviceSize Size, const void* Data)
    {
        Expand(Size);
        _BufferMemory->SubmitBufferData(MapOffset, SubmitOffset, Size, Data);
    }

    NPGS_INLINE void FStagingBuffer::FetchBufferData(vk::DeviceSize MapOffset, vk::DeviceSize FetchOffset, vk::DeviceSize Size, void* Target) const
    {
        _BufferMemory->FetchBufferData(MapOffset, FetchOffset, Size, Target);
    }

    NPGS_INLINE void FStagingBuffer::Release()
    {
        _BufferMemory.reset();
    }

    NPGS_INLINE bool FStagingBuffer::AllocatedByVma() const
    {
        return _Allocator != nullptr;
    }

    NPGS_INLINE FVulkanBuffer& FStagingBuffer::GetBuffer()
    {
        return _BufferMemory->GetResource();
    }

    NPGS_INLINE const FVulkanBuffer& FStagingBuffer::GetBuffer() const
    {
        return _BufferMemory->GetResource();
    }

    NPGS_INLINE FVulkanImage& FStagingBuffer::GetImage()
    {
        return *_AliasedImage;
    }

    NPGS_INLINE const FVulkanImage& FStagingBuffer::GetImage() const
    {
        return *_AliasedImage;
    }

    NPGS_INLINE FVulkanDeviceMemory& FStagingBuffer::GetMemory()
    {
        return _BufferMemory->GetMemory();
    }

    NPGS_INLINE const FVulkanDeviceMemory& FStagingBuffer::GetMemory() const
    {
        return _BufferMemory->GetMemory();
    }
} // namespace Npgs
