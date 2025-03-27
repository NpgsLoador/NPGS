#include "ShaderBufferManager.h"

#include <algorithm>
#include <array>
#include <map>
#include <utility>

#include "Engine/Core/Runtime/AssetLoaders/AssetManager.h"
#include "Engine/Core/Runtime/AssetLoaders/Shader.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"
#include "Engine/Utils/Logger.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

FShaderBufferManager::FShaderBufferManager()
    : _Allocator(FVulkanContext::GetClassInstance()->GetVmaAllocator())
{
}

void FShaderBufferManager::CreateDescriptorBuffer(const FDescriptorBufferCreateInfo& DescriptorBufferCreateInfo,
                                                  const VmaAllocationCreateInfo* AllocationCreateInfo)
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

        if (AllocationCreateInfo != nullptr)
        {
            vk::BufferCreateInfo BufferCreateInfo({}, BufferSize, BufferUsage);
            BufferInfo.Buffers.emplace_back(_Allocator, *AllocationCreateInfo, BufferCreateInfo);
        }
        else
        {
            BufferInfo.Buffers.emplace_back(BufferSize, BufferUsage);
        }

        BufferInfo.Buffers[i].CopyData(0, 0, BufferSize, EmptyData.data());
    }

    _DescriptorBuffers.emplace(DescriptorBufferCreateInfo.Name, std::move(BufferInfo));
    NpgsCoreTrace("Created descriptor buffer \"{}\" with size {} bytes.", DescriptorBufferCreateInfo.Name, BufferSize);

    BindResourceToDescriptorBuffersInternal(DescriptorBufferCreateInfo);
}

FShaderBufferManager* FShaderBufferManager::GetInstance()
{
    static FShaderBufferManager kInstance;
    return &kInstance;
}

vk::DeviceSize FShaderBufferManager::CalculateDescriptorBufferSize(const FDescriptorBufferCreateInfo& DescriptorBufferCreateInfo)
{
    static bool kbDescriptorBufferPropertiesGot = false;
    if (!kbDescriptorBufferPropertiesGot)
    {
        vk::PhysicalDeviceProperties2 Properties2;
        Properties2.pNext = &_DescriptorBufferProperties;
        FVulkanContext::GetClassInstance()->GetPhysicalDevice().getProperties2(&Properties2);
        kbDescriptorBufferPropertiesGot = true;
    }

    vk::DeviceSize UniformBufferDescriptorSize        = _DescriptorBufferProperties.uniformBufferDescriptorSize;
    vk::DeviceSize StorageBufferDescriptorSize        = _DescriptorBufferProperties.storageBufferDescriptorSize;
    vk::DeviceSize SamplerInfoDescriptorSize          = _DescriptorBufferProperties.samplerDescriptorSize;
    vk::DeviceSize SampledImageDescriptorSize         = _DescriptorBufferProperties.sampledImageDescriptorSize;
    vk::DeviceSize StorageImageDescriptorSize         = _DescriptorBufferProperties.storageImageDescriptorSize;
    vk::DeviceSize CombinedImageSamplerDescriptorSize = _DescriptorBufferProperties.combinedImageSamplerDescriptorSize;

    std::unordered_map<std::uint32_t, vk::DeviceSize> SetSizes;

    auto AddToSetSize = [&SetSizes](std::uint32_t Set, vk::DeviceSize Size) -> void {
        SetSizes[Set] += Size;
    };

    for (const auto& UniformBuffer : DescriptorBufferCreateInfo.UniformBuffers)
    {
        const auto& BufferInfo = _DataBuffers.at(UniformBuffer);
        AddToSetSize(BufferInfo.CreateInfo.Set, UniformBufferDescriptorSize);
    }

    for (const auto& StorageBuffer : DescriptorBufferCreateInfo.StorageBuffers)
    {
        const auto& BufferInfo = _DataBuffers.at(StorageBuffer);
        AddToSetSize(BufferInfo.CreateInfo.Set, StorageBufferDescriptorSize);
    }

    for (const auto& SamplerInfo : DescriptorBufferCreateInfo.SamplerInfos)
    {
        AddToSetSize(SamplerInfo.Set, SamplerInfoDescriptorSize);
    }

    for (const auto& ImageInfo : DescriptorBufferCreateInfo.SampledImageInfos)
    {
        AddToSetSize(ImageInfo.Set, SampledImageDescriptorSize);
    }

    for (const auto& ImageInfo : DescriptorBufferCreateInfo.StorageImageInfos)
    {
        AddToSetSize(ImageInfo.Set, StorageImageDescriptorSize);
    }

    for (const auto& ImageInfo : DescriptorBufferCreateInfo.CombinedImageSamplerInfos)
    {
        AddToSetSize(ImageInfo.Set, CombinedImageSamplerDescriptorSize);
    }

    const vk::DeviceSize kSetAlignment = _DescriptorBufferProperties.descriptorBufferOffsetAlignment * 4;
    vk::DeviceSize TotalSize = 0;

    for (const auto& [Set, Size] : SetSizes)
    {
        // 计算需要对齐到的大小
        vk::DeviceSize AlignedSize = ((Size + kSetAlignment - 1) / kSetAlignment) * kSetAlignment;
        TotalSize += AlignedSize;
    }

    return TotalSize;
}

