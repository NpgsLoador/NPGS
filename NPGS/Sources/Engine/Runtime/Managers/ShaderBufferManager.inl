#include <exception>
#include <format>
#include <stdexcept>
#include <utility>

#include "Engine/Core/Base/Config/EngineConfig.hpp"
#include "Engine/Core/Utils/FieldReflection.hpp"
#include "Engine/Core/Logger.hpp"
#include "Engine/Runtime/Graphics/Vulkan/Context.hpp"

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
    void FShaderBufferManager::AllocateDataBuffers(const FDataBufferCreateInfo& DataBufferCreateInfo)
    {
        FDataBufferInfo BufferInfo;
        BufferInfo.CreateInfo = DataBufferCreateInfo;
        BufferInfo.Type       = DataBufferCreateInfo.Usage;

        StructType BufferStruct{};
        Utils::ForEachField(BufferStruct, [&, this](const auto& Field, std::size_t Index) -> void
        {
            FDataBufferFieldInfo FieldInfo;
            FieldInfo.Size = sizeof(decltype(Field));

            if (DataBufferCreateInfo.Usage == vk::DescriptorType::eUniformBufferDynamic)
            {
                vk::DeviceSize MinUniformAlignment =
                    VulkanContext_->GetPhysicalDeviceProperties().limits.minUniformBufferOffsetAlignment;
                FieldInfo.Alignment = (FieldInfo.Size + MinUniformAlignment - 1) & ~(MinUniformAlignment - 1);
                FieldInfo.Offset    = BufferInfo.Size;
            }
            else if (DataBufferCreateInfo.Usage == vk::DescriptorType::eStorageBufferDynamic)
            {
                vk::DeviceSize MinStorageAlignment =
                    VulkanContext_->GetPhysicalDeviceProperties().limits.minStorageBufferOffsetAlignment;
                FieldInfo.Alignment = (FieldInfo.Size + MinStorageAlignment - 1) & ~(MinStorageAlignment - 1);
                FieldInfo.Offset    = BufferInfo.Size;
            }
            else
            {
                FieldInfo.Alignment = FieldInfo.Size;
                FieldInfo.Offset    = reinterpret_cast<const std::byte*>(&Field)
                                    - reinterpret_cast<const std::byte*>(&BufferStruct);
            }

            BufferInfo.Size += FieldInfo.Alignment;
            BufferInfo.Fields.emplace(DataBufferCreateInfo.Fields[Index], std::move(FieldInfo));
        });

        vk::BufferUsageFlags BufferUsage = DataBufferCreateInfo.Usage == vk::DescriptorType::eUniformBuffer
                                         ? vk::BufferUsageFlagBits::eUniformBuffer
                                         : vk::BufferUsageFlagBits::eStorageBuffer;

        BufferUsage |= vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst;

        const auto& Limits = VulkanContext_->GetPhysicalDeviceProperties().limits;

        try
        {
            if (BufferInfo.Type == vk::DescriptorType::eUniformBuffer ||
                BufferInfo.Type == vk::DescriptorType::eUniformBufferDynamic)
            {
                BufferInfo.Offset = UniformHeapAllocator_.Allocate(BufferInfo.Size, Limits.minUniformBufferOffsetAlignment);
            }
            else
            {
                BufferInfo.Offset = StorageHeapAllocator_.Allocate(BufferInfo.Size, Limits.minStorageBufferOffsetAlignment);
            }
        }
        catch (const std::exception& e)
        {
            NpgsCoreError("Heap allocation failed for data buffer {}: {}", DataBufferCreateInfo.Name, e.what());
        }

        DataBuffers_.emplace(DataBufferCreateInfo.Name, std::move(BufferInfo));
    }

    template <typename StructType>
    requires std::is_class_v<StructType>
    void FShaderBufferManager::UpdateDataBuffers(std::string_view Name, const StructType& Data) const
    {
        const auto& BufferInfo = GetDataBufferInfo(Name);
        for (std::uint32_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
        {
            const FDeviceLocalBuffer* TargetHeap = nullptr;
            if (BufferInfo.Type == vk::DescriptorType::eUniformBuffer ||
                BufferInfo.Type == vk::DescriptorType::eUniformBufferDynamic)
            {
                TargetHeap = &UniformDataHeaps_[i];
            }
            else
            {
                TargetHeap = &StorageDataHeaps_[i];
            }

            TargetHeap->CopyData(0, BufferInfo.Offset, BufferInfo.Size, &Data);
        }
    }

    template <typename StructType>
    requires std::is_class_v<StructType>
    void FShaderBufferManager::UpdateDataBuffer(std::uint32_t FrameIndex, std::string_view Name, const StructType& Data) const
    {
        const auto& BufferInfo = GetDataBufferInfo(Name);

        const FDeviceLocalBuffer* TargetHeap = nullptr;
        if (BufferInfo.Type == vk::DescriptorType::eUniformBuffer ||
            BufferInfo.Type == vk::DescriptorType::eUniformBufferDynamic)
        {
            TargetHeap = &UniformDataHeaps_[FrameIndex];
        }
        else
        {
            TargetHeap = &StorageDataHeaps_[FrameIndex];
        }

        TargetHeap->CopyData(0, BufferInfo.Offset, BufferInfo.Size, &Data);
    }

    template <typename FieldType>
    std::vector<FShaderBufferManager::TUpdater<FieldType>>
    FShaderBufferManager::GetFieldUpdaters(std::string_view BufferName, std::string_view FieldName) const
    {
        const auto& BufferInfo  = GetDataBufferInfo(BufferName);
        auto FieldOffsetAndSize = GetDataBufferFieldOffsetAndSize(BufferInfo, FieldName);

        vk::DeviceSize FieldOffset    = FieldOffsetAndSize.first;
        vk::DeviceSize FieldSize      = FieldOffsetAndSize.second;
        vk::DeviceSize AbsoluteOffset = BufferInfo.Offset + FieldOffset;

        std::vector<TUpdater<FieldType>> Updaters;
        for (std::uint32_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
        {
            const FDeviceLocalBuffer* TargetHeap = nullptr;
            if (BufferInfo.Type == vk::DescriptorType::eUniformBuffer ||
                BufferInfo.Type == vk::DescriptorType::eUniformBufferDynamic)
            {
                TargetHeap = &UniformDataHeaps_[i];
            }
            else
            {
                TargetHeap = &StorageDataHeaps_[i];
            }

            Updaters.emplace_back(*TargetHeap, AbsoluteOffset, FieldSize);
        }

        return Updaters;
    }

    template <typename FieldType>
    FShaderBufferManager::TUpdater<FieldType>
    FShaderBufferManager::GetFieldUpdater(std::uint32_t FrameIndex, std::string_view BufferName, std::string_view FieldName) const
    {
        const auto& BufferInfo  = GetDataBufferInfo(BufferName);
        auto FieldOffsetAndSize = GetDataBufferFieldOffsetAndSize(BufferInfo, FieldName);

        vk::DeviceSize FieldOffset    = FieldOffsetAndSize.first;
        vk::DeviceSize FieldSize      = FieldOffsetAndSize.second;
        vk::DeviceSize AbsoluteOffset = BufferInfo.Offset + FieldOffset;

        const FDeviceLocalBuffer* TargetHeap = nullptr;
        if (BufferInfo.Type == vk::DescriptorType::eUniformBuffer ||
            BufferInfo.Type == vk::DescriptorType::eUniformBufferDynamic)
        {
            TargetHeap = &UniformDataHeaps_[FrameIndex];
        }
        else
        {
            TargetHeap = &StorageDataHeaps_[FrameIndex];
        }

        return TUpdater<FieldType>(*TargetHeap, AbsoluteOffset, FieldSize);
    }

    template <typename... Args>
    std::vector<vk::DeviceSize>
    FShaderBufferManager::GetDescriptorBindingOffsets(std::string_view BufferName, Args... Sets) const
    {
        const auto& BufferInfo = GetDescriptorBufferInfo(BufferName);

        std::vector<vk::DeviceSize> Offsets;
        Offsets.reserve(sizeof...(Sets));

        auto GetOffset = [&](std::uint32_t SetIndex) -> vk::DeviceSize
        {
            auto AllocIt = BufferInfo.SetAllocations.find(SetIndex);
            if (AllocIt == BufferInfo.SetAllocations.end())
            {
                throw std::out_of_range(std::format(R"(Set {} allocation not found in descriptor buffer "{}".)", SetIndex, BufferName));
            }

            return AllocIt->second.Offset;
        };

        (Offsets.push_back(GetOffset(Sets)), ...);
        return Offsets;
    }

    NPGS_INLINE const FDeviceLocalBuffer& FShaderBufferManager::GetResourceDescriptorHeap(std::uint32_t FrameIndex) const
    {
        return ResourceDescriptorHeaps_[FrameIndex];
    }

    NPGS_INLINE const FDeviceLocalBuffer& FShaderBufferManager::GetSamplerDescriptorHeap(std::uint32_t FrameIndex) const
    {
        return SamplerDescriptorHeaps_[FrameIndex];
    }

    NPGS_INLINE vk::DescriptorBufferBindingInfoEXT FShaderBufferManager::GetResourceHeapBindingInfo(std::uint32_t FrameIndex) const
    {
        auto HeapUsage = vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT |
                         vk::BufferUsageFlagBits::eSamplerDescriptorBufferEXT  |
                         vk::BufferUsageFlagBits::eShaderDeviceAddress         |
                         vk::BufferUsageFlagBits::eTransferDst;

        vk::DescriptorBufferBindingInfoEXT BindingInfo(GetResourceDescriptorHeap(FrameIndex).GetBuffer().GetDeviceAddress(), HeapUsage);
        return BindingInfo;
    }

    NPGS_INLINE vk::DescriptorBufferBindingInfoEXT FShaderBufferManager::GetSamplerHeapBindingInfo(std::uint32_t FrameIndex) const
    {
        auto HeapUsage = vk::BufferUsageFlagBits::eSamplerDescriptorBufferEXT |
                         vk::BufferUsageFlagBits::eShaderDeviceAddress        |
                         vk::BufferUsageFlagBits::eTransferDst;

        vk::DescriptorBufferBindingInfoEXT BindingInfo(GetSamplerDescriptorHeap(FrameIndex).GetBuffer().GetDeviceAddress(), HeapUsage);
        return BindingInfo;
    }
} // namespace Npgs
