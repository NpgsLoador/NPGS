#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "Engine/Core/Base/Base.h"
#include "Engine/Core/Runtime/Graphics/Resources/Resources.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

class FShaderBufferManager
{
public:
    template <typename DataType>
    class TUpdater
    {
    public:
        TUpdater(const FDeviceLocalBuffer& Buffer, vk::DeviceSize Offset, vk::DeviceSize Size)
            : _Buffer(&Buffer), _Offset(Offset), _Size(Size)
        {
        }

        const TUpdater& operator<<(const DataType& Data) const
        {
            Submit(Data);
            return *this;
        }

        void Submit(const DataType& Data) const
        {
            _Buffer->CopyData(0, _Offset, _Size, &Data);
        }

    private:
        const Graphics::FDeviceLocalBuffer* _Buffer;
        vk::DeviceSize                      _Offset;
        vk::DeviceSize                      _Size;
    };

    struct FDataBufferCreateInfo
    {
        std::string              Name;
        std::vector<std::string> Fields;
        std::uint32_t            Set{};
        std::uint32_t            Binding{};
        vk::DescriptorType       Usage{};
    };

    struct FDescriptorImageInfo
    {
        std::uint32_t Set{};
        std::uint32_t Binding{};
        vk::DescriptorImageInfo Info;
    };

    struct FDescriptorBufferCreateInfo
    {
        std::string Name;
        std::vector<std::string> UniformBuffers;
        std::vector<std::string> StorageBuffers;
        std::vector<FDescriptorImageInfo> SamplerInfos;
        std::vector<FDescriptorImageInfo> SampledImageInfos;
        std::vector<FDescriptorImageInfo> StorageImageInfos;
        std::vector<FDescriptorImageInfo> CombinedImageSamplerInfos;
        std::unordered_map<std::uint32_t, vk::DeviceSize> SetSizes;
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
        std::size_t operator()(const std::pair<std::uint32_t, std::uint32_t>& SetBinding) const noexcept
        {
            return std::hash<std::uint32_t>()(SetBinding.first) ^ std::hash<std::uint32_t>()(SetBinding.second);
        }
    };

public:
    void SetCustomVmaAllocator(VmaAllocator Allocator);
    void RestoreDefaultVmaAllocator();

    template <typename StructType>
    requires std::is_class_v<StructType>
    void CreateDataBuffers(const FDataBufferCreateInfo& DataBufferCreateInfo,
                           const VmaAllocationCreateInfo* AllocationCreateInfo = nullptr,
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

    const Graphics::FDeviceLocalBuffer& GetDataBuffer(std::uint32_t FrameIndex, const std::string& BufferName);

    void CreateDescriptorBuffer(const FDescriptorBufferCreateInfo& DescriptorBufferCreateInfo,
                                const VmaAllocationCreateInfo* AllocationCreateInfo = nullptr);

    void RemoveDescriptorBuffer(const std::string& Name);
    vk::DeviceSize GetDescriptorBindingOffset(const std::string& BufferName, std::uint32_t Set, std::uint32_t Binding) const;

    const Graphics::FDeviceLocalBuffer& GetDescriptorBuffer(std::uint32_t FrameIndex, const std::string& BufferName);

    static FShaderBufferManager* GetInstance();

private:
    FShaderBufferManager();
    FShaderBufferManager(const FShaderBufferManager&) = delete;
    FShaderBufferManager(FShaderBufferManager&&)      = delete;
    ~FShaderBufferManager()                           = default;

    FShaderBufferManager& operator=(const FShaderBufferManager&) = delete;
    FShaderBufferManager& operator=(FShaderBufferManager&&)      = delete;

    vk::DeviceSize CalculateDescriptorBufferSize(const FDescriptorBufferCreateInfo& DescriptorBufferCreateInfo);
    void BindResourceToDescriptorBuffersInternal(const FDescriptorBufferCreateInfo& DescriptorBufferCreateInfo);

private:
    vk::PhysicalDeviceDescriptorBufferPropertiesEXT _DescriptorBufferProperties;
    // [Name, Buffer]
    std::unordered_map<std::string, FDataBufferInfo>       _DataBuffers;
    std::unordered_map<std::string, FDescriptorBufferInfo> _DescriptorBuffers;
    // [Name, [[Set, Binding], Offset]]
    std::unordered_map<std::string, std::unordered_map<std::pair<std::uint32_t, std::uint32_t>, vk::DeviceSize, FSetBindingHash>> _OffsetsMap;

    VmaAllocator _Allocator;
};

_GRAPHICS_END
_RUNTIME_END
_NPGS_END

#include "ShaderBufferManager.inl"
