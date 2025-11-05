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

        BufferCount = BufferCount ? BufferCount : Config::Graphics::kMaxFrameInFlight;

        BufferInfo.Buffers.reserve(BufferCount);
        vk::BufferUsageFlags BufferUsage = DataBufferCreateInfo.Usage == vk::DescriptorType::eUniformBuffer
                                         ? vk::BufferUsageFlagBits::eUniformBuffer
                                         : vk::BufferUsageFlagBits::eStorageBuffer;

        BufferUsage |= vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst;

        for (std::uint32_t i = 0; i != BufferCount; ++i)
        {
            vk::BufferCreateInfo BufferCreateInfo({}, BufferInfo.Size, BufferUsage);
            BufferInfo.Buffers.emplace_back(VulkanContext_, Allocator_, AllocationCreateInfo, BufferCreateInfo);
            BufferInfo.Buffers[i].SetPersistentMapping(true);
        }

        DataBuffers_.emplace(DataBufferCreateInfo.Name, std::move(BufferInfo));
    }

    NPGS_INLINE void FShaderBufferManager::RemoveDataBuffer(std::string_view Name)
    {
        DataBuffers_.erase(Name);
    }

    template <typename StructType>
    requires std::is_class_v<StructType>
    void FShaderBufferManager::UpdateDataBuffers(std::string_view Name, const StructType& Data) const
    {
        const auto& BufferInfo = GetDataBufferInfo(Name);
        for (std::uint32_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
        {
            BufferInfo.Buffers[i].CopyData(0, 0, BufferInfo.Size, &Data);
        }
    }

    template <typename StructType>
    requires std::is_class_v<StructType>
    inline void FShaderBufferManager::UpdateDataBuffer(std::uint32_t FrameIndex, std::string_view Name, const StructType& Data) const
    {
        const auto& BufferInfo = GetDataBufferInfo(Name);
        BufferInfo.Buffers[FrameIndex].CopyData(0, 0, BufferInfo.Size, &Data);
    }

    template <typename FieldType>
    std::vector<FShaderBufferManager::TUpdater<FieldType>>
    FShaderBufferManager::GetFieldUpdaters(std::string_view BufferName, std::string_view FieldName) const
    {
        const auto& BufferInfo = GetDataBufferInfo(BufferName);

        std::vector<TUpdater<FieldType>> Updaters;
        for (std::uint32_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
        {
            auto FieldOffsetAndSize = GetDataBufferFieldOffsetAndSize(BufferInfo, FieldName);

            vk::DeviceSize FieldOffset = FieldOffsetAndSize.first;
            vk::DeviceSize FieldSize   = FieldOffsetAndSize.second;

            Updaters.emplace_back(BufferInfo.Buffers[i], FieldOffset, FieldSize);
        }

        return Updaters;
    }

    template <typename FieldType>
    FShaderBufferManager::TUpdater<FieldType>
    FShaderBufferManager::GetFieldUpdater(std::uint32_t FrameIndex, std::string_view BufferName, std::string_view FieldName) const
    {
        const auto& BufferInfo  = GetDataBufferInfo(BufferName);
        auto FieldOffsetAndSize = GetDataBufferFieldOffsetAndSize(BufferInfo, FieldName);

        vk::DeviceSize FieldOffset = FieldOffsetAndSize.first;
        vk::DeviceSize FieldSize   = FieldOffsetAndSize.second;

        return TUpdater<FieldType>(BufferInfo.Buffers[FrameIndex], FieldOffset, FieldSize);
    }

    template <typename Type>
    void FShaderBufferManager::UpdateResourceDescriptors(std::string_view BufferName, std::uint32_t Set, std::uint32_t Binding,
                                                         vk::DescriptorType ConfirmedUsage, const Type& DescriptorInfo)
    {
        auto& BufferInfo    = GetDescriptorBufferInfo(BufferName);
        auto& OffsetAndType = GetDescriptorBindingOffsetAndType(BufferName, Set, Binding);

        vk::DeviceSize     Offset       = OffsetAndType.first;
        vk::DescriptorType CurrentUsage = OffsetAndType.second;

        if (CurrentUsage != ConfirmedUsage)
        {
            throw std::invalid_argument(std::format(R"(Descriptor type mismatch for buffer "{}", set {}, binding {}.)",
                                                    BufferName, Set, Binding));
        }
        if (ConfirmedUsage == vk::DescriptorType::eUniformBuffer || CurrentUsage == vk::DescriptorType::eStorageBuffer)
        {
            throw std::invalid_argument("Update buffer descriptor please use UpdateUniformDescriptors");
        }

        vk::DescriptorGetInfoEXT DescriptorGetInfo(ConfirmedUsage, &DescriptorInfo);
        vk::DeviceSize DescriptorSize = GetDescriptorSize(CurrentUsage);

        for (std::uint32_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
        {
            auto& BufferMemory = BufferInfo.Buffers[i].GetMemory();
            void* Target       = BufferMemory.GetMappedTargetMemory();
            vk::Device Device  = VulkanContext_->GetDevice();
            Device.getDescriptorEXT(DescriptorGetInfo, DescriptorSize, reinterpret_cast<std::byte*>(Target) + Offset);
        }
    }

    template <typename Type>
    void FShaderBufferManager::UpdateResourceDescriptor(std::uint32_t FrameIndex, std::string_view BufferName, std::uint32_t Set,
                                                        std::uint32_t Binding, vk::DescriptorType ConfirmedUsage, const Type& DescriptorInfo)
    {
        auto& BufferInfo = GetDescriptorBufferInfo(BufferName);
        auto& Buffer     = BufferInfo.Buffers[FrameIndex];

        auto& OffsetAndType = GetDescriptorBindingOffsetAndType(BufferName, Set, Binding);

        vk::DeviceSize     Offset       = OffsetAndType.first;
        vk::DescriptorType CurrentUsage = OffsetAndType.second;

        if (CurrentUsage != ConfirmedUsage)
        {
            throw std::invalid_argument(std::format(R"(Descriptor type mismatch for buffer "{}", set {}, binding {}.)",
                                                    BufferName, Set, Binding));
        }
        if (ConfirmedUsage == vk::DescriptorType::eUniformBuffer || CurrentUsage == vk::DescriptorType::eStorageBuffer)
        {
            throw std::invalid_argument("Update buffer descriptor please use UpdateUniformDescriptor");
        }

        vk::DescriptorGetInfoEXT DescriptorGetInfo(ConfirmedUsage, &DescriptorInfo);
        vk::DeviceSize DescriptorSize = GetDescriptorSize(CurrentUsage);

        auto& BufferMemory = Buffer.GetMemory();
        void* Target       = BufferMemory.GetMappedTargetMemory();
        vk::Device Device  = VulkanContext_->GetDevice();
        Device.getDescriptorEXT(DescriptorGetInfo, DescriptorSize, reinterpret_cast<std::byte*>(Target) + Offset);
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
} // namespace Npgs
