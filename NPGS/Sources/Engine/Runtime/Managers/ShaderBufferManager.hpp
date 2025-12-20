#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <ankerl/unordered_dense.h>
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "Engine/Core/Utils/Hash.hpp"
#include "Engine/Runtime/AssetLoaders/Shader.hpp"
#include "Engine/Runtime/Graphics/Resources/DeviceLocalBuffer.hpp"
#include "Engine/Runtime/Graphics/Vulkan/Context.hpp"
#include "Engine/Runtime/Graphics/Vulkan/Wrappers.hpp"

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
        std::string                                                     Name;
        std::vector<std::string>                                        UniformBufferNames;
        std::vector<std::string>                                        StorageBufferNames;
        std::vector<FDescriptorSampler>                                 SamplerInfos;
        std::vector<FDescriptorImageInfo>                               SampledImageInfos;
        std::vector<FDescriptorImageInfo>                               StorageImageInfos;
        std::vector<FDescriptorImageInfo>                               CombinedImageSamplerInfos;
        ankerl::unordered_dense::map<std::uint32_t, FDescriptorSetInfo> SetInfos;
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

    public:
        FShaderBufferManager(FVulkanContext* VulkanContext);
        FShaderBufferManager(const FShaderBufferManager&) = delete;
        FShaderBufferManager(FShaderBufferManager&&)      = delete;
        ~FShaderBufferManager();

        FShaderBufferManager& operator=(const FShaderBufferManager&) = delete;
        FShaderBufferManager& operator=(FShaderBufferManager&&)      = delete;

        template <typename StructType>
        requires std::is_class_v<StructType>
        void AllocateDataBuffers(const FDataBufferCreateInfo& DataBufferCreateInfo);

        void FreeDataBuffer(std::string_view Name);

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

        vk::DeviceSize GetDataBufferDeviceAddress(std::uint32_t FrameIndex, std::string_view BufferName) const;
        void AllocateDescriptorBuffer(const FDescriptorBufferCreateInfo& DescriptorBufferCreateInfo);
        void FreeDescriptorBuffer(std::string_view Name);

        template <typename... Args>
        std::vector<vk::DeviceSize> GetDescriptorBindingOffsets(std::string_view BufferName, Args... Sets) const;

        vk::DeviceSize GetDescriptorBindingOffset(std::string_view BufferName, std::uint32_t Set) const;
        const FDeviceLocalBuffer& GetResourceDescriptorHeap(std::uint32_t FrameIndex) const;
        const FDeviceLocalBuffer& GetSamplerDescriptorHeap(std::uint32_t FrameIndex) const;
        vk::DescriptorBufferBindingInfoEXT GetResourceHeapBindingInfo(std::uint32_t FrameIndex) const;
        vk::DescriptorBufferBindingInfoEXT GetSamplerHeapBindingInfo(std::uint32_t FrameIndex) const;

    private:
        struct FDataBufferFieldInfo
        {
            vk::DeviceSize Offset{};
            vk::DeviceSize Size{};
            vk::DeviceSize Alignment{};
        };

        struct FDataBufferInfo
        {
            Utils::TStringHeteroHashTable<std::string, FDataBufferFieldInfo> Fields;
            FDataBufferCreateInfo CreateInfo;
            vk::DeviceSize        Offset{};
            vk::DeviceSize        Size{};
            vk::DescriptorType    Type{ vk::DescriptorType::eUniformBuffer };
        };

        enum class EHeapType
        {
            kResource, kSampler
        };

        struct FDescriptorBufferInfo
        {
            struct FSetAllocation
            {
                EHeapType      HeapType{ EHeapType::kSampler };
                vk::DeviceSize Offset{};
                vk::DeviceSize Size{};
            };

            std::string                                                 Name;
            ankerl::unordered_dense::map<std::uint32_t, FSetAllocation> SetAllocations;
        };

        struct FSetBindingHash
        {
            std::size_t operator()(const std::pair<std::uint32_t, std::uint32_t>& SetBinding) const noexcept;
        };

        class FHeapAllocator
        {
        public:
            void Initialize(vk::DeviceSize TotalSize);
            vk::DeviceSize Allocate(vk::DeviceSize Size, vk::DeviceSize Alignment);
            void Free(vk::DeviceSize Offset, vk::DeviceSize Size);
            void Reset();

        private:
            vk::DeviceSize TotalSize_{};
            // [BlockOffset, BlockSize]
            std::map<vk::DeviceSize, vk::DeviceSize> FreeBlocks_;
        };

    private:
        void InitializeHeaps();
        const FDataBufferInfo& GetDataBufferInfo(std::string_view BufferName) const;

        std::pair<vk::DeviceSize, vk::DeviceSize> 
        GetDataBufferFieldOffsetAndSize(const FDataBufferInfo& BufferInfo, std::string_view FieldName) const;

        const FDescriptorBufferInfo& GetDescriptorBufferInfo(std::string_view BufferName) const;        
        vk::DeviceSize GetDescriptorSize(vk::DescriptorType Usage) const;
        vk::DeviceSize CalculateDescriptorBufferSize(const FDescriptorBufferCreateInfo& DescriptorBufferCreateInfo);
        void BindResourceToDescriptorBuffersInternal(const FDescriptorBufferCreateInfo& DescriptorBufferCreateInfo);

    private:
        FVulkanContext* VulkanContext_;

        vk::PhysicalDeviceDescriptorBufferPropertiesEXT DescriptorBufferProperties_;
        // [DataBufferName, DataBuffer]
        Utils::TStringHeteroHashTable<std::string, FDataBufferInfo> DataBuffers_;
        // [DescriptorBufferName, DescriptorBuffer]
        Utils::TStringHeteroHashTable<std::string, FDescriptorBufferInfo> DescriptorBuffers_;
        
        std::vector<FDeviceLocalBuffer> ResourceDescriptorHeaps_;
        std::vector<FDeviceLocalBuffer> SamplerDescriptorHeaps_;
        std::vector<FDeviceLocalBuffer> UniformDataHeaps_;
        std::vector<FDeviceLocalBuffer> StorageDataHeaps_;
        
        VmaAllocator   Allocator_;
        FHeapAllocator ResourceHeapAllocator_;
        FHeapAllocator SamplerHeapAllocator_;
        FHeapAllocator UniformHeapAllocator_;
        FHeapAllocator StorageHeapAllocator_;
    };
} // namespace Npgs

#include "ShaderBufferManager.inl"
