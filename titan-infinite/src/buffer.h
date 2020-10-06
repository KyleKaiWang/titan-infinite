/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#pragma once
#include <vulkan/vulkan.hpp>

template<class T>
struct Resource
{
    T resource;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize bufferSize = 0;
    uint32_t memoryTypeIndex;
    VkDescriptorBufferInfo descriptor;
    void* mapped = nullptr;
};

namespace buffer {

    void flush(const VkDevice& device, VkDeviceMemory memory, VkDeviceSize size, VkDeviceSize offset);

    VkBuffer createBuffer(const VkDevice& device,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkSharingMode sharingMode);

    Resource<VkBuffer> createBuffer(Device* device,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkSharingMode sharingMode,
        VkMemoryPropertyFlags memoryFlags,
        void* data = nullptr
    );

    void createBuffer(Device* device,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkSharingMode sharingMode,
        VkMemoryPropertyFlags memoryFlags,
        VkBuffer* buffer,
        VkDeviceMemory* memory,
        void* data = nullptr
    );
}