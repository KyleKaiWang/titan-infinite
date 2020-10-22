/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#include "pch.h"
#include "buffer.h"

namespace buffer {
    
    void initDescriptor(Resource<VkBuffer>& buffer, VkDeviceSize size, VkDeviceSize offset)
    {
        buffer.descriptor.buffer = buffer.resource;
        buffer.descriptor.range = buffer.bufferSize;
        buffer.descriptor.offset = offset;
    }

    void flush(const VkDevice& device, VkDeviceMemory memory, VkDeviceSize size, VkDeviceSize offset) {
        VkMappedMemoryRange mappedRange{};
        mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        mappedRange.memory = memory;
        mappedRange.offset = offset;
        mappedRange.size = size;
        vkFlushMappedMemoryRanges(device, 1, &mappedRange);
    }

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

    Resource<VkBuffer> createBuffer(Device* device,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags memoryFlags,
        VkSharingMode sharingMode,
        void* data
    ) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = sharingMode;

        Resource<VkBuffer> buffer;
        if (vkCreateBuffer(device->getDevice(), &bufferInfo, nullptr, &buffer.resource) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer!");
        }

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device->getDevice(), buffer.resource, &memReqs);
        buffer.bufferSize = memReqs.size;

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = device->findMemoryType(memReqs.memoryTypeBits, memoryFlags);

        if (vkAllocateMemory(device->getDevice(), &allocInfo, nullptr, &buffer.memory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate uniform buffer memory!");
        }

        // If a pointer to the buffer data has been passed, map the buffer and copy over the data
        if (data != nullptr)
        {
            memory::map(device->getDevice(), buffer.memory, 0, size, &buffer.mapped);
            memcpy(buffer.mapped, data, size);

            // If host coherency hasn't been requested, do a manual flush to make writes visible
            if ((memoryFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
            {
                buffer::flush(device->getDevice(), buffer.memory, size, 0);
            }
            memory::unmap(device->getDevice(), buffer.memory);
        }
        initDescriptor(buffer);

        vkBindBufferMemory(device->getDevice(), buffer.resource, buffer.memory, 0);

        return buffer;
    }

    void createBuffer(Device* device,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags memoryFlags,
        VkSharingMode sharingMode,
        VkBuffer* buffer,
        VkDeviceMemory* memory,
        void* data
    ) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = sharingMode;

        if (vkCreateBuffer(device->getDevice(), &bufferInfo, nullptr, buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer!");
        }

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device->getDevice(), *buffer, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = device->findMemoryType(memReqs.memoryTypeBits, memoryFlags);

        if (vkAllocateMemory(device->getDevice(), &allocInfo, nullptr, memory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate uniform buffer memory!");
        }

        // If a pointer to the buffer data has been passed, map the buffer and copy over the data
        if (data != nullptr)
        {
            void* mapped;
            memory::map(device->getDevice(), *memory, 0, size, &mapped);
            memcpy(mapped, data, size);

            // If host coherency hasn't been requested, do a manual flush to make writes visible
            if ((memoryFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
            {
                VkMappedMemoryRange mappedRange{};
                mappedRange.memory = *memory;
                mappedRange.offset = 0;
                mappedRange.size = size;
                vkFlushMappedMemoryRanges(device->getDevice(), 1, &mappedRange);
            }
            memory::unmap(device->getDevice(), *memory);
        }

        vkBindBufferMemory(device->getDevice(), *buffer, *memory, 0);
    }
}