void FShaderBufferManager::BindResourceToDescriptorBuffersInternal(const FDescriptorBufferCreateInfo& DescriptorBufferCreateInfo)
{
    auto& DescriptorBufferInfo = _DescriptorBuffers.at(DescriptorBufferCreateInfo.Name);
    vk::Device Device = FVulkanContext::GetClassInstance()->GetDevice();

    std::map<std::uint32_t, vk::DeviceSize> SetOffsets;
    const vk::DeviceSize kSetAlignment = _DescriptorBufferProperties.descriptorBufferOffsetAlignment * 4;

    auto InsertOffsetMap = [this, &DescriptorBufferCreateInfo](const auto& Info, vk::DeviceSize AppendOffset) -> void
    {
        std::uint32_t Set     = Info.Set;
        std::uint32_t Binding = Info.Binding;
        _OffsetsMap[DescriptorBufferCreateInfo.Name].emplace(std::make_pair(Set, Binding), AppendOffset);
    };

    auto GetAlignOffset = [kSetAlignment](vk::DeviceSize CurrentOffset, vk::DeviceSize CurrentSet,
                                          const std::map<uint32_t, vk::DeviceSize>& SetOffsets)
    {
        if (SetOffsets.empty() || SetOffsets.rbegin()->first != CurrentSet)
        {
            return ((CurrentOffset + kSetAlignment - 1) / kSetAlignment) * kSetAlignment;
        }
        return CurrentOffset;
    };

    for (std::uint32_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
    {
        void* Target = nullptr;
        auto& BufferMemory = DescriptorBufferInfo.Buffers[i].GetMemory();
        BufferMemory.MapMemoryForSubmit(0, DescriptorBufferInfo.Size, Target);
        vk::DeviceSize AppendOffset = 0;
        SetOffsets.clear();

        for (std::size_t j = 0; j != DescriptorBufferCreateInfo.UniformBuffers.size(); ++j)
        {
            const auto& BufferInfo = _DataBuffers.at(DescriptorBufferCreateInfo.UniformBuffers[j]);
            std::uint32_t CurrentSet = BufferInfo.CreateInfo.Set;

            AppendOffset = GetAlignOffset(AppendOffset, CurrentSet, SetOffsets);

            vk::DescriptorAddressInfoEXT AddressInfo(BufferInfo.Buffers[i].GetBuffer().GetDeviceAddress(), BufferInfo.Size);
            vk::DescriptorGetInfoEXT DescriptorInfo(BufferInfo.CreateInfo.Usage, &AddressInfo);
            vk::DeviceSize DescriptorSize = _DescriptorBufferProperties.uniformBufferDescriptorSize;
            Device.getDescriptorEXT(DescriptorInfo, DescriptorSize, static_cast<std::byte*>(Target) + AppendOffset);

            InsertOffsetMap(BufferInfo.CreateInfo, AppendOffset);
            AppendOffset += DescriptorSize;
            SetOffsets[CurrentSet] = AppendOffset;
        }

        for (std::size_t j = 0; j != DescriptorBufferCreateInfo.StorageBuffers.size(); ++j)
        {
            const auto& BufferInfo = _DataBuffers.at(DescriptorBufferCreateInfo.StorageBuffers[j]);
            std::uint32_t CurrentSet = BufferInfo.CreateInfo.Set;

            AppendOffset = GetAlignOffset(AppendOffset, CurrentSet, SetOffsets);

            vk::DescriptorAddressInfoEXT AddressInfo(BufferInfo.Buffers[i].GetBuffer().GetDeviceAddress(), BufferInfo.Size);
            vk::DescriptorGetInfoEXT DescriptorInfo(BufferInfo.CreateInfo.Usage, &AddressInfo);
            vk::DeviceSize DescriptorSize = _DescriptorBufferProperties.storageBufferDescriptorSize;
            Device.getDescriptorEXT(DescriptorInfo, DescriptorSize, static_cast<std::byte*>(Target) + AppendOffset);

            InsertOffsetMap(BufferInfo.CreateInfo, AppendOffset);
            AppendOffset += DescriptorSize;
            SetOffsets[CurrentSet] = AppendOffset;
        }

        for (std::size_t j = 0; j != DescriptorBufferCreateInfo.SamplerInfos.size(); ++j)
        {
            const auto& SamplerInfo = DescriptorBufferCreateInfo.SamplerInfos[j];
            AppendOffset = GetAlignOffset(AppendOffset, SamplerInfo.Set, SetOffsets);

            vk::DescriptorGetInfoEXT DescriptorInfo(vk::DescriptorType::eSampler, &SamplerInfo.Info);
            vk::DeviceSize DescriptorSize = _DescriptorBufferProperties.samplerDescriptorSize;
            Device.getDescriptorEXT(DescriptorInfo, DescriptorSize, static_cast<std::byte*>(Target) + AppendOffset);

            InsertOffsetMap(SamplerInfo, AppendOffset);
            AppendOffset += DescriptorSize;
            SetOffsets[SamplerInfo.Set] = AppendOffset;
        }

        for (std::size_t j = 0; j != DescriptorBufferCreateInfo.SampledImageInfos.size(); ++j)
        {
            const auto& ImageInfo = DescriptorBufferCreateInfo.SampledImageInfos[j];
            AppendOffset = GetAlignOffset(AppendOffset, ImageInfo.Set, SetOffsets);

            vk::DescriptorGetInfoEXT DescriptorInfo(vk::DescriptorType::eSampledImage, &ImageInfo.Info);
            vk::DeviceSize DescriptorSize = _DescriptorBufferProperties.sampledImageDescriptorSize;
            Device.getDescriptorEXT(DescriptorInfo, DescriptorSize, static_cast<std::byte*>(Target) + AppendOffset);

            InsertOffsetMap(ImageInfo, AppendOffset);
            AppendOffset += DescriptorSize;
            SetOffsets[ImageInfo.Set] = AppendOffset;
        }

        for (std::size_t j = 0; j != DescriptorBufferCreateInfo.StorageImageInfos.size(); ++j)
        {
            const auto& ImageInfo = DescriptorBufferCreateInfo.StorageImageInfos[j];
            AppendOffset = GetAlignOffset(AppendOffset, ImageInfo.Set, SetOffsets);

            vk::DescriptorGetInfoEXT DescriptorInfo(vk::DescriptorType::eStorageImage, &ImageInfo.Info);
            vk::DeviceSize DescriptorSize = _DescriptorBufferProperties.storageImageDescriptorSize;
            Device.getDescriptorEXT(DescriptorInfo, DescriptorSize, static_cast<std::byte*>(Target) + AppendOffset);

            InsertOffsetMap(ImageInfo, AppendOffset);
            AppendOffset += DescriptorSize;
            SetOffsets[ImageInfo.Set] = AppendOffset;
        }

        for (std::size_t j = 0; j != DescriptorBufferCreateInfo.CombinedImageSamplerInfos.size(); ++j)
        {
            const auto& ImageInfo = DescriptorBufferCreateInfo.CombinedImageSamplerInfos[j];
            AppendOffset = GetAlignOffset(AppendOffset, ImageInfo.Set, SetOffsets);

            vk::DescriptorGetInfoEXT DescriptorInfo(vk::DescriptorType::eCombinedImageSampler, &ImageInfo.Info);
            vk::DeviceSize DescriptorSize = _DescriptorBufferProperties.combinedImageSamplerDescriptorSize;
            Device.getDescriptorEXT(DescriptorInfo, DescriptorSize, static_cast<std::byte*>(Target) + AppendOffset);

            InsertOffsetMap(ImageInfo, AppendOffset);
            AppendOffset += DescriptorSize;
            SetOffsets[ImageInfo.Set] = AppendOffset;
        }

        BufferMemory.UnmapMemory(0, DescriptorBufferInfo.Size);
    }
}

_GRAPHICS_END
_RUNTIME_END
_NPGS_END
