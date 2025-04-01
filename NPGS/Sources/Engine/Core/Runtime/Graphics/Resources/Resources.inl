#include "Resources.h"

#include "Engine/Core/Base/Assert.h"
#include "Engine/Core/Base/Base.h"

namespace Npgs
{
    NPGS_INLINE vk::DescriptorImageInfo FAttachment::CreateDescriptorImageInfo(const FVulkanSampler& Sampler) const
    {
        return { *Sampler, **_ImageView, vk::ImageLayout::eShaderReadOnlyOptimal };
    }

    NPGS_INLINE vk::DescriptorImageInfo FAttachment::CreateDescriptorImageInfo(const vk::Sampler& Sampler) const
    {
        return { Sampler, **_ImageView, vk::ImageLayout::eShaderReadOnlyOptimal };
    }

    NPGS_INLINE FVulkanImage& FAttachment::GetImage()
    {
        return _ImageMemory->GetResource();
    }

    NPGS_INLINE const FVulkanImage& FAttachment::GetImage() const
    {
        return _ImageMemory->GetResource();
    }

    NPGS_INLINE FVulkanImageView& FAttachment::GetImageView()
    {
        return *_ImageView;
    }

    NPGS_INLINE const FVulkanImageView& FAttachment::GetImageView() const
    {
        return *_ImageView;
    }

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

    NPGS_INLINE bool FStagingBuffer::IsUsingVma() const
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
