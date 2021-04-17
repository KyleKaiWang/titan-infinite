/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#include "pch.h"
#include "buffer.h"

VkResult Buffer::map(VkDeviceSize size, VkDeviceSize offset)
{
    return vkMapMemory(device, memory, offset, size, 0, &mapped);
}

void Buffer::unmap()
{
    if (mapped)
    {
        vkUnmapMemory(device, memory);
        mapped = nullptr;
    }
}

void Buffer::updateDescriptor(VkDeviceSize size, VkDeviceSize offset)
{
    descriptor.buffer = buffer;
    descriptor.range = bufferSize;
    descriptor.offset = offset;
}

void Buffer::flush(VkDeviceSize size, VkDeviceSize offset) {
    VkMappedMemoryRange mappedRange{};
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = memory;
    mappedRange.offset = offset;
    mappedRange.size = size;
    vkFlushMappedMemoryRanges(device, 1, &mappedRange);
}

void Buffer::destroy()
{
    if (mapped) {
        memory::unmap(device, memory);
    }
    if(buffer)
        vkDestroyBuffer(device, buffer, nullptr);
    if(memory)
        vkFreeMemory(device, memory, nullptr);
    buffer = VK_NULL_HANDLE;
    memory = VK_NULL_HANDLE;
}

namespace buffer {

    VkBuffer createBuffer(
        const VkDevice& device, 
        VkDeviceSize size, 
        VkBufferUsageFlags usage, 
        VkMemoryPropertyFlags memoryFlags, 
        VkSharingMode sharingMode
    ) {
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

    Buffer createBuffer(
        Device* device,
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

        Buffer buffer;
        buffer.device = device->getDevice();
        if (vkCreateBuffer(device->getDevice(), &bufferInfo, nullptr, &buffer.buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer!");
        }

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device->getDevice(), buffer.buffer, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = device->findMemoryType(memReqs.memoryTypeBits, memoryFlags);

        // If the buffer has VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT set we also need to enable the appropriate flag during allocation
        VkMemoryAllocateFlagsInfoKHR allocFlagsInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR };
        if (memoryFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
            allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
            allocInfo.pNext = &allocFlagsInfo;
        }

        if (vkAllocateMemory(device->getDevice(), &allocInfo, nullptr, &buffer.memory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate buffer memory!");
        }

        // If a pointer to the buffer data has been passed, map the buffer and copy over the data
        if (data != nullptr)
        {
            memory::map(device->getDevice(), buffer.memory, 0, size, &buffer.mapped);
            memcpy(buffer.mapped, data, size);

            // If host coherency hasn't been requested, do a manual flush to make writes visible
            if ((memoryFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
            {
                buffer.flush(size, 0);
            }
            memory::unmap(device->getDevice(), buffer.memory);
        }
        buffer.bufferSize = size;
        buffer.updateDescriptor();

        
        if (vkBindBufferMemory(device->getDevice(), buffer.buffer, buffer.memory, 0)) {
            throw std::runtime_error("failed to bind buffer memory!");
        }
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

        // If the buffer has VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT set we also need to enable the appropriate flag during allocation
        VkMemoryAllocateFlagsInfoKHR allocFlagsInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR };
        if (memoryFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
            allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
            allocInfo.pNext = &allocFlagsInfo;
        }

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