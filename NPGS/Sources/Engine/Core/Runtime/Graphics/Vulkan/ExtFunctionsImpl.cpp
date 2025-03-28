#include "ExtFunctionsImpl.h"

#include <cstddef>
#include <cstdint>

PFN_vkCmdBindDescriptorBuffersEXT            kVkCmdBindDescriptorBuffersExt            = nullptr;
PFN_vkCmdSetDescriptorBufferOffsetsEXT       kVkCmdSetDescriptorBufferOffsetsExt       = nullptr;
PFN_vkCmdSetDescriptorBufferOffsets2EXT      kVkCmdSetDescriptorBufferOffsets2Ext      = nullptr;
PFN_vkCreateDebugUtilsMessengerEXT           kVkCreateDebugUtilsMessengerExt           = nullptr;
PFN_vkDestroyDebugUtilsMessengerEXT          kVkDestroyDebugUtilsMessengerExt          = nullptr;
PFN_vkGetDescriptorEXT                       kVkGetDescriptorExt                       = nullptr;
PFN_vkGetDescriptorSetLayoutSizeEXT          kVkGetDescriptorSetLayoutSizeExt          = nullptr;
PFN_vkGetDescriptorSetLayoutBindingOffsetEXT kVkGetDescriptorSetLayoutBindingOffsetExt = nullptr;
PFN_vkSetHdrMetadataEXT                      kVkSetHdrMetadataExt                      = nullptr;

VKAPI_ATTR void VKAPI_CALL
vkCmdBindDescriptorBuffersEXT(VkCommandBuffer CommandBuffer, std::uint32_t BufferCount,
                              const VkDescriptorBufferBindingInfoEXT* BindingInfos)
{
    kVkCmdBindDescriptorBuffersExt(CommandBuffer, BufferCount, BindingInfos);
}

VKAPI_ATTR void VKAPI_CALL
vkCmdSetDescriptorBufferOffsetsEXT(VkCommandBuffer CommandBuffer, VkPipelineBindPoint PipelineBindPoint, VkPipelineLayout Layout,
                                   std::uint32_t FirstSet, std::uint32_t SetCount, const std::uint32_t* BufferIndices, const VkDeviceSize* Offsets)
{
    kVkCmdSetDescriptorBufferOffsetsExt(CommandBuffer, PipelineBindPoint, Layout, FirstSet, SetCount, BufferIndices, Offsets);
}

VKAPI_ATTR void VKAPI_CALL
vkCmdSetDescriptorBufferOffsets2EXT(VkCommandBuffer CommandBuffer, const VkSetDescriptorBufferOffsetsInfoEXT* SetDescriptorBufferOffsetsInfo)
{
    kVkCmdSetDescriptorBufferOffsets2Ext(CommandBuffer, SetDescriptorBufferOffsetsInfo);
}

VKAPI_ATTR VkResult VKAPI_CALL
vkCreateDebugUtilsMessengerEXT(VkInstance Instance, const VkDebugUtilsMessengerCreateInfoEXT* CreateInfo,
                               const VkAllocationCallbacks* Allocator, VkDebugUtilsMessengerEXT* Messenger)
{
    return kVkCreateDebugUtilsMessengerExt(Instance, CreateInfo, Allocator, Messenger);
}

VKAPI_ATTR void VKAPI_CALL
vkDestroyDebugUtilsMessengerEXT(VkInstance Instance, VkDebugUtilsMessengerEXT Messenger, const VkAllocationCallbacks* Allocator)
{
    kVkDestroyDebugUtilsMessengerExt(Instance, Messenger, Allocator);
}

VKAPI_ATTR void VKAPI_CALL
vkGetDescriptorEXT(VkDevice Device, const VkDescriptorGetInfoEXT* DescriptorInfo, std::size_t DataSize, void* Descriptor)
{
    kVkGetDescriptorExt(Device, DescriptorInfo, DataSize, Descriptor);
}

VKAPI_ATTR void VKAPI_CALL
vkGetDescriptorSetLayoutSizeEXT(VkDevice Device, VkDescriptorSetLayout Layout, VkDeviceSize* LayoutSizeInBytes)
{
    kVkGetDescriptorSetLayoutSizeExt(Device, Layout, LayoutSizeInBytes);
}

VKAPI_ATTR void VKAPI_CALL
vkGetDescriptorSetLayoutBindingOffsetEXT(VkDevice Device, VkDescriptorSetLayout Layout, uint32_t Binding, VkDeviceSize* Offset)
{
    kVkGetDescriptorSetLayoutBindingOffsetExt(Device, Layout, Binding, Offset);
}

VKAPI_ATTR void VKAPI_CALL
vkSetHdrMetadataEXT(VkDevice Device, uint32_t SwapchainCount, const VkSwapchainKHR* Swapchains, const VkHdrMetadataEXT* Metadata)
{
    kVkSetHdrMetadataExt(Device, SwapchainCount, Swapchains, Metadata);
}
