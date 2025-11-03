#include <format>
#include <stdexcept>
#include <utility>

#include "Engine/Core/Base/Config/EngineConfig.hpp"
#include "Engine/Core/Runtime/Graphics/Vulkan/Context.hpp"
#include "Engine/Utils/FieldReflection.hpp"
#include "Engine/Utils/Logger.hpp"

namespace Npgs
{
    template <typename DataType>
    FShaderBufferManager::TUpdater<DataType>::TUpdater(const FDeviceLocalBuffer& Buffer, vk::DeviceSize Offset, vk::DeviceSize Size)
        : Buffer_(&Buffer), Offset_(Offset), Size_(Size)
    {
    }

    template <typename DataType>
    NPGS_INLINE const FShaderBufferManager::TUpdater<DataType>&
    FShaderBufferManager::TUpdater<DataType>::operator<<(const DataType& Data) const
    {
        Submit(Data);
        return *this;
    }

    template <typename DataType>
    NPGS_INLINE void FShaderBufferManager::TUpdater<DataType>::Submit(const DataType& Data) const
    {
        Buffer_->CopyData(0, Offset_, Size_, &Data);
    }

    NPGS_INLINE std::size_t FShaderBufferManager::FSetBindingHash::operator()(const std::pair<std::uint32_t, std::uint32_t>& SetBinding) const noexcept
    {
        return std::hash<std::uint32_t>()(SetBinding.first) ^ std::hash<std::uint32_t>()(SetBinding.second);
    }

    template <typename StructType>
    requires std::is_class_v<StructType>
    void FShaderBufferManager::CreateDataBuffers(const FDataBufferCreateInfo& DataBufferCreateInfo,
                                                 const VmaAllocationCreateInfo& AllocationCreateInfo,
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
            vk::BufferCreateInfo BufferCreateInfo({}, BufferInfo.Size, BufferUsage | vk::BufferUsageFlagBits::eTransferDst);
            BufferInfo.Buffers.emplace_back(VulkanContext_, Allocator_, AllocationCreateInfo, BufferCreateInfo);

            StructType EmptyData{};
            BufferInfo.Buffers[i].SetPersistentMapping(true);
            BufferInfo.Buffers[i].CopyData(0, 0, BufferInfo.Size, &EmptyData);
        }

