#include "ShaderBufferManager.h"

#include <utility>

#include "Engine/Core/Base/Config/EngineConfig.h"
#include "Engine/Core/Runtime/AssetLoaders/AssetManager.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"
#include "Engine/Utils/FieldReflection.hpp"
#include "Engine/Utils/Logger.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

template <typename StructType>
requires std::is_class_v<StructType>
inline void FShaderBufferManager::CreateDataBuffers(const FDataBufferCreateInfo& DataBufferCreateInfo,
                                                    const VmaAllocationCreateInfo* AllocationCreateInfo,
                                                    std::uint32_t BufferCount)
{
    const auto* VulkanContext = FVulkanContext::GetClassInstance();
    FDataBufferInfo BufferInfo;
    BufferInfo.CreateInfo = DataBufferCreateInfo;

    vk::DeviceSize MinUniformAlignment = VulkanContext->GetPhysicalDeviceProperties().limits.minUniformBufferOffsetAlignment;
    vk::DeviceSize MinStorageAlignment = VulkanContext->GetPhysicalDeviceProperties().limits.minStorageBufferOffsetAlignment;
    StructType BufferStruct{};
    Util::ForEachField(BufferStruct, [&](const auto& Field, std::size_t Index)
    {
        FDataBufferFieldInfo FieldInfo;
        FieldInfo.Size = sizeof(decltype(Field));

        if (DataBufferCreateInfo.Usage == vk::DescriptorType::eUniformBufferDynamic)
        {
            FieldInfo.Alignment = (FieldInfo.Size + MinUniformAlignment - 1) & ~(MinUniformAlignment - 1);
            FieldInfo.Offset    = BufferInfo.Size;
        }
        else if (DataBufferCreateInfo.Usage == vk::DescriptorType::eStorageBufferDynamic)
        {
            FieldInfo.Alignment = (FieldInfo.Size + MinStorageAlignment - 1) & ~(MinStorageAlignment - 1);
            FieldInfo.Offset    = BufferInfo.Size;
        }
        else
        {
            FieldInfo.Alignment = FieldInfo.Size;
            FieldInfo.Offset    = reinterpret_cast<const std::byte*>(&Field) - reinterpret_cast<const std::byte*>(&BufferStruct);
        }

        BufferInfo.Size += FieldInfo.Alignment;
        BufferInfo.Fields.emplace(DataBufferCreateInfo.Fields[Index], std::move(FieldInfo));
    });

    BufferCount = BufferCount ? BufferCount : Config::Graphics::kMaxFrameInFlight;

    BufferInfo.Buffers.reserve(BufferCount);
    vk::BufferUsageFlags BufferUsage = DataBufferCreateInfo.Usage == vk::DescriptorType::eUniformBuffer
                                     ? vk::BufferUsageFlagBits::eUniformBuffer
                                     : vk::BufferUsageFlagBits::eStorageBuffer;
    BufferUsage |= vk::BufferUsageFlagBits::eShaderDeviceAddress;

    for (std::uint32_t i = 0; i != BufferCount; ++i)
    {
        if (AllocationCreateInfo != nullptr)
        {
            vk::BufferCreateInfo BufferCreateInfo({}, BufferInfo.Size, BufferUsage | vk::BufferUsageFlagBits::eTransferDst);
            BufferInfo.Buffers.emplace_back(_Allocator, *AllocationCreateInfo, BufferCreateInfo);
        }
        else
        {
            BufferInfo.Buffers.emplace_back(BufferInfo.Size, BufferUsage);
        }

        StructType EmptyData{};
        BufferInfo.Buffers[i].EnablePersistentMapping();
        BufferInfo.Buffers[i].CopyData(0, 0, BufferInfo.Size, &EmptyData);
    }

    _DataBuffers.emplace(DataBufferCreateInfo.Name, std::move(BufferInfo));
}

