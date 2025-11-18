#include "stdafx.h"
#include "ShaderBufferManager.hpp"

#include <format>
#include <stdexcept>
#include <Volk/volk.h>

#include "Engine/Core/Base/Config/EngineConfig.hpp"
#include "Engine/Utils/Logger.hpp"

namespace Npgs
{
    FShaderBufferManager::FShaderBufferManager(FVulkanContext* VulkanContext)
        : VulkanContext_(VulkanContext)
    {
        VmaVulkanFunctions VulkanFunctions
        {
            .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
            .vkGetDeviceProcAddr   = vkGetDeviceProcAddr
        };

        VmaAllocatorCreateInfo AllocatorCreateInfo
        {
            .flags            = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice   = VulkanContext_->GetPhysicalDevice(),
            .device           = VulkanContext_->GetDevice(),
            .pVulkanFunctions = &VulkanFunctions,
            .instance         = VulkanContext_->GetInstance()
        };

        vmaCreateAllocator(&AllocatorCreateInfo, &Allocator_);
    }

    FShaderBufferManager::~FShaderBufferManager()
    {
        DataBuffers_.clear();
        DescriptorBuffers_.clear();
        vmaDestroyAllocator(Allocator_);
    }

    const FDeviceLocalBuffer& FShaderBufferManager::GetDataBuffer(std::uint32_t FrameIndex, std::string_view BufferName) const
    {
        auto it = DataBuffers_.find(BufferName);
        if (it == DataBuffers_.end())
        {
            throw std::out_of_range(std::format(R"(Data buffer "{}" not found.)", BufferName));
        }

        auto& BufferInfo = it->second;
        return BufferInfo.Buffers[FrameIndex];
    }

