#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <type_traits>
#include <vector>

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.hpp"

namespace Npgs
{
    class FStagingBuffer
    {
    public:
        FStagingBuffer(vk::PhysicalDevice, vk::Device Device, VmaAllocator Allocator,
                       const VmaAllocationCreateInfo& AllocationCreateInfo,
                       const vk::BufferCreateInfo& BufferCreateInfo);

        FStagingBuffer(const FStagingBuffer&) = delete;
        FStagingBuffer(FStagingBuffer&& Other) noexcept;
        ~FStagingBuffer() = default;

        FStagingBuffer& operator=(const FStagingBuffer&) = delete;
        FStagingBuffer& operator=(FStagingBuffer&& Other) noexcept;

        operator FVulkanBuffer& ();
        operator const FVulkanBuffer& () const;
        operator FVulkanDeviceMemory& ();
        operator const FVulkanDeviceMemory& () const;

        void* MapMemory(vk::DeviceSize Size);
        void  UnmapMemory();
        void  SubmitBufferData(vk::DeviceSize MapOffset, vk::DeviceSize SubmitOffset, vk::DeviceSize Size, const void* Data);
        void  FetchBufferData(vk::DeviceSize MapOffset, vk::DeviceSize FetchOffset, vk::DeviceSize Size, void* Target) const;
        void  Release();

        FVulkanImage* CreateAliasedImage(vk::Format OriginFormat, const vk::ImageCreateInfo& ImageCreateInfo);

        FVulkanBuffer& GetBuffer();
        const FVulkanBuffer& GetBuffer() const;
        FVulkanImage& GetImage();
        const FVulkanImage& GetImage() const;
        FVulkanDeviceMemory& GetMemory();
        const FVulkanDeviceMemory& GetMemory() const;

    private:
        void Expand(vk::DeviceSize Size);

    private:
        vk::PhysicalDevice                   PhysicalDevice_;
        vk::Device                           Device_;
        std::unique_ptr<FVulkanBufferMemory> BufferMemory_;
        std::unique_ptr<FVulkanImage>        AliasedImage_;
        vk::DeviceSize                       MemoryUsage_;
        VmaAllocator                         Allocator_;
        VmaAllocationCreateInfo              AllocationCreateInfo_;
    };
} // namespace Npgs

#include "StagingBuffer.inl"
