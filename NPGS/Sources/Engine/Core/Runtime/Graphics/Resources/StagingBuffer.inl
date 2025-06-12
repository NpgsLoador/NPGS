#include "Engine/Core/Base/Base.h"

namespace Npgs
{
    NPGS_INLINE FStagingBuffer::operator FVulkanBuffer& ()
    {
        return BufferMemory_->GetResource();
    }

    NPGS_INLINE FStagingBuffer::operator const FVulkanBuffer& () const
    {
        return BufferMemory_->GetResource();
    }

    NPGS_INLINE FStagingBuffer::operator FVulkanDeviceMemory& ()
    {
        return BufferMemory_->GetMemory();
    }

    NPGS_INLINE FStagingBuffer::operator const FVulkanDeviceMemory& () const
    {
        return BufferMemory_->GetMemory();
    }

    inline void* FStagingBuffer::MapMemory(vk::DeviceSize Size)
    {
        Expand(Size);
        void* Target = nullptr;
        BufferMemory_->MapMemoryForSubmit(0, Size, Target);
        MemoryUsage_ = Size;
        return Target;
    }

    inline void FStagingBuffer::UnmapMemory()
    {
        BufferMemory_->UnmapMemory(0, MemoryUsage_);
        MemoryUsage_ = 0;
    }

    inline void FStagingBuffer::SubmitBufferData(vk::DeviceSize MapOffset, vk::DeviceSize SubmitOffset, vk::DeviceSize Size, const void* Data)
    {
        Expand(Size);
        BufferMemory_->SubmitBufferData(MapOffset, SubmitOffset, Size, Data);
    }

    NPGS_INLINE void FStagingBuffer::FetchBufferData(vk::DeviceSize MapOffset, vk::DeviceSize FetchOffset, vk::DeviceSize Size, void* Target) const
    {
        BufferMemory_->FetchBufferData(MapOffset, FetchOffset, Size, Target);
    }

    NPGS_INLINE void FStagingBuffer::Release()
    {
        BufferMemory_.reset();
    }

    NPGS_INLINE FVulkanBuffer& FStagingBuffer::GetBuffer()
    {
        return BufferMemory_->GetResource();
    }

    NPGS_INLINE const FVulkanBuffer& FStagingBuffer::GetBuffer() const
    {
        return BufferMemory_->GetResource();
    }

    NPGS_INLINE FVulkanImage& FStagingBuffer::GetImage()
    {
        return *AliasedImage_;
    }

    NPGS_INLINE const FVulkanImage& FStagingBuffer::GetImage() const
    {
        return *AliasedImage_;
    }

    NPGS_INLINE FVulkanDeviceMemory& FStagingBuffer::GetMemory()
    {
        return BufferMemory_->GetMemory();
    }

    NPGS_INLINE const FVulkanDeviceMemory& FStagingBuffer::GetMemory() const
    {
        return BufferMemory_->GetMemory();
    }
} // namespace Npgs
