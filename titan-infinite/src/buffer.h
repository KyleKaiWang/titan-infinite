/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#pragma once
#include <vulkan/vulkan.hpp>

struct Buffer
{
    VkDevice device;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize bufferSize = 0;
    uint32_t memoryTypeIndex;
    VkDescriptorBufferInfo descriptor;
    void* mapped = nullptr;
    VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    void unmap();
    void updateDescriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    void flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    void destroy();
};

namespace buffer {

    VkBuffer createBuffer(
        const VkDevice& device,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags memoryFlags,
        VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE
    );

    Buffer createBuffer(
        Device* device,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags memoryFlags,
        VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        void* data = nullptr
    );

    void createBuffer(
        Device* device,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags memoryFlags,
        VkSharingMode sharingMode,
        VkBuffer* buffer,
        VkDeviceMemory* memory,
        void* data = nullptr
    );
}