NPGS_INLINE void FShaderBufferManager::RemoveDataBuffer(const std::string& Name)
{
    auto it = _DataBuffers.find(Name);
    if (it == _DataBuffers.end())
    {
        return;
    }

    _DataBuffers.erase(it);
}

template <typename StructType>
requires std::is_class_v<StructType>
inline void FShaderBufferManager::UpdateDataBuffers(const std::string& Name, const StructType& Data)
{
    auto& BufferInfo = _DataBuffers.at(Name);
    for (std::uint32_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
    {
        BufferInfo.Buffers[i].CopyData(0, 0, BufferInfo.Size, &Data);
    }
}

template <typename StructType>
requires std::is_class_v<StructType>
inline void FShaderBufferManager::UpdateDataBuffer(std::uint32_t FrameIndex, const std::string& Name, const StructType& Data)
{
    auto& BufferInfo = _DataBuffers.at(Name);
    BufferInfo.Buffers[FrameIndex].CopyData(0, 0, BufferInfo.Size, &Data);
}

template <typename FieldType>
NPGS_INLINE std::vector<FShaderBufferManager::TUpdater<FieldType>>
FShaderBufferManager::GetFieldUpdaters(const std::string& BufferName, const std::string& FieldName) const
{
    auto& BufferInfo = _DataBuffers.at(BufferName);
    std::vector<TUpdater<FieldType>> Updaters;
    for (std::uint32_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
    {
        Updaters.emplace_back(BufferInfo.Buffers[i], BufferInfo.Fields.at(FieldName).Offset, BufferInfo.Fields.at(FieldName).Size);
    }

    return Updaters;
}

template <typename FieldType>
NPGS_INLINE FShaderBufferManager::TUpdater<FieldType>
FShaderBufferManager::GetFieldUpdater(std::uint32_t FrameIndex, const std::string& BufferName, const std::string& FieldName) const
{
    auto& BufferInfo = _DataBuffers.at(BufferName);
    return TUpdater<FieldType>(BufferInfo.Buffers[FrameIndex], BufferInfo.Fields.at(FieldName).Offset, BufferInfo.Fields.at(FieldName).Size);
}

template <typename... Args>
NPGS_INLINE std::vector<vk::DeviceSize>
FShaderBufferManager::GetDescriptorBindingOffsets(const std::string& BufferName, Args... Sets)
{
    std::vector<vk::DeviceSize> Offsets;
    for (auto Set : { Sets... })
    {
        Offsets.emplace_back(GetDescriptorBindingOffset(BufferName, Set, 0));
    }

    return Offsets;
}

NPGS_INLINE const Graphics::FDeviceLocalBuffer& FShaderBufferManager::GetDataBuffer(std::uint32_t FrameIndex, const std::string& BufferName)
{
    auto& BufferInfo = _DataBuffers.at(BufferName);
    return BufferInfo.Buffers[FrameIndex];
}

NPGS_INLINE void FShaderBufferManager::RemoveDescriptorBuffer(const std::string& Name)
{
    auto it = _DescriptorBuffers.find(Name);
    if (it == _DescriptorBuffers.end())
    {
        return;
    }

    _DescriptorBuffers.erase(it);
}

NPGS_INLINE vk::DeviceSize
FShaderBufferManager::GetDescriptorBindingOffset(const std::string& BufferName, std::uint32_t Set, std::uint32_t Binding) const
{
    auto Pair = std::make_pair(Set, Binding);
    return _OffsetsMap.at(BufferName).at(Pair);
}

NPGS_INLINE const Graphics::FDeviceLocalBuffer&
FShaderBufferManager::GetDescriptorBuffer(std::uint32_t FrameIndex, const std::string& BufferName)
{
    auto& BufferInfo = _DescriptorBuffers.at(BufferName);
    return BufferInfo.Buffers[FrameIndex];
}

_GRAPHICS_END
_RUNTIME_END
_NPGS_END
