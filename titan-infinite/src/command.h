/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#pragma once
#include "device.h"
#include "swapChain.h"
#include <vulkan/vulkan.hpp>

struct Command {

private:
    VkDevice m_device;
public:

	VkCommandPool m_commandPool;
    std::vector<VkCommandBuffer> m_commandBuffers;
    const VkCommandPool& getCommandPool() const { return m_commandPool; }
    const std::vector<VkCommandBuffer>& getCommandBuffers() { return m_commandBuffers; }

    void create(VkDevice device) {
        m_device = device;
    }

    void destroy() {
        vkFreeCommandBuffers(m_device, m_commandPool, static_cast<uint32_t>(m_commandBuffers.size()), m_commandBuffers.data());
        vkDestroyCommandPool(m_device, m_commandPool, NULL);
    }

    void createCommandPool(const VkDevice& device, uint32_t queueFamilyIndices) {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = queueFamilyIndices;
        poolInfo.flags = 0;

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }
    }

    std::vector<VkCommandBuffer> allocate(const VkDevice& device, const VkCommandPool& commandPool, VkCommandBufferLevel level, uint32_t commandBufferCount) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = level;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = commandBufferCount;

        std::vector<VkCommandBuffer> commandBuffers(commandBufferCount);
        vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data());

        return std::move(commandBuffers);
    }

    void beginCommandBuffer(const VkDevice& device, const VkCommandBuffer& cmdBuffer, VkCommandBufferBeginInfo* cmdBufferBeginInfo) {
        vkBeginCommandBuffer(cmdBuffer, cmdBufferBeginInfo);
    }

    void endCommandBuffer(const VkCommandBuffer& commandBuffer) {
        vkEndCommandBuffer(commandBuffer);
    }

    void submitCommandBuffer(const VkQueue& queue, const VkSubmitInfo* submitInfo, const VkFence& fence) {
        vkQueueSubmit(queue, 1, submitInfo, fence ? fence : VK_NULL_HANDLE);
        vkQueueWaitIdle(queue);
    }
};