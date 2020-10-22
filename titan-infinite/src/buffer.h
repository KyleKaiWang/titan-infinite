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
    void initDescriptor(Resource<VkBuffer>& buffer, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    void flush(const VkDevice& device, VkDeviceMemory memory, VkDeviceSize size, VkDeviceSize offset);

    VkBuffer createBuffer(const VkDevice& device,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkSharingMode sharingMode);

    Resource<VkBuffer> createBuffer(Device* device,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags memoryFlags,
        VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        void* data = nullptr
    );

    void createBuffer(Device* device,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags memoryFlags,
        VkSharingMode sharingMode,
        VkBuffer* buffer,
        VkDeviceMemory* memory,
        void* data = nullptr
    );
}