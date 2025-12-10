#include "stdafx.h"
#include "ShaderBufferManager.hpp"

#include <exception>
#include <format>
#include <iterator>
#include <stdexcept>
#include <Volk/volk.h>

#include "Engine/Core/Base/Config/EngineConfig.hpp"
#include "Engine/Core/Logger.hpp"

namespace Npgs
{
    namespace
    {
        bool IsPureSamplerSet(const FDescriptorSetInfo& SetInfo)
        {
            if (SetInfo.Bindings.empty())
            {
                return false;
            }

            for (const auto& [Binding, Info] : SetInfo.Bindings)
            {
                if (Info.Type != vk::DescriptorType::eSampler)
                {
                    return false;
                }
            }

            return true;
        }
    }

    void FShaderBufferManager::FHeapAllocator::Initialize(vk::DeviceSize TotalSize)
    {
        TotalSize_ = TotalSize;
        FreeBlocks_.clear();
        FreeBlocks_[0] = TotalSize;
    }

    vk::DeviceSize FShaderBufferManager::FHeapAllocator::Allocate(vk::DeviceSize Size, vk::DeviceSize Alignment)
    {
        for (auto it = FreeBlocks_.begin(); it != FreeBlocks_.end(); ++it)
        {
            vk::DeviceSize BlockOffset = it->first;
            vk::DeviceSize BlockSize   = it->second;

            vk::DeviceSize AlignedOffset = (BlockOffset + Alignment - 1) & ~(Alignment - 1);
            vk::DeviceSize Padding       = AlignedOffset - BlockOffset;
            vk::DeviceSize RequiredSize  = Size + Padding;

            if (BlockSize >= RequiredSize)
            {
                FreeBlocks_.erase(it);
                if (Padding > 0)
                {
                    FreeBlocks_[BlockOffset] = Padding;
                }

                vk::DeviceSize RemainingSize = BlockSize - RequiredSize;
                if (RemainingSize > 0)
                {
                    FreeBlocks_[AlignedOffset + Size] = RemainingSize;
                }

                return AlignedOffset;
            }
        }

        throw std::runtime_error("Heap allocation failed: insufficient memory.");
    }

    void FShaderBufferManager::FHeapAllocator::Free(vk::DeviceSize Offset, vk::DeviceSize Size)
    {
        FreeBlocks_[Offset] = Size;

        auto it     = FreeBlocks_.find(Offset);
        auto NextIt = std::next(it);
        if (NextIt != FreeBlocks_.end())
        {
            if (it->first + it->second == NextIt->first)
            {
                it->second += NextIt->second;
                FreeBlocks_.erase(NextIt);
            }
        }

        if (it != FreeBlocks_.begin())
        {
            auto PrevIt = std::prev(it);
            if (PrevIt->first + PrevIt->second == it->first)
            {
                PrevIt->second += it->second;
                FreeBlocks_.erase(it);
            }
        }
    }

    void FShaderBufferManager::FHeapAllocator::Reset()
    {
        FreeBlocks_.clear();
        FreeBlocks_[0] = TotalSize_;
    }

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

        vk::PhysicalDeviceProperties2 Properties2;
        Properties2.pNext = &DescriptorBufferProperties_;
        VulkanContext_->GetPhysicalDevice().getProperties2(&Properties2);

