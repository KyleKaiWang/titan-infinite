/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#include "pch.h"
#include "memory.h"
#include <mutex>

namespace memory {
    std::mutex imageMutex;
    std::mutex memMutex;

    VkMemoryRequirements getMemoryRequirements(const VkDevice& device, const VkImage& image) {
        VkMemoryRequirements requirements;
        vkGetImageMemoryRequirements(device, image, &requirements);
        return requirements;
    }

    VkMemoryRequirements getMemoryRequirements(const VkDevice& device, const VkBuffer& buffer) {
        VkMemoryRequirements requirements;
        vkGetBufferMemoryRequirements(device, buffer, &requirements);
        return requirements;
    }

    VkDeviceMemory allocate(const VkDevice& device, VkDeviceSize allocationSize, uint32_t memoryTypeIndex) {
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = allocationSize;
        allocInfo.memoryTypeIndex = memoryTypeIndex;

        VkDeviceMemory memory;
        if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate memory!");
        }
        return memory;
    }

    void bind(const VkDevice& device, const VkDeviceMemory& memory, VkDeviceSize offset, const VkImage& image) {
        {
            std::lock_guard<std::mutex> lock(imageMutex);
            vkBindImageMemory(device, image, memory, offset);
        }
    }

    void bind(const VkDevice& device, const VkDeviceMemory& memory, VkDeviceSize offset, const VkBuffer& buffer) {
        {
            std::lock_guard<std::mutex> lock(imageMutex);
            vkBindBufferMemory(device, buffer, memory, offset);
        }
    }

    void map(const VkDevice& device, const VkDeviceMemory& memory, VkDeviceSize offset, VkDeviceSize size, void** data) {
        {
            assert(data);

            std::lock_guard<std::mutex> lock(memMutex);
            auto res = vkMapMemory(device, memory, offset, size, 0, data);
            assert(res == VK_SUCCESS);
        }
    }

    void unmap(const VkDevice& device, const VkDeviceMemory& memory) {
        {
            std::lock_guard<std::mutex> lock(memMutex);
            vkUnmapMemory(device, memory);
        }
    }
}