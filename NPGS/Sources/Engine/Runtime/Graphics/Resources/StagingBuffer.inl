#include "StagingBuffer.hpp"
#include "Engine/Core/Base/Base.hpp"

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

    NPGS_INLINE void FStagingBuffer::SubmitBufferData(vk::DeviceSize MapOffset, vk::DeviceSize SubmitOffset, vk::DeviceSize Size, const void* Data)
    {
        BufferMemory_->SubmitBufferData(MapOffset, SubmitOffset, Size, Data);
    }

    NPGS_INLINE void FStagingBuffer::FetchBufferData(vk::DeviceSize MapOffset, vk::DeviceSize FetchOffset, vk::DeviceSize Size, void* Target) const
    {
        BufferMemory_->FetchBufferData(MapOffset, FetchOffset, Size, Target);
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
