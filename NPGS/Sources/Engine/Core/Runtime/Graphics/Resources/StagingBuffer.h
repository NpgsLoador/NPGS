#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <type_traits>
#include <vector>

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"

namespace Npgs
{
    class FStagingBuffer
    {
    public:
        FStagingBuffer() = delete;
        FStagingBuffer(vk::DeviceSize Size);
        FStagingBuffer(vk::Device Device, const vk::PhysicalDeviceProperties& PhysicalDeviceProperties,
                       const vk::PhysicalDeviceMemoryProperties& PhysicalDeviceMemoryProperties, vk::DeviceSize Size);

        FStagingBuffer(const VmaAllocationCreateInfo& AllocationCreateInfo, const vk::BufferCreateInfo& BufferCreateInfo);
        FStagingBuffer(VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo,
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
        bool  AllocatedByVma() const;

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
        vk::Device                                _Device;
        const vk::PhysicalDeviceProperties*       _PhysicalDeviceProperties;
        const vk::PhysicalDeviceMemoryProperties* _PhysicalDeviceMemoryProperties;
        std::unique_ptr<FVulkanBufferMemory>      _BufferMemory;
        std::unique_ptr<FVulkanImage>             _AliasedImage;
        vk::DeviceSize                            _MemoryUsage;
        VmaAllocator                              _Allocator;
        VmaAllocationCreateInfo                   _AllocationCreateInfo;
    };
} // namespace Npgs

#include "StagingBuffer.inl"
