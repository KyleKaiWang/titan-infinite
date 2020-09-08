/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#pragma once
#include <vulkan/vulkan.hpp>
#include "device.h"

template<class T>
struct Resource
{
    T resource;
    VkDeviceMemory memory;
    VkDeviceSize allocationSize;
    uint32_t memoryTypeIndex;
};

struct MeshBuffer
{
    Resource<VkBuffer> vertexBuffer;
    Resource<VkBuffer> indexBuffer;
    uint32_t numElements;
};

namespace buffer {

    VkBuffer createBuffer(const VkDevice& device,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkSharingMode sharingMode) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = sharingMode;

        VkBuffer buffer;
        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer!");
        }
        return buffer;
    }

    Resource<VkBuffer> createBuffer(Device& device,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkSharingMode sharingMode, 
        VkMemoryPropertyFlags memoryFlags) {


        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = sharingMode;

        Resource<VkBuffer> buffer;
        if (vkCreateBuffer(device.getDevice(), &bufferInfo, nullptr, &buffer.resource) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer!");
        }

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device.getDevice(), buffer.resource, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = device.findMemoryType(memReqs.memoryTypeBits, memoryFlags);

        if (vkAllocateMemory(device.getDevice(), &allocInfo, nullptr, &buffer.memory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate uniform buffer memory!");
        }
        vkBindBufferMemory(device.getDevice(), buffer.resource, buffer.memory, 0);

        return buffer;
    }

}