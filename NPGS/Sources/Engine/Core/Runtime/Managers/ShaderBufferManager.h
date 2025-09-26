#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "Engine/Core/Runtime/Graphics/Resources/DeviceLocalBuffer.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"

namespace Npgs
{
    class FShaderBufferManager
    {
    public:
        template <typename DataType>
        class TUpdater
        {
        public:
            TUpdater(const FDeviceLocalBuffer& Buffer, vk::DeviceSize Offset, vk::DeviceSize Size);
            const TUpdater& operator<<(const DataType& Data) const;
            void Submit(const DataType& Data) const;

        private:
            const FDeviceLocalBuffer* Buffer_;
            vk::DeviceSize            Offset_;
            vk::DeviceSize            Size_;
        };

        struct FDataBufferCreateInfo
        {
            std::string              Name;
            std::vector<std::string> Fields;
            std::uint32_t            Set{};
            std::uint32_t            Binding{};
            vk::DescriptorType       Usage{};
        };

        struct FDescriptorSampler
        {
            std::uint32_t Set{};
            std::uint32_t Binding{};
            vk::Sampler   Sampler{};
        };

        struct FDescriptorImageInfo
        {
            std::uint32_t Set{};
            std::uint32_t Binding{};
            vk::DescriptorImageInfo Info;
        };

        struct FDescriptorBufferCreateInfo
        {
            std::string                             Name;
            std::vector<std::string>                UniformBufferNames;
            std::vector<std::string>                StorageBufferNames;
            std::vector<FDescriptorSampler>         SamplerInfos;
            std::vector<FDescriptorImageInfo>       SampledImageInfos;
            std::vector<FDescriptorImageInfo>       StorageImageInfos;
            std::vector<FDescriptorImageInfo>       CombinedImageSamplerInfos;
            std::map<std::uint32_t, vk::DeviceSize> SetSizes;
        };

    private:
        struct FDataBufferFieldInfo
        {
            vk::DeviceSize Offset{};
            vk::DeviceSize Size{};
            vk::DeviceSize Alignment{};
        };

        struct FDataBufferInfo
        {
            std::unordered_map<std::string, FDataBufferFieldInfo> Fields;
            std::unordered_map<std::string, vk::DeviceSize> BoundShaders; // [ShaderName, Range]
            std::vector<FDeviceLocalBuffer> Buffers;
            FDataBufferCreateInfo           CreateInfo;
            vk::DeviceSize                  Size{};
        };

        struct FDescriptorBufferInfo
        {
            std::string                     Name;
            std::vector<FDeviceLocalBuffer> Buffers;
            vk::DeviceSize                  Size{};
        };

        struct FSetBindingHash
        {
            std::size_t operator()(const std::pair<std::uint32_t, std::uint32_t>& SetBinding) const noexcept;
        };

    public:
        FShaderBufferManager(FVulkanContext* VulkanContext);
        FShaderBufferManager(const FShaderBufferManager&) = delete;
        FShaderBufferManager(FShaderBufferManager&&)      = delete;
        ~FShaderBufferManager();

        FShaderBufferManager& operator=(const FShaderBufferManager&) = delete;
        FShaderBufferManager& operator=(FShaderBufferManager&&)      = delete;

        template <typename StructType>
        requires std::is_class_v<StructType>
        void CreateDataBuffers(const FDataBufferCreateInfo& DataBufferCreateInfo,
                               const VmaAllocationCreateInfo& AllocationCreateInfo,
                               std::uint32_t BufferCount = 0);

        void RemoveDataBuffer(const std::string& Name);

        template <typename StructType>
        requires std::is_class_v<StructType>
        void UpdateDataBuffers(const std::string& Name, const StructType& Data);

        template <typename StructType>
        requires std::is_class_v<StructType>
        void UpdateDataBuffer(std::uint32_t FrameIndex, const std::string& Name, const StructType& Data);

        template <typename FieldType>
        std::vector<TUpdater<FieldType>> GetFieldUpdaters(const std::string& BufferName, const std::string& FieldName) const;

        template <typename FieldType>
        TUpdater<FieldType> GetFieldUpdater(std::uint32_t FrameIndex, const std::string& BufferName, const std::string& FieldName) const;

        const FDeviceLocalBuffer& GetDataBuffer(std::uint32_t FrameIndex, const std::string& BufferName);

        void CreateDescriptorBuffer(const FDescriptorBufferCreateInfo& DescriptorBufferCreateInfo,
                                    const VmaAllocationCreateInfo& AllocationCreateInfo);

        void RemoveDescriptorBuffer(const std::string& Name);
        vk::DeviceSize GetDescriptorBindingOffset(const std::string& BufferName, std::uint32_t Set, std::uint32_t Binding) const;

        template <typename... Args>
        std::vector<vk::DeviceSize> GetDescriptorBindingOffsets(const std::string& BufferName, Args... Sets);

        const FDeviceLocalBuffer& GetDescriptorBuffer(std::uint32_t FrameIndex, const std::string& BufferName);

    private:
        vk::DeviceSize CalculateDescriptorBufferSize(const FDescriptorBufferCreateInfo& DescriptorBufferCreateInfo);
        void BindResourceToDescriptorBuffersInternal(const FDescriptorBufferCreateInfo& DescriptorBufferCreateInfo);

    private:
        FVulkanContext* VulkanContext_;

        vk::PhysicalDeviceDescriptorBufferPropertiesEXT DescriptorBufferProperties_;
        // [Name, Buffer]
        std::unordered_map<std::string, FDataBufferInfo>       DataBuffers_;
        std::unordered_map<std::string, FDescriptorBufferInfo> DescriptorBuffers_;
        // [Name, [[Set, Binding], Offset]]
        std::unordered_map<std::string, std::unordered_map<std::pair<std::uint32_t, std::uint32_t>, vk::DeviceSize, FSetBindingHash>> OffsetsMap_;

        VmaAllocator Allocator_;
    };
} // namespace Npgs

#include "ShaderBufferManager.inl"