        InitializeHeaps(64uz * 1024 * 1024, 4uz * 1024 * 1024);
    }

    FShaderBufferManager::~FShaderBufferManager()
    {
        DataBuffers_.clear();
        DescriptorBuffers_.clear();
        ResourceDescriptorHeaps_.clear();
        SamplerDescriptorHeaps_.clear();
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

    void FShaderBufferManager::AllocateDescriptorBuffer(const FDescriptorBufferCreateInfo& DescriptorBufferCreateInfo)
    {
        FDescriptorBufferInfo BufferInfo;
        BufferInfo.Name = DescriptorBufferCreateInfo.Name;

        for (const auto& [SetIndex, SetInfo] : DescriptorBufferCreateInfo.SetInfos)
        {
            bool bPureSampler = IsPureSamplerSet(SetInfo);

            vk::DeviceSize AllocationSize   = SetInfo.Size;
            vk::DeviceSize AllocationOffset = 0;
            vk::DeviceSize Alignment        = DescriptorBufferProperties_.descriptorBufferOffsetAlignment;
            AllocationSize = (AllocationSize + Alignment - 1) & ~(Alignment - 1);

            EHeapType TargetHeap = EHeapType::kResource;
            try
            {
                if (bPureSampler)
                {
                    TargetHeap = EHeapType::kSampler;
                    AllocationOffset = SamplerHeapAllocator_.Allocate(AllocationSize, Alignment);
                }
                else
                {
                    TargetHeap = EHeapType::kResource;
                    AllocationOffset = ResourceHeapAllocator_.Allocate(AllocationSize, Alignment);
                }
            }
            catch (const std::exception& e)
            {
                NpgsCoreError("Heap allocation failed for Set {} in {}: {}", SetIndex, DescriptorBufferCreateInfo.Name, e.what());
                continue;
            }

            BufferInfo.SetAllocations[SetIndex] = { TargetHeap, AllocationOffset, AllocationSize };
        }

        DescriptorBuffers_.emplace(BufferInfo.Name, std::move(BufferInfo));
        BindResourceToDescriptorBuffersInternal(DescriptorBufferCreateInfo);
    }

    void FShaderBufferManager::FreeDescriptorBuffer(std::string_view Name)
    {
        const auto& BufferInfo = GetDescriptorBufferInfo(Name);

        for (const auto& [SetIndex, SetAllocation] : BufferInfo.SetAllocations)
        {
            if (SetAllocation.HeapType == EHeapType::kResource)
            {
                ResourceHeapAllocator_.Free(SetAllocation.Offset, SetAllocation.Size);
            }
            else
            {
                SamplerHeapAllocator_.Free(SetAllocation.Offset, SetAllocation.Size);
            }
        }

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

    void FShaderBufferManager::InitializeHeaps(vk::DeviceSize ResourceHeapSize, vk::DeviceSize SamplerHeapSize)
    {
        ResourceHeapAllocator_.Initialize(ResourceHeapSize);
        SamplerHeapAllocator_.Initialize(SamplerHeapSize);

        ResourceDescriptorHeaps_.reserve(Config::Graphics::kMaxFrameInFlight);
        SamplerDescriptorHeaps_.reserve(Config::Graphics::kMaxFrameInFlight);

        VmaAllocationCreateInfo AllocationCreateInfo
        {
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
            .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        };

        void* TempTarget = nullptr;

        for (std::uint32_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
        {
            auto ResourceHeapUsage = vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT |
                                     vk::BufferUsageFlagBits::eSamplerDescriptorBufferEXT  |
                                     vk::BufferUsageFlagBits::eShaderDeviceAddress         |
                                     vk::BufferUsageFlagBits::eTransferDst;

            vk::BufferCreateInfo ResourceHeapCreateInfo({}, ResourceHeapSize, ResourceHeapUsage);
            ResourceDescriptorHeaps_.emplace_back(VulkanContext_, std::format("ResourceHeap_Frame{}", i), Allocator_,
                                                  AllocationCreateInfo, ResourceHeapCreateInfo);
            ResourceDescriptorHeaps_.back().SetPersistentMapping(true);
            ResourceDescriptorHeaps_.back().GetMemory().MapMemoryForSubmit(0, ResourceHeapSize, TempTarget);

            auto SamplerHeapUsage = vk::BufferUsageFlagBits::eSamplerDescriptorBufferEXT |
                                    vk::BufferUsageFlagBits::eShaderDeviceAddress        |
                                    vk::BufferUsageFlagBits::eTransferDst;

            vk::BufferCreateInfo SamplerHeapCreateInfo({}, SamplerHeapSize, SamplerHeapUsage);
            SamplerDescriptorHeaps_.emplace_back(VulkanContext_, std::format("SamplerHeap_Frame{}", i), Allocator_,
                                                 AllocationCreateInfo, SamplerHeapCreateInfo);
            SamplerDescriptorHeaps_.back().SetPersistentMapping(true);
            SamplerDescriptorHeaps_.back().GetMemory().MapMemoryForSubmit(0, SamplerHeapSize, TempTarget);
        }
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

    const std::pair<vk::DeviceSize, vk::DescriptorType>&
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
        vk::DeviceSize TotalSize = 0;
        for (auto& [Set, Info] : DescriptorBufferCreateInfo.SetInfos)
        {
            TotalSize += Info.Size;
        }

        return TotalSize;
    }

    void FShaderBufferManager::BindResourceToDescriptorBuffersInternal(const FDescriptorBufferCreateInfo& DescriptorBufferCreateInfo)
    {
        auto& BufferInfo = DescriptorBuffers_.at(DescriptorBufferCreateInfo.Name);
        vk::Device Device = VulkanContext_->GetDevice();

        auto& OffsetsSubMapRef = OffsetsMap_[BufferInfo.Name];
        OffsetsSubMapRef.clear();

        for (const auto& [SetIndex, SetLayoutInfo] : DescriptorBufferCreateInfo.SetInfos)
        {
            const auto& SetAllocation = BufferInfo.SetAllocations.at(SetIndex);
            for (const auto& [Binding, BindingLayout] : SetLayoutInfo.Bindings)
            {
                vk::DeviceSize AbsoluteOffset = SetAllocation.Offset + BindingLayout.Offset;
                OffsetsSubMapRef[{ SetIndex, Binding }] = { AbsoluteOffset, BindingLayout.Type };
            }
        }

        for (std::uint32_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
        {
            void* ResourceHeapBase = ResourceDescriptorHeaps_[i].GetMemory().GetMappedTargetMemory();
            void* SamplerHeapBase  = SamplerDescriptorHeaps_[i].GetMemory().GetMappedTargetMemory();

            auto GetTargetAddress = [&](std::uint32_t Set, std::uint32_t Binding) -> std::byte*
            {
                const auto& SetAllocation = BufferInfo.SetAllocations.at(Set);
                vk::DeviceSize AbsoluteOffset = OffsetsSubMapRef.at({ Set, Binding }).first;

                void* Base = (SetAllocation.HeapType == EHeapType::kResource) ? ResourceHeapBase : SamplerHeapBase;
                return static_cast<std::byte*>(Base) + AbsoluteOffset;
            };

            auto GetBufferDescriptors = [&](const auto& Names) -> void
            {
                for (const auto& Name : Names)
                {
                    const auto& DataBuffer = DataBuffers_.at(Name);
                    auto* Target = GetTargetAddress(DataBuffer.CreateInfo.Set, DataBuffer.CreateInfo.Binding);

                    vk::DescriptorAddressInfoEXT AddressInfo(DataBuffer.Buffers[i].GetBuffer().GetDeviceAddress(), DataBuffer.Size);
                    vk::DescriptorGetInfoEXT GetInfo(DataBuffer.CreateInfo.Usage, &AddressInfo);

                    Device.getDescriptorEXT(GetInfo, GetDescriptorSize(DataBuffer.CreateInfo.Usage), Target);
                }
            };

            GetBufferDescriptors(DescriptorBufferCreateInfo.UniformBufferNames);
            GetBufferDescriptors(DescriptorBufferCreateInfo.StorageBufferNames);

            for (const auto& Info : DescriptorBufferCreateInfo.SamplerInfos)
            {
                auto* Target = GetTargetAddress(Info.Set, Info.Binding);
                vk::DescriptorGetInfoEXT GetInfo(vk::DescriptorType::eSampler, &Info.Sampler);
                Device.getDescriptorEXT(GetInfo, GetDescriptorSize(vk::DescriptorType::eSampler), Target);
            }

            auto GetImageDescriptors = [&](const auto& Infos, vk::DescriptorType Type) -> void
            {
                for (const auto& Info : Infos)
                {
                    auto* Target = GetTargetAddress(Info.Set, Info.Binding);
                    vk::DescriptorGetInfoEXT GetInfo(Type, &Info.Info);
                    Device.getDescriptorEXT(GetInfo, GetDescriptorSize(Type), Target);
                }
            };

            GetImageDescriptors(DescriptorBufferCreateInfo.SampledImageInfos, vk::DescriptorType::eSampledImage);
            GetImageDescriptors(DescriptorBufferCreateInfo.StorageImageInfos, vk::DescriptorType::eStorageImage);
            GetImageDescriptors(DescriptorBufferCreateInfo.CombinedImageSamplerInfos, vk::DescriptorType::eCombinedImageSampler);
        }
    }
} // namespace Npgs