    void FShaderBufferManager::CreateDescriptorBuffer(const FDescriptorBufferCreateInfo& DescriptorBufferCreateInfo,
                                                      const VmaAllocationCreateInfo& AllocationCreateInfo)
    {
        vk::DeviceSize BufferSize = CalculateDescriptorBufferSize(DescriptorBufferCreateInfo);
        if (BufferSize == 0)
        {
            NpgsCoreError("Failed to create descriptor buffer: buffer size is zero.");
            return;
        }

        FDescriptorBufferInfo BufferInfo;
        BufferInfo.Name = DescriptorBufferCreateInfo.Name;
        BufferInfo.Size = BufferSize;
        BufferInfo.Buffers.reserve(Config::Graphics::kMaxFrameInFlight);

        for (std::uint32_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
        {
            std::string BufferName = std::format("{}_DescriptorBuffer_Frame{}", DescriptorBufferCreateInfo.Name, i);
            auto BufferUsage = vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT | vk::BufferUsageFlagBits::eShaderDeviceAddress;
            vk::BufferCreateInfo BufferCreateInfo({}, BufferSize, BufferUsage);
            BufferInfo.Buffers.emplace_back(VulkanContext_, BufferName, Allocator_, AllocationCreateInfo, BufferCreateInfo);
            BufferInfo.Buffers[i].SetPersistentMapping(true);
        }

        DescriptorBuffers_.emplace(DescriptorBufferCreateInfo.Name, std::move(BufferInfo));
        NpgsCoreTrace("Created descriptor buffer \"{}\" with size {} bytes.", DescriptorBufferCreateInfo.Name, BufferSize);

        BindResourceToDescriptorBuffersInternal(DescriptorBufferCreateInfo);
    }

    void FShaderBufferManager::RemoveDescriptorBuffer(std::string_view Name)
    {
        DescriptorBuffers_.erase(Name);
        OffsetsMap_.erase(Name);
    }

    vk::DeviceSize
    FShaderBufferManager::GetDescriptorBindingOffset(std::string_view BufferName, std::uint32_t Set, std::uint32_t Binding) const
    {
        auto SetBindingPair = std::make_pair(Set, Binding);
        auto it = OffsetsMap_.find(BufferName);
        if (it == OffsetsMap_.end())
        {
            throw std::out_of_range(std::format(R"(Buffer offset "{}" not found.)", BufferName));
        }

        return it->second.at(SetBindingPair).first;
    }

    const FDeviceLocalBuffer&
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

    const FShaderBufferManager::FDataBufferInfo&
    FShaderBufferManager::GetDataBufferInfo(std::string_view BufferName) const
    {
        auto it = DataBuffers_.find(BufferName);
        if (it == DataBuffers_.end())
        {
            throw std::out_of_range(std::format(R"(Data bufer info "{}" not found.)", BufferName));
        }

        return it->second;
    }

    std::pair<vk::DeviceSize, vk::DeviceSize>
    FShaderBufferManager::GetDataBufferFieldOffsetAndSize(const FDataBufferInfo& BufferInfo, std::string_view FieldName) const
    {
        auto it = BufferInfo.Fields.find(FieldName);
        if (it == BufferInfo.Fields.end())
        {
            throw std::out_of_range(std::format(R"(Buffer field "{}" not found.)", FieldName));
        }

        vk::DeviceSize FieldOffset = it->second.Offset;
        vk::DeviceSize FieldSize   = it->second.Size;

        return { FieldOffset, FieldSize };
    }

    const FShaderBufferManager::FDescriptorBufferInfo&
    FShaderBufferManager::GetDescriptorBufferInfo(std::string_view BufferName) const
    {
        auto it = DescriptorBuffers_.find(BufferName);
        if (it == DescriptorBuffers_.end())
        {
            throw std::out_of_range(std::format(R"(Descriptor buffer info "{}" not found.)", BufferName));
        }

        return it->second;
    }

    std::pair<vk::DeviceSize, vk::DescriptorType>
    FShaderBufferManager::GetDescriptorBindingOffsetAndType(std::string_view BufferName, std::uint32_t Set, std::uint32_t Binding) const
    {
        auto it = OffsetsMap_.find(BufferName);
        if (it == OffsetsMap_.end())
        {
            throw std::out_of_range(std::format(R"(Descriptor buffer offsets for "{}" not found.)", BufferName));
        }

        const auto& OffsetMap = it->second;
        auto SetBindingPair   = std::make_pair(Set, Binding);
        return OffsetMap.at(SetBindingPair);
    }

    vk::DeviceSize FShaderBufferManager::GetDescriptorSize(vk::DescriptorType Usage) const
    {
        switch (Usage)
        {
        case vk::DescriptorType::eUniformBuffer:
            return DescriptorBufferProperties_.uniformBufferDescriptorSize;
        case vk::DescriptorType::eStorageBuffer:
            return DescriptorBufferProperties_.storageBufferDescriptorSize;
        case vk::DescriptorType::eSampler:
            return DescriptorBufferProperties_.samplerDescriptorSize;
        case vk::DescriptorType::eSampledImage:
            return DescriptorBufferProperties_.sampledImageDescriptorSize;
        case vk::DescriptorType::eStorageImage:
            return DescriptorBufferProperties_.storageImageDescriptorSize;
        case vk::DescriptorType::eCombinedImageSampler:
            return DescriptorBufferProperties_.combinedImageSamplerDescriptorSize;
        default:
            throw std::invalid_argument("Unsupported Descriptor Type for GetDescriptorSize.");
        }
    }

    vk::DeviceSize FShaderBufferManager::CalculateDescriptorBufferSize(const FDescriptorBufferCreateInfo& DescriptorBufferCreateInfo)
    {
        static bool bDescriptorBufferPropertiesGot = false;
        if (!bDescriptorBufferPropertiesGot)
        {
            vk::PhysicalDeviceProperties2 Properties2;
            Properties2.pNext = &DescriptorBufferProperties_;
            VulkanContext_->GetPhysicalDevice().getProperties2(&Properties2);
            bDescriptorBufferPropertiesGot = true;
        }

        vk::DeviceSize TotalSize = 0;
        for (auto& [Set, Info] : DescriptorBufferCreateInfo.SetInfos)
        {
            TotalSize += Info.Size;
        }

        return TotalSize;
    }

    void FShaderBufferManager::BindResourceToDescriptorBuffersInternal(const FDescriptorBufferCreateInfo& DescriptorBufferCreateInfo)
    {
        auto& DescriptorBufferInfo = DescriptorBuffers_.at(DescriptorBufferCreateInfo.Name);
        vk::Device Device = VulkanContext_->GetDevice();

        auto InsertOffsetMap = [this, &DescriptorBufferCreateInfo](const auto& Info, vk::DeviceSize Offset, vk::DescriptorType Type) -> void
        {
            std::uint32_t Set     = Info.Set;
            std::uint32_t Binding = Info.Binding;
            OffsetsMap_[DescriptorBufferCreateInfo.Name].try_emplace(std::make_pair(Set, Binding), std::make_pair(Offset, Type));
        };

        auto& SetOffsets = SetBaseOffsetsMap_[DescriptorBufferCreateInfo.Name];
        SetOffsets.clear();
        vk::DeviceSize CurrentSetOffset = 0;
        for (auto& [Set, Info] : DescriptorBufferCreateInfo.SetInfos)
        {
            SetOffsets[Set] = CurrentSetOffset;
            CurrentSetOffset += Info.Size;
        }

        for (std::uint32_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
        {
            void* Target       = nullptr;
            auto& BufferMemory = DescriptorBufferInfo.Buffers[i].GetMemory();
            BufferMemory.MapMemoryForSubmit(0, DescriptorBufferInfo.Size, Target);

            const auto& SetInfos = DescriptorBufferCreateInfo.SetInfos;

            for (std::size_t j = 0; j != DescriptorBufferCreateInfo.UniformBufferNames.size(); ++j)
            {
                const auto&    BufferInfo     = DataBuffers_.at(DescriptorBufferCreateInfo.UniformBufferNames[j]);
                std::uint32_t  CurrentSet     = BufferInfo.CreateInfo.Set;
                std::uint32_t  CurrentBinding = BufferInfo.CreateInfo.Binding;
                vk::DeviceSize SetOffset      = SetOffsets.at(CurrentSet);
                vk::DeviceSize BindingOffset  = SetOffset + SetInfos.at(CurrentSet).Bindings.at(CurrentBinding).Offset;

                vk::DescriptorType Usage = BufferInfo.CreateInfo.Usage;
                vk::DescriptorAddressInfoEXT AddressInfo(BufferInfo.Buffers[i].GetBuffer().GetDeviceAddress(), BufferInfo.Size);
                vk::DescriptorGetInfoEXT DescriptorGetInfo(Usage, &AddressInfo);
                vk::DeviceSize DescriptorSize = DescriptorBufferProperties_.uniformBufferDescriptorSize;
                Device.getDescriptorEXT(DescriptorGetInfo, DescriptorSize, static_cast<std::byte*>(Target) + BindingOffset);

                InsertOffsetMap(BufferInfo.CreateInfo, BindingOffset, Usage);
            }

            for (std::size_t j = 0; j != DescriptorBufferCreateInfo.StorageBufferNames.size(); ++j)
            {
                const auto&    BufferInfo     = DataBuffers_.at(DescriptorBufferCreateInfo.StorageBufferNames[j]);
                std::uint32_t  CurrentSet     = BufferInfo.CreateInfo.Set;
                std::uint32_t  CurrentBinding = BufferInfo.CreateInfo.Binding;
                vk::DeviceSize SetOffset      = SetOffsets.at(CurrentSet);
                vk::DeviceSize BindingOffset  = SetOffset + SetInfos.at(CurrentSet).Bindings.at(CurrentBinding).Offset;

                vk::DescriptorType Usage = BufferInfo.CreateInfo.Usage;
                vk::DescriptorAddressInfoEXT AddressInfo(BufferInfo.Buffers[i].GetBuffer().GetDeviceAddress(), BufferInfo.Size);
                vk::DescriptorGetInfoEXT DescriptorGetInfo(Usage, &AddressInfo);
                vk::DeviceSize DescriptorSize = DescriptorBufferProperties_.storageBufferDescriptorSize;
                Device.getDescriptorEXT(DescriptorGetInfo, DescriptorSize, static_cast<std::byte*>(Target) + BindingOffset);

                InsertOffsetMap(BufferInfo.CreateInfo, BindingOffset, Usage);
            }

            for (std::size_t j = 0; j != DescriptorBufferCreateInfo.SamplerInfos.size(); ++j)
            {
                const auto&    SamplerInfo    = DescriptorBufferCreateInfo.SamplerInfos[j];
                std::uint32_t  CurrentSet     = SamplerInfo.Set;
                std::uint32_t  CurrentBinding = SamplerInfo.Binding;
                vk::DeviceSize SetOffset      = SetOffsets.at(CurrentSet);
                vk::DeviceSize BindingOffset  = SetOffset + SetInfos.at(CurrentSet).Bindings.at(CurrentBinding).Offset;

                vk::DescriptorType Usage = vk::DescriptorType::eSampler;
                vk::DescriptorGetInfoEXT DescriptorGetInfo(Usage, &SamplerInfo.Sampler);
                vk::DeviceSize DescriptorSize = DescriptorBufferProperties_.samplerDescriptorSize;
                Device.getDescriptorEXT(DescriptorGetInfo, DescriptorSize, static_cast<std::byte*>(Target) + BindingOffset);

                InsertOffsetMap(SamplerInfo, BindingOffset, Usage);
            }

            for (std::size_t j = 0; j != DescriptorBufferCreateInfo.SampledImageInfos.size(); ++j)
            {
                const auto&    ImageInfo      = DescriptorBufferCreateInfo.SampledImageInfos[j];
                std::uint32_t  CurrentSet     = ImageInfo.Set;
                std::uint32_t  CurrentBinding = ImageInfo.Binding;
                vk::DeviceSize SetOffset      = SetOffsets.at(CurrentSet);
                vk::DeviceSize BindingOffset  = SetOffset + SetInfos.at(CurrentSet).Bindings.at(CurrentBinding).Offset;

                vk::DescriptorType Usage = vk::DescriptorType::eSampledImage;
                vk::DescriptorGetInfoEXT DescriptorGetInfo(Usage, &ImageInfo.Info);
                vk::DeviceSize DescriptorSize = DescriptorBufferProperties_.sampledImageDescriptorSize;
                Device.getDescriptorEXT(DescriptorGetInfo, DescriptorSize, static_cast<std::byte*>(Target) + BindingOffset);

                InsertOffsetMap(ImageInfo, BindingOffset, Usage);
            }

            for (std::size_t j = 0; j != DescriptorBufferCreateInfo.StorageImageInfos.size(); ++j)
            {
                const auto&    ImageInfo      = DescriptorBufferCreateInfo.StorageImageInfos[j];
                std::uint32_t  CurrentSet     = ImageInfo.Set;
                std::uint32_t  CurrentBinding = ImageInfo.Binding;
                vk::DeviceSize SetOffset      = SetOffsets.at(CurrentSet);
                vk::DeviceSize BindingOffset  = SetOffset + SetInfos.at(CurrentSet).Bindings.at(CurrentBinding).Offset;

                vk::DescriptorType Usage = vk::DescriptorType::eStorageImage;
                vk::DescriptorGetInfoEXT DescriptorGetInfo(Usage, &ImageInfo.Info);
                vk::DeviceSize DescriptorSize = DescriptorBufferProperties_.storageImageDescriptorSize;
                Device.getDescriptorEXT(DescriptorGetInfo, DescriptorSize, static_cast<std::byte*>(Target) + BindingOffset);

                InsertOffsetMap(ImageInfo, BindingOffset, Usage);
            }

            for (std::size_t j = 0; j != DescriptorBufferCreateInfo.CombinedImageSamplerInfos.size(); ++j)
            {
                const auto&    ImageInfo      = DescriptorBufferCreateInfo.CombinedImageSamplerInfos[j];
                std::uint32_t  CurrentSet     = ImageInfo.Set;
                std::uint32_t  CurrentBinding = ImageInfo.Binding;
                vk::DeviceSize SetOffset      = SetOffsets.at(CurrentSet);
                vk::DeviceSize BindingOffset  = SetOffset + SetInfos.at(CurrentSet).Bindings.at(CurrentBinding).Offset;

                vk::DescriptorType Usage = vk::DescriptorType::eCombinedImageSampler;
                vk::DescriptorGetInfoEXT DescriptorGetInfo(Usage, &ImageInfo.Info);
                vk::DeviceSize DescriptorSize = DescriptorBufferProperties_.combinedImageSamplerDescriptorSize;
                Device.getDescriptorEXT(DescriptorGetInfo, DescriptorSize, static_cast<std::byte*>(Target) + BindingOffset);

                InsertOffsetMap(ImageInfo, BindingOffset, Usage);
            }
        }
    }
} // namespace Npgs
