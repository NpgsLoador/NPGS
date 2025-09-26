#include "stdafx.h"
#include "ShaderBufferManager.hpp"

#include "Engine/Core/Base/Config/EngineConfig.hpp"
#include "Engine/Utils/Logger.hpp"

namespace Npgs
{
    FShaderBufferManager::FShaderBufferManager(FVulkanContext* VulkanContext)
        : VulkanContext_(VulkanContext)
    {
        VmaAllocatorCreateInfo AllocatorCreateInfo
        {
            .flags          = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice = VulkanContext_->GetPhysicalDevice(),
            .device         = VulkanContext_->GetDevice(),
            .instance       = VulkanContext_->GetInstance()
        };

        vmaCreateAllocator(&AllocatorCreateInfo, &Allocator_);
    }

    FShaderBufferManager::~FShaderBufferManager()
    {
        DataBuffers_.clear();
        DescriptorBuffers_.clear();
        vmaDestroyAllocator(Allocator_);
    }

    void FShaderBufferManager::RemoveDataBuffer(const std::string& Name)
    {
        auto it = DataBuffers_.find(Name);
        if (it == DataBuffers_.end())
        {
            return;
        }

        DataBuffers_.erase(it);
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

        std::vector<std::byte> EmptyData(BufferSize, std::byte{});

        for (std::uint32_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
        {
            auto BufferUsage = vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT | vk::BufferUsageFlagBits::eShaderDeviceAddress;
            vk::BufferCreateInfo BufferCreateInfo({}, BufferSize, BufferUsage);
            BufferInfo.Buffers.emplace_back(VulkanContext_, Allocator_, AllocationCreateInfo, BufferCreateInfo);
            BufferInfo.Buffers[i].CopyData(0, 0, BufferSize, EmptyData.data());
        }

        DescriptorBuffers_.emplace(DescriptorBufferCreateInfo.Name, std::move(BufferInfo));
        NpgsCoreTrace("Created descriptor buffer \"{}\" with size {} bytes.", DescriptorBufferCreateInfo.Name, BufferSize);

        BindResourceToDescriptorBuffersInternal(DescriptorBufferCreateInfo);
    }

    void FShaderBufferManager::RemoveDescriptorBuffer(const std::string& Name)
    {
        auto it = DescriptorBuffers_.find(Name);
        if (it == DescriptorBuffers_.end())
        {
            return;
        }

        DescriptorBuffers_.erase(it);
    }

    vk::DeviceSize FShaderBufferManager::CalculateDescriptorBufferSize(const FDescriptorBufferCreateInfo& DescriptorBufferCreateInfo)
    {
        static bool kbDescriptorBufferPropertiesGot = false;
        if (!kbDescriptorBufferPropertiesGot)
        {
            vk::PhysicalDeviceProperties2 Properties2;
            Properties2.pNext = &DescriptorBufferProperties_;
            VulkanContext_->GetPhysicalDevice().getProperties2(&Properties2);
            kbDescriptorBufferPropertiesGot = true;
        }

        vk::DeviceSize TotalSize = 0;
        for (auto [Set, Size] : DescriptorBufferCreateInfo.SetSizes)
        {
            TotalSize += Size;
        }

        return TotalSize;
    }

    void FShaderBufferManager::BindResourceToDescriptorBuffersInternal(const FDescriptorBufferCreateInfo& DescriptorBufferCreateInfo)
    {
        auto& DescriptorBufferInfo = DescriptorBuffers_.at(DescriptorBufferCreateInfo.Name);
        vk::Device Device = VulkanContext_->GetDevice();

        auto InsertOffsetMap = [this, &DescriptorBufferCreateInfo](const auto& Info, vk::DeviceSize Offset) -> void
        {
            std::uint32_t Set     = Info.Set;
            std::uint32_t Binding = Info.Binding;
            OffsetsMap_[DescriptorBufferCreateInfo.Name].try_emplace(std::make_pair(Set, Binding), Offset);
        };

        std::map<std::uint32_t, vk::DeviceSize> SetOffsets;
        for (auto [Set, Size] : DescriptorBufferCreateInfo.SetSizes)
        {
            if (Set > DescriptorBufferCreateInfo.SetSizes.begin()->first)
            {
                SetOffsets[Set] = SetOffsets[Set - 1] + DescriptorBufferCreateInfo.SetSizes.at(Set - 1);
            }
            else
            {
                SetOffsets[Set] = 0;
            }
        }

        for (std::uint32_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
        {
            void* Target = nullptr;
            auto& BufferMemory = DescriptorBufferInfo.Buffers[i].GetMemory();
            BufferMemory.MapMemoryForSubmit(0, DescriptorBufferInfo.Size, Target);

            vk::DeviceSize Offset  = 0;
            std::uint32_t  PrevSet = 0;

            for (std::size_t j = 0; j != DescriptorBufferCreateInfo.UniformBufferNames.size(); ++j)
            {
                const auto&   BufferInfo = DataBuffers_.at(DescriptorBufferCreateInfo.UniformBufferNames[j]);
                std::uint32_t CurrentSet = BufferInfo.CreateInfo.Set;

                if (CurrentSet != PrevSet && CurrentSet < DescriptorBufferCreateInfo.SetSizes.size())
                {
                    Offset  = SetOffsets[CurrentSet];
                    PrevSet = CurrentSet;
                }

                vk::DescriptorAddressInfoEXT AddressInfo(BufferInfo.Buffers[i].GetBuffer().GetDeviceAddress(), BufferInfo.Size);
                vk::DescriptorGetInfoEXT DescriptorInfo(BufferInfo.CreateInfo.Usage, &AddressInfo);
                vk::DeviceSize DescriptorSize = DescriptorBufferProperties_.uniformBufferDescriptorSize;
                Device.getDescriptorEXT(DescriptorInfo, DescriptorSize, static_cast<std::byte*>(Target) + Offset);

                InsertOffsetMap(BufferInfo.CreateInfo, Offset);
                Offset += DescriptorSize;
            }

            for (std::size_t j = 0; j != DescriptorBufferCreateInfo.StorageBufferNames.size(); ++j)
            {
                const auto&   BufferInfo = DataBuffers_.at(DescriptorBufferCreateInfo.StorageBufferNames[j]);
                std::uint32_t CurrentSet = BufferInfo.CreateInfo.Set;

                if (CurrentSet != PrevSet && CurrentSet < DescriptorBufferCreateInfo.SetSizes.size())
                {
                    Offset  = SetOffsets[CurrentSet];
                    PrevSet = CurrentSet;
                }

                vk::DescriptorAddressInfoEXT AddressInfo(BufferInfo.Buffers[i].GetBuffer().GetDeviceAddress(), BufferInfo.Size);
                vk::DescriptorGetInfoEXT DescriptorInfo(BufferInfo.CreateInfo.Usage, &AddressInfo);
                vk::DeviceSize DescriptorSize = DescriptorBufferProperties_.storageBufferDescriptorSize;
                Device.getDescriptorEXT(DescriptorInfo, DescriptorSize, static_cast<std::byte*>(Target) + Offset);

                InsertOffsetMap(BufferInfo.CreateInfo, Offset);
                Offset += DescriptorSize;
            }

            for (std::size_t j = 0; j != DescriptorBufferCreateInfo.SamplerInfos.size(); ++j)
            {
                const auto&  SamplerInfo = DescriptorBufferCreateInfo.SamplerInfos[j];
                std::uint32_t CurrentSet = SamplerInfo.Set;

                if (CurrentSet != PrevSet && CurrentSet < DescriptorBufferCreateInfo.SetSizes.size())
                {
                    Offset  = SetOffsets[CurrentSet];
                    PrevSet = CurrentSet;
                }

                vk::DescriptorGetInfoEXT DescriptorInfo(vk::DescriptorType::eSampler, &SamplerInfo.Sampler);
                vk::DeviceSize DescriptorSize = DescriptorBufferProperties_.samplerDescriptorSize;
                Device.getDescriptorEXT(DescriptorInfo, DescriptorSize, static_cast<std::byte*>(Target) + Offset);

                InsertOffsetMap(SamplerInfo, Offset);
                Offset += DescriptorSize;
            }

            for (std::size_t j = 0; j != DescriptorBufferCreateInfo.SampledImageInfos.size(); ++j)
            {
                const auto&   ImageInfo  = DescriptorBufferCreateInfo.SampledImageInfos[j];
                std::uint32_t CurrentSet = ImageInfo.Set;

                if (CurrentSet != PrevSet && CurrentSet < DescriptorBufferCreateInfo.SetSizes.size())
                {
                    Offset  = SetOffsets[CurrentSet];
                    PrevSet = CurrentSet;
                }

                vk::DescriptorGetInfoEXT DescriptorInfo(vk::DescriptorType::eSampledImage, &ImageInfo.Info);
                vk::DeviceSize DescriptorSize = DescriptorBufferProperties_.sampledImageDescriptorSize;
                Device.getDescriptorEXT(DescriptorInfo, DescriptorSize, static_cast<std::byte*>(Target) + Offset);

                InsertOffsetMap(ImageInfo, Offset);
                Offset += DescriptorSize;
            }

            for (std::size_t j = 0; j != DescriptorBufferCreateInfo.StorageImageInfos.size(); ++j)
            {
                const auto&   ImageInfo  = DescriptorBufferCreateInfo.StorageImageInfos[j];
                std::uint32_t CurrentSet = ImageInfo.Set;

                if (CurrentSet != PrevSet && CurrentSet < DescriptorBufferCreateInfo.SetSizes.size())
                {
                    Offset  = SetOffsets[CurrentSet];
                    PrevSet = CurrentSet;
                }

                vk::DescriptorGetInfoEXT DescriptorInfo(vk::DescriptorType::eStorageImage, &ImageInfo.Info);
                vk::DeviceSize DescriptorSize = DescriptorBufferProperties_.storageImageDescriptorSize;
                Device.getDescriptorEXT(DescriptorInfo, DescriptorSize, static_cast<std::byte*>(Target) + Offset);

                InsertOffsetMap(ImageInfo, Offset);
                Offset += DescriptorSize;
            }

            for (std::size_t j = 0; j != DescriptorBufferCreateInfo.CombinedImageSamplerInfos.size(); ++j)
            {
                const auto&   ImageInfo  = DescriptorBufferCreateInfo.CombinedImageSamplerInfos[j];
                std::uint32_t CurrentSet = ImageInfo.Set;

                if (CurrentSet != PrevSet && CurrentSet < DescriptorBufferCreateInfo.SetSizes.size())
                {
                    Offset  = SetOffsets[CurrentSet];
                    PrevSet = CurrentSet;
                }

                vk::DescriptorGetInfoEXT DescriptorInfo(vk::DescriptorType::eCombinedImageSampler, &ImageInfo.Info);
                vk::DeviceSize DescriptorSize = DescriptorBufferProperties_.combinedImageSamplerDescriptorSize;
                Device.getDescriptorEXT(DescriptorInfo, DescriptorSize, static_cast<std::byte*>(Target) + Offset);

                InsertOffsetMap(ImageInfo, Offset);
                Offset += DescriptorSize;
            }

            BufferMemory.UnmapMemory(0, DescriptorBufferInfo.Size);
        }
    }
} // namespace Npgs
