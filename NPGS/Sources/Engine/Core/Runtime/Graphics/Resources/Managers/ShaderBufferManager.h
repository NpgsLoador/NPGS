#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <unordered_map>
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

    struct FBufferCreateInfo
    {
        std::string              Name;
        std::vector<std::string> Fields;
        std::uint32_t            Set{};
        std::uint32_t            Binding{};
        vk::DescriptorType       Usage{};
    };

private:
    struct FBufferFieldInfo
    {
        vk::DeviceSize Offset{};
        vk::DeviceSize Size{};
        vk::DeviceSize Alignment{};
    };

    struct FBufferInfo
    {
        std::unordered_map<std::string, FBufferFieldInfo> Fields;
        std::vector<FDeviceLocalBuffer> Buffers;
        FBufferCreateInfo               CreateInfo;
        vk::DeviceSize                  Size{};
    };

public:
    template <typename StructType>
    requires std::is_class_v<StructType>
    void CreateBuffers(const FBufferCreateInfo& BufferCreateInfo,
                       const VmaAllocationCreateInfo* AllocationCreateInfo = nullptr,
                       std::uint32_t BufferCount = 0);

    void RemoveBuffer(const std::string& Name);

    template <typename StructType>
    requires std::is_class_v<StructType>
    void UpdateEntrieBuffers(const std::string& Name, const StructType& Data);

    template <typename StructType>
    requires std::is_class_v<StructType>
    void UpdateEntrieBuffer(std::uint32_t FrameIndex, const std::string& Name, const StructType& Data);

    template <typename FieldType>
    std::vector<TUpdater<FieldType>> GetFieldUpdaters(const std::string& BufferName, const std::string& FieldName) const;

    template <typename FieldType>
    TUpdater<FieldType> GetFieldUpdater(std::uint32_t FrameIndex, const std::string& BufferName, const std::string& FieldName) const;

    void BindShaderToBuffers(const std::string& BufferName, const std::string& ShaderName, vk::DeviceSize Range = 0);
    void BindShaderToBuffer(std::uint32_t FrameIndex, const std::string& BufferName, const std::string& ShaderName, vk::DeviceSize Range = 0);
    void BindShaderListToBuffers(const std::string& BufferName, const std::vector<std::string>& ShaderNameList);
    void BindShaderListToBuffer(std::uint32_t FrameIndex, const std::string& BufferName, const std::vector<std::string>& ShaderNameList);

    template <typename... Args>
    void BindShadersToBuffers(const std::string& BufferName, Args&&... ShaderNames);

    template <typename... Args>
    void BindShadersToBuffer(std::uint32_t FrameIndex, const std::string& BufferName, Args&&... ShaderNames);

    const Graphics::FDeviceLocalBuffer& GetBuffer(std::uint32_t FrameIndex, const std::string& BufferName);

    static FShaderBufferManager* GetInstance();

private:
    FShaderBufferManager();
    FShaderBufferManager(const FShaderBufferManager&) = delete;
    FShaderBufferManager(FShaderBufferManager&&)      = delete;
    ~FShaderBufferManager()                           = default;

    FShaderBufferManager& operator=(const FShaderBufferManager&) = delete;
    FShaderBufferManager& operator=(FShaderBufferManager&&)      = delete;

private:
    std::unordered_map<std::string, FBufferInfo> _Buffers;
    VmaAllocator                                 _Allocator;
};

_GRAPHICS_END
_RUNTIME_END
_NPGS_END

#include "ShaderBufferManager.inl"
