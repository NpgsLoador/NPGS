#include "ShaderBufferManager.h"

#include <utility>

#include "Engine/Core/Base/Config/EngineConfig.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"
#include "Engine/Utils/FieldReflection.hpp"
#include "Engine/Utils/Logger.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

template <typename StructType>
requires std::is_class_v<StructType>
inline void FShaderBufferManager::CreateBuffers(const FBufferCreateInfo& BufferCreateInfo,
                                                const VmaAllocationCreateInfo* AllocationCreateInfo,
                                                std::uint32_t BufferCount)
{
    const auto* VulkanContext = FVulkanContext::GetClassInstance();
    FBufferInfo BufferInfo;
    BufferInfo.CreateInfo = BufferCreateInfo;

    vk::DeviceSize MinUniformAlignment = VulkanContext->GetPhysicalDeviceProperties().limits.minUniformBufferOffsetAlignment;
    vk::DeviceSize MinStorageAlignment = VulkanContext->GetPhysicalDeviceProperties().limits.minStorageBufferOffsetAlignment;
    StructType BufferStruct{};
    Util::ForEachField(BufferStruct, [&](const auto& Field, std::size_t Index)
    {
        FBufferFieldInfo FieldInfo;
        FieldInfo.Size = sizeof(decltype(Field));

        if (BufferCreateInfo.Usage == vk::DescriptorType::eUniformBufferDynamic)
        {
            FieldInfo.Alignment = (FieldInfo.Size + MinUniformAlignment - 1) & ~(MinUniformAlignment - 1);
            FieldInfo.Offset    = BufferInfo.Size;
        }
        else if (BufferCreateInfo.Usage == vk::DescriptorType::eStorageBufferDynamic)
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
        BufferInfo.Fields.emplace(BufferCreateInfo.Fields[Index], std::move(FieldInfo));
    });

    BufferCount = BufferCount ? BufferCount : Config::Graphics::kMaxFrameInFlight;

    BufferInfo.Buffers.reserve(BufferCount);
    vk::BufferUsageFlagBits BufferUsage = BufferCreateInfo.Usage == vk::DescriptorType::eUniformBuffer
                                        ? vk::BufferUsageFlagBits::eUniformBuffer
                                        : vk::BufferUsageFlagBits::eStorageBuffer;

    for (std::uint32_t i = 0; i != BufferCount; ++i)
    {
        if (AllocationCreateInfo != nullptr)
        {
            vk::BufferCreateInfo BufferCreateInfo({}, BufferInfo.Size, BufferUsage | vk::BufferUsageFlagBits::eTransferDst);
            BufferInfo.Buffers.emplace_back(*AllocationCreateInfo, BufferCreateInfo);
        }
        else
        {
            BufferInfo.Buffers.emplace_back(BufferInfo.Size, BufferUsage);
        }

        StructType EmptyData{};
        BufferInfo.Buffers[i].EnablePersistentMapping();
        BufferInfo.Buffers[i].CopyData(0, 0, BufferInfo.Size, &EmptyData);
    }

    _Buffers.emplace(BufferCreateInfo.Name, std::move(BufferInfo));
}

NPGS_INLINE void FShaderBufferManager::RemoveBuffer(const std::string& Name)
{
    auto it = _Buffers.find(Name);
    if (it == _Buffers.end())
    {
        return;
    }

    _Buffers.erase(it);
}

template <typename StructType>
requires std::is_class_v<StructType>
inline void FShaderBufferManager::UpdateEntrieBuffers(const std::string& Name, const StructType& Data)
{
    auto& BufferInfo = _Buffers.at(Name);
    for (std::uint32_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
    {
        BufferInfo.Buffers[i].CopyData(0, 0, BufferInfo.Size, &Data);
    }
}

template <typename StructType>
requires std::is_class_v<StructType>
inline void FShaderBufferManager::UpdateEntrieBuffer(std::uint32_t FrameIndex, const std::string& Name, const StructType& Data)
{
    auto& BufferInfo = _Buffers.at(Name);
    BufferInfo.Buffers[FrameIndex].CopyData(0, 0, BufferInfo.Size, &Data);
}

template <typename FieldType>
NPGS_INLINE std::vector<FShaderBufferManager::TUpdater<FieldType>>
FShaderBufferManager::GetFieldUpdaters(const std::string& BufferName, const std::string& FieldName) const
{
    auto& BufferInfo = _Buffers.at(BufferName);
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
    auto& BufferInfo = _Buffers.at(BufferName);
    return TUpdater<FieldType>(BufferInfo.Buffers[FrameIndex], BufferInfo.Fields.at(FieldName).Offset, BufferInfo.Fields.at(FieldName).Size);
}

template <typename... Args>
NPGS_INLINE void FShaderBufferManager::BindShadersToBuffers(const std::string& BufferName, Args&&... ShaderNames)
{
    std::vector<std::string> ShaderNameList{ std::forward<Args>(ShaderNames)... };
    BindShaderListToBuffers(BufferName, ShaderNameList);
}

template <typename... Args>
NPGS_INLINE void FShaderBufferManager::BindShadersToBuffer(std::uint32_t FrameIndex, const std::string& BufferName, Args&&... ShaderNames)
{
    std::vector<std::string> ShaderNameList{ std::forward<Args>(ShaderNames)... };
    BindShaderListToBuffer(FrameIndex, BufferName, ShaderNameList);
}

NPGS_INLINE const Graphics::FDeviceLocalBuffer& FShaderBufferManager::GetBuffer(std::uint32_t FrameIndex, const std::string& BufferName)
{
    auto& BufferInfo = _Buffers.at(BufferName);
    return BufferInfo.Buffers[FrameIndex];
}

_GRAPHICS_END
_RUNTIME_END
_NPGS_END
