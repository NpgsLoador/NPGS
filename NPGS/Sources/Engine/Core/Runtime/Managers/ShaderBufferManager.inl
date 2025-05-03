#include <utility>

#include "Engine/Core/Base/Base.h"
#include "Engine/Core/Base/Config/EngineConfig.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"
#include "Engine/Core/Runtime/Managers/AssetManager.h"
#include "Engine/Utils/FieldReflection.hpp"
#include "Engine/Utils/Logger.h"

namespace Npgs
{
    template <typename StructType>
    requires std::is_class_v<StructType>
    inline void FShaderBufferManager::CreateDataBuffers(const FDataBufferCreateInfo& DataBufferCreateInfo,
                                                        const VmaAllocationCreateInfo* AllocationCreateInfo,
                                                        std::uint32_t BufferCount)
    {
        FDataBufferInfo BufferInfo;
        BufferInfo.CreateInfo = DataBufferCreateInfo;

        vk::DeviceSize MinUniformAlignment = VulkanContext_->GetPhysicalDeviceProperties().limits.minUniformBufferOffsetAlignment;
        vk::DeviceSize MinStorageAlignment = VulkanContext_->GetPhysicalDeviceProperties().limits.minStorageBufferOffsetAlignment;
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
                BufferInfo.Buffers.emplace_back(VulkanContext_, Allocator_, *AllocationCreateInfo, BufferCreateInfo);
            }
            else
            {
                BufferInfo.Buffers.emplace_back(VulkanContext_, BufferInfo.Size, BufferUsage);
            }

            StructType EmptyData{};
            BufferInfo.Buffers[i].EnablePersistentMapping();
            BufferInfo.Buffers[i].CopyData(0, 0, BufferInfo.Size, &EmptyData);
        }

        DataBuffers_.emplace(DataBufferCreateInfo.Name, std::move(BufferInfo));
    }

    NPGS_INLINE void FShaderBufferManager::RemoveDataBuffer(const std::string& Name)
    {
        auto it = DataBuffers_.find(Name);
        if (it == DataBuffers_.end())
        {
            return;
        }

        DataBuffers_.erase(it);
    }

    template <typename StructType>
    requires std::is_class_v<StructType>
    inline void FShaderBufferManager::UpdateDataBuffers(const std::string& Name, const StructType& Data)
    {
        auto& BufferInfo = DataBuffers_.at(Name);
        for (std::uint32_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
        {
            BufferInfo.Buffers[i].CopyData(0, 0, BufferInfo.Size, &Data);
        }
    }

    template <typename StructType>
    requires std::is_class_v<StructType>
    inline void FShaderBufferManager::UpdateDataBuffer(std::uint32_t FrameIndex, const std::string& Name, const StructType& Data)
    {
        auto& BufferInfo = DataBuffers_.at(Name);
        BufferInfo.Buffers[FrameIndex].CopyData(0, 0, BufferInfo.Size, &Data);
    }

    template <typename FieldType>
    NPGS_INLINE std::vector<FShaderBufferManager::TUpdater<FieldType>>
    FShaderBufferManager::GetFieldUpdaters(const std::string& BufferName, const std::string& FieldName) const
    {
        auto& BufferInfo = DataBuffers_.at(BufferName);
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
        auto& BufferInfo = DataBuffers_.at(BufferName);
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

    NPGS_INLINE const FDeviceLocalBuffer&
    FShaderBufferManager::GetDataBuffer(std::uint32_t FrameIndex, const std::string& BufferName)
    {
        auto& BufferInfo = DataBuffers_.at(BufferName);
        return BufferInfo.Buffers[FrameIndex];
    }

    NPGS_INLINE void FShaderBufferManager::RemoveDescriptorBuffer(const std::string& Name)
    {
        auto it = DescriptorBuffers_.find(Name);
        if (it == DescriptorBuffers_.end())
        {
            return;
        }

        DescriptorBuffers_.erase(it);
    }

    NPGS_INLINE vk::DeviceSize
    FShaderBufferManager::GetDescriptorBindingOffset(const std::string& BufferName, std::uint32_t Set, std::uint32_t Binding) const
    {
        auto Pair = std::make_pair(Set, Binding);
        return OffsetsMap_.at(BufferName).at(Pair);
    }

    NPGS_INLINE const FDeviceLocalBuffer&
    FShaderBufferManager::GetDescriptorBuffer(std::uint32_t FrameIndex, const std::string& BufferName)
    {
        auto& BufferInfo = DescriptorBuffers_.at(BufferName);
        return BufferInfo.Buffers[FrameIndex];
    }
} // namespace Npgs