        DataBuffers_.emplace(DataBufferCreateInfo.Name, std::move(BufferInfo));
    }

    template <typename StructType>
    requires std::is_class_v<StructType>
    void FShaderBufferManager::UpdateDataBuffers(std::string_view Name, const StructType& Data)
    {
        auto it = DataBuffers_.find(Name);
        if (it == DataBuffers_.end())
        {
            throw std::out_of_range(std::format(R"(Data buffer "{}" not found.)", Name));
        }

        auto& BufferInfo = it->second;
        for (std::uint32_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
        {
            BufferInfo.Buffers[i].CopyData(0, 0, BufferInfo.Size, &Data);
        }
    }

    template <typename StructType>
    requires std::is_class_v<StructType>
    void FShaderBufferManager::UpdateDataBuffer(std::uint32_t FrameIndex, std::string_view Name, const StructType& Data)
    {
        auto it = DataBuffers_.find(Name);
        if (it == DataBuffers_.end())
        {
            throw std::out_of_range(std::format(R"(Data buffer "{}" not found.)", Name));
        }

        auto& BufferInfo = it->second;
        BufferInfo.Buffers[FrameIndex].CopyData(0, 0, BufferInfo.Size, &Data);
    }

    template <typename FieldType>
    std::vector<FShaderBufferManager::TUpdater<FieldType>>
    FShaderBufferManager::GetFieldUpdaters(std::string_view BufferName, std::string_view FieldName) const
    {
        auto BufferInfoIt = DataBuffers_.find(BufferName);
        if (BufferInfoIt == DataBuffers_.end())
        {
            throw std::out_of_range(std::format(R"(Data buffer "{}" not found.)", BufferName));
        }

        auto& BufferInfo = BufferInfoIt->second;

        std::vector<TUpdater<FieldType>> Updaters;
        for (std::uint32_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
        {
            auto FieldIt = BufferInfo.Fields.find(FieldName);
            if (FieldIt == BufferInfo.Fields.end())
            {
                throw std::out_of_range(std::format(R"(Buffer field "{}" not found.)", FieldName));
            }

            vk::DeviceSize FieldOffset = FieldIt->second.Offset;
            vk::DeviceSize FieldSize   = FieldIt->second.Size;
            Updaters.emplace_back(BufferInfo.Buffers[i], FieldOffset, FieldSize);
        }

        return Updaters;
    }

    template <typename FieldType>
    FShaderBufferManager::TUpdater<FieldType>
    FShaderBufferManager::GetFieldUpdater(std::uint32_t FrameIndex, std::string_view BufferName, std::string_view FieldName) const
    {
        auto BufferInfoIt = DataBuffers_.find(BufferName);
        if (BufferInfoIt == DataBuffers_.end())
        {
            throw std::out_of_range(std::format(R"(Data buffer "{}" not found.)", BufferName));
        }

        auto& BufferInfo = BufferInfoIt->second;

        auto FieldIt = BufferInfo.Fields.find(FieldName);
        if (FieldIt == BufferInfo.Fields.end())
        {
            throw std::out_of_range(std::format(R"(Buffer field "{}" not found.)", FieldName));
        }

        vk::DeviceSize FieldOffset = FieldIt->second.Offset;
        vk::DeviceSize FieldSize   = FieldIt->second.Size;

        return TUpdater<FieldType>(BufferInfo.Buffers[FrameIndex], FieldOffset, FieldSize);
    }

    inline const FDeviceLocalBuffer&
    FShaderBufferManager::GetDataBuffer(std::uint32_t FrameIndex, std::string_view BufferName) const
    {
        auto it = DataBuffers_.find(BufferName);
        if (it == DataBuffers_.end())
        {
            throw std::out_of_range(std::format(R"(Data buffer "{}" not found.)", BufferName));
        }

        auto& BufferInfo = it->second;
        return BufferInfo.Buffers[FrameIndex];
    }

    NPGS_INLINE void FShaderBufferManager::RemoveDescriptorBuffer(std::string_view Name)
    {
        DescriptorBuffers_.erase(Name);
    }

    template <typename... Args>
    std::vector<vk::DeviceSize>
    FShaderBufferManager::GetDescriptorBindingOffsets(std::string_view BufferName, Args... Sets) const
    {
        std::vector<vk::DeviceSize> Offsets;
        for (auto Set : { Sets... })
        {
            Offsets.emplace_back(GetDescriptorBindingOffset(BufferName, Set, 0));
        }

        return Offsets;
    }

    inline vk::DeviceSize
    FShaderBufferManager::GetDescriptorBindingOffset(std::string_view BufferName, std::uint32_t Set, std::uint32_t Binding) const
    {
        auto Pair = std::make_pair(Set, Binding);
        auto it = OffsetsMap_.find(BufferName);
        if (it == OffsetsMap_.end())
        {
            throw std::out_of_range(std::format(R"(Buffer offset "{}" not found.)", BufferName));
        }

        return it->second.at(Pair);
    }

    inline const FDeviceLocalBuffer&
    FShaderBufferManager::GetDescriptorBuffer(std::uint32_t FrameIndex, std::string_view BufferName) const
    {
        auto it = DescriptorBuffers_.find(BufferName);
        if (it == DescriptorBuffers_.end())
        {
            throw std::out_of_range(std::format(R"(Descriptor buffer "{}" not found.)", BufferName));
        }

        auto& BufferInfo = it->second;
        return BufferInfo.Buffers[FrameIndex];
    }
} // namespace Npgs
