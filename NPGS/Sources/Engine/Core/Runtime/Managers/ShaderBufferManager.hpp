#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "Engine/Core/Runtime/AssetLoaders/Shader.hpp"
#include "Engine/Core/Runtime/Graphics/Resources/DeviceLocalBuffer.hpp"
#include "Engine/Core/Runtime/Graphics/Vulkan/Context.hpp"
#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.hpp"
#include "Engine/Utils/Hash.hpp"

namespace Npgs
{
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
        std::uint32_t           Set{};
        std::uint32_t           Binding{};
        vk::DescriptorImageInfo Info;
    };

    struct FDescriptorBufferCreateInfo
    {
        std::string                                           Name;
        std::vector<std::string>                              UniformBufferNames;
        std::vector<std::string>                              StorageBufferNames;
        std::vector<FDescriptorSampler>                       SamplerInfos;
        std::vector<FDescriptorImageInfo>                     SampledImageInfos;
        std::vector<FDescriptorImageInfo>                     StorageImageInfos;
        std::vector<FDescriptorImageInfo>                     CombinedImageSamplerInfos;
        std::unordered_map<std::uint32_t, FDescriptorSetInfo> SetInfos;
    };

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

    private:
        struct FDataBufferFieldInfo
        {
            vk::DeviceSize Offset{};
            vk::DeviceSize Size{};
            vk::DeviceSize Alignment{};
        };

        struct FDataBufferInfo
        {
            std::unordered_map<std::string, FDataBufferFieldInfo,
                               Utils::FStringViewHeteroHash, Utils::FStringViewHeteroEqual> Fields;

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

        void RemoveDataBuffer(std::string_view Name);

        template <typename StructType>
        requires std::is_class_v<StructType>
        void UpdateDataBuffers(std::string_view Name, const StructType& Data) const;

        template <typename StructType>
        requires std::is_class_v<StructType>
        void UpdateDataBuffer(std::uint32_t FrameIndex, std::string_view Name, const StructType& Data) const;

        template <typename FieldType>
        std::vector<TUpdater<FieldType>> GetFieldUpdaters(std::string_view BufferName, std::string_view FieldName) const;

        template <typename FieldType>
        TUpdater<FieldType> GetFieldUpdater(std::uint32_t FrameIndex, std::string_view BufferName, std::string_view FieldName) const;

        const FDeviceLocalBuffer& GetDataBuffer(std::uint32_t FrameIndex, std::string_view BufferName) const;

        void CreateDescriptorBuffer(const FDescriptorBufferCreateInfo& DescriptorBufferCreateInfo,
                                    const VmaAllocationCreateInfo& AllocationCreateInfo);

        void RemoveDescriptorBuffer(std::string_view Name);

        // TODO
        // UpdateBufferDescriptor(s)
        // Complete UpdateResourceDescriptor(s) implementations
        template <typename Type>
        void UpdateResourceDescriptors(std::string_view BufferName, std::uint32_t Set, std::uint32_t Binding,
                                       vk::DescriptorType ConfirmedUsage, const Type& DescriptorInfo);

        template <typename Type>
        void UpdateResourceDescriptor(std::uint32_t FrameIndex, std::string_view BufferName, std::uint32_t Set,
                                      std::uint32_t Binding, vk::DescriptorType ConfirmedUsage, const Type& DescriptorInfo);

        template <typename... Args>
        std::vector<vk::DeviceSize> GetDescriptorBindingOffsets(std::string_view BufferName, Args... Sets) const;

        vk::DeviceSize GetDescriptorBindingOffset(std::string_view BufferName, std::uint32_t Set, std::uint32_t Binding) const;
        const FDeviceLocalBuffer& GetDescriptorBuffer(std::uint32_t FrameIndex, std::string_view BufferName) const;

    private:
        const FDataBufferInfo& GetDataBufferInfo(std::string_view BufferName) const;

        std::pair<vk::DeviceSize, vk::DeviceSize> 
        GetDataBufferFieldOffsetAndSize(const FDataBufferInfo& BufferInfo, std::string_view FieldName) const;

        const FDescriptorBufferInfo& GetDescriptorBufferInfo(std::string_view BufferName) const;

        std::pair<vk::DeviceSize, vk::DescriptorType>
        GetDescriptorBindingOffsetAndType(std::string_view BufferName, std::uint32_t Set, std::uint32_t Binding) const;
        
        vk::DeviceSize GetDescriptorSize(vk::DescriptorType Usage) const;
        vk::DeviceSize CalculateDescriptorBufferSize(const FDescriptorBufferCreateInfo& DescriptorBufferCreateInfo);
        void BindResourceToDescriptorBuffersInternal(const FDescriptorBufferCreateInfo& DescriptorBufferCreateInfo);

    private:
        FVulkanContext* VulkanContext_;

        vk::PhysicalDeviceDescriptorBufferPropertiesEXT DescriptorBufferProperties_;
        // [DataBufferName, DataBuffer]
        std::unordered_map<std::string, FDataBufferInfo, Utils::FStringViewHeteroHash, Utils::FStringViewHeteroEqual> DataBuffers_;
        // [DescriptorBufferName, DescriptorBuffer]
        std::unordered_map<std::string, FDescriptorBufferInfo, Utils::FStringViewHeteroHash, Utils::FStringViewHeteroEqual> DescriptorBuffers_;
        // [DescriptorBufferName, [[Set, Binding], [Offset, Type]]]
        using FSubMap = std::unordered_map<std::pair<std::uint32_t, std::uint32_t>, std::pair<vk::DeviceSize, vk::DescriptorType>, FSetBindingHash>;
        std::unordered_map<std::string, FSubMap, Utils::FStringViewHeteroHash, Utils::FStringViewHeteroEqual> OffsetsMap_;
        // [DescriptorBuffername, [Set, BaseOffset]]
        std::unordered_map<std::string, std::unordered_map<std::uint32_t, vk::DeviceSize>,
                           Utils::FStringViewHeteroHash, Utils::FStringViewHeteroEqual> SetBaseOffsetsMap_;

        VmaAllocator Allocator_;
    };
} // namespace Npgs

#include "ShaderBufferManager.inl"
