#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "Engine/Runtime/Graphics/Vulkan/Context.hpp"
#include "Engine/Runtime/Graphics/Vulkan/Wrappers.hpp"

namespace Npgs
{
    class FDeviceLocalBuffer
    {
    public:
        FDeviceLocalBuffer(FVulkanContext* VulkanContext, std::string_view Name, VmaAllocator Allocator,
                           const VmaAllocationCreateInfo& AllocationCreateInfo,
                           const vk::BufferCreateInfo& BufferCreateInfo);

        FDeviceLocalBuffer(const FDeviceLocalBuffer&) = delete;
        FDeviceLocalBuffer(FDeviceLocalBuffer&& Other) noexcept;
        ~FDeviceLocalBuffer() = default;

        FDeviceLocalBuffer& operator=(const FDeviceLocalBuffer&) = delete;
        FDeviceLocalBuffer& operator=(FDeviceLocalBuffer&& Other) noexcept;

        operator FVulkanBuffer& ();
        operator const FVulkanBuffer& () const;

        void CopyData(vk::DeviceSize MapOffset, vk::DeviceSize TargetOffset, vk::DeviceSize Size, const void* Data) const;

        void CopyData(vk::DeviceSize ElementIndex, vk::DeviceSize ElementCount, vk::DeviceSize ElementSize,
                      vk::DeviceSize SrcStride, vk::DeviceSize DstStride, vk::DeviceSize MapOffset, const void* Data) const;

        template <typename ContainerType>
        requires std::is_class_v<ContainerType>
        void CopyData(const ContainerType& Data) const;

        void UpdateData(const FVulkanCommandBuffer& CommandBuffer, vk::DeviceSize Offset, vk::DeviceSize Size, const void* Data) const;

        template <typename ContainerType>
        requires std::is_class_v<ContainerType>
        void UpdateData(const FVulkanCommandBuffer& CommandBuffer, const ContainerType& Data) const;

        void SetPersistentMapping(bool bFlag) const;

        FVulkanBuffer& GetBuffer();
        const FVulkanBuffer& GetBuffer() const;
        FVulkanDeviceMemory& GetMemory();
        const FVulkanDeviceMemory& GetMemory() const;

    private:
        vk::Result CreateBuffer(const VmaAllocationCreateInfo& AllocationCreateInfo,
                                const vk::BufferCreateInfo& BufferCreateInfo);

        vk::Result RecreateBuffer(const VmaAllocationCreateInfo& AllocationCreateInfo,
                                  const vk::BufferCreateInfo& BufferCreateInfo);

    private:
        FVulkanContext*                      VulkanContext_;
        VmaAllocator                         Allocator_;
        std::unique_ptr<FVulkanBufferMemory> BufferMemory_;
        std::string                          Name_;
    };
} // namespace Npgs

#include "DeviceLocalBuffer.inl"
