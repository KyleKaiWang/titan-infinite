/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#pragma once
#include <vulkan/vulkan.hpp>

namespace memory {
    VkMemoryRequirements getMemoryRequirements(const VkDevice& device, const VkImage& image);
    VkMemoryRequirements getMemoryRequirements(const VkDevice& device, const VkBuffer& buffer);
    VkDeviceMemory allocate(const VkDevice& device, VkDeviceSize allocationSize, uint32_t memoryTypeIndex);
    void bind(const VkDevice& device, const VkDeviceMemory& memory, VkDeviceSize offset, const VkImage& image);
    void bind(const VkDevice& device, const VkDeviceMemory& memory, VkDeviceSize offset, const VkBuffer& buffer);
    void map(const VkDevice& device, const VkDeviceMemory& memory, VkDeviceSize offset, VkDeviceSize size, void** data);
    void unmap(const VkDevice& device, const VkDeviceMemory& memory);
}