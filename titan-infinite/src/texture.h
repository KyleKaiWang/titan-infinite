/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#pragma once
#include <vulkan/vulkan.hpp>
#include "buffer.h"

struct TextureObject
{
    VkSampler sampler;
    VkImage image;
    VkImageLayout imageLayout;
    VkImageView view;
    
    bool needs_staging;
    VkBuffer buffer;
    VkDeviceSize buffer_size;

    VkDeviceMemory imageMemory;
    VkDeviceMemory bufferMemory;
    int32_t width, height;
    uint32_t layers;
    uint32_t levels;
};

namespace texture {

    VkSampler createSampler(const VkDevice& device,
        VkFilter magFilter, VkFilter minFilter, VkSamplerMipmapMode mipmapMode,
        VkSamplerAddressMode addressModeU, VkSamplerAddressMode addressModeV,
        VkSamplerAddressMode addressModeW, float mipLodBias,
        VkBool32 anisotropyEnable, float maxAnisotropy, VkBool32 compareEnable,
        VkCompareOp compareOp, float minLod, float maxLod,
        VkBorderColor borderColor, VkBool32 unnormalizedCoordinates) {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = magFilter;
        samplerInfo.minFilter = minFilter;
        samplerInfo.mipmapMode = mipmapMode;
        samplerInfo.addressModeU = addressModeU;
        samplerInfo.addressModeV = addressModeV;
        samplerInfo.addressModeW = addressModeW;
        samplerInfo.mipLodBias = mipLodBias;
        samplerInfo.anisotropyEnable = anisotropyEnable;
        samplerInfo.maxAnisotropy = maxAnisotropy;
        samplerInfo.borderColor = borderColor;
        samplerInfo.unnormalizedCoordinates = unnormalizedCoordinates;
        samplerInfo.compareEnable = compareEnable;
        samplerInfo.compareOp = compareOp;
        samplerInfo.minLod = minLod;
        samplerInfo.maxLod = maxLod;

        VkSampler sampler;
        if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture sampler!");
        }
        return sampler;
    }

    void setImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageAspectFlags aspectMask, VkImageLayout old_image_layout,
        VkImageLayout new_image_layout, VkPipelineStageFlags src_stages, VkPipelineStageFlags dest_stages) {

        assert(commandBuffer != VK_NULL_HANDLE);

        VkImageMemoryBarrier image_memory_barrier = {};
        image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        image_memory_barrier.pNext = NULL;
        image_memory_barrier.srcAccessMask = 0;
        image_memory_barrier.dstAccessMask = 0;
        image_memory_barrier.oldLayout = old_image_layout;
        image_memory_barrier.newLayout = new_image_layout;
        image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        image_memory_barrier.image = image;
        image_memory_barrier.subresourceRange.aspectMask = aspectMask;
        image_memory_barrier.subresourceRange.baseMipLevel = 0;
        image_memory_barrier.subresourceRange.levelCount = 1;
        image_memory_barrier.subresourceRange.baseArrayLayer = 0;
        image_memory_barrier.subresourceRange.layerCount = 1;

        switch (old_image_layout) {
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            image_memory_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            image_memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            break;

        default:
            break;
        }

        switch (new_image_layout) {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            image_memory_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            image_memory_barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        default:
            break;
        }

        vkCmdPipelineBarrier(commandBuffer, src_stages, dest_stages, 0, 0, NULL, 0, NULL, 1, &image_memory_barrier);
    }

    TextureObject loadTexture(const std::string& filename, Device& device, const CommandPool& cmdPool, int channels = 4) {
        TextureObject texObj;
        int texChannels;
        stbi_uc* pixels = stbi_load(filename.c_str(), &texObj.width, &texObj.height, &texChannels, channels);
        if (!pixels) {
            throw std::runtime_error("failed to load texture image!");
        }

        VkFormatProperties formatProps;
        vkGetPhysicalDeviceFormatProperties(device.getPhysicalDevice(), VK_FORMAT_R8G8B8A8_UNORM, &formatProps);
        texObj.needs_staging = (!(formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT));
        
        texObj.image = renderer::createImage(
            device.getDevice(),
            0,
            VK_IMAGE_TYPE_2D,
            VK_FORMAT_R8G8B8A8_UNORM,
            { (uint32_t)texObj.width, (uint32_t)texObj.height, 1 },
            1,
            1,
            VK_SAMPLE_COUNT_1_BIT,
            texObj.needs_staging ? VK_IMAGE_TILING_OPTIMAL : VK_IMAGE_TILING_LINEAR,
            texObj.needs_staging ? VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT : VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            texObj.needs_staging ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_PREINITIALIZED
            );

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(device.getDevice(), texObj.image, &memReqs);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.pNext = NULL;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = device.findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(device.getDevice(), &allocInfo, nullptr, &(texObj.imageMemory)) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate buffer memory!");
        }

        if (vkBindImageMemory(device.getDevice(), texObj.image, texObj.imageMemory, 0) != VK_SUCCESS) {
            throw std::runtime_error("Failed to bind image memory");
        }

        VkImageSubresource subres{};
        subres.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subres.mipLevel = 0;
        subres.arrayLayer = 0;

        VkSubresourceLayout layout{};
        void* data;

        if (texObj.needs_staging) {
            texObj.buffer = buffer::createBuffer(
                device.getDevice(), 
                texObj.width * texObj.height * 4,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
                VK_SHARING_MODE_EXCLUSIVE
                );

            VkMemoryRequirements memReqs;
            vkGetBufferMemoryRequirements(device.getDevice(), texObj.buffer, &memReqs);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memReqs.size;
            allocInfo.memoryTypeIndex = device.findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            if (vkAllocateMemory(device.getDevice(), &allocInfo, nullptr, &texObj.bufferMemory) != VK_SUCCESS) {
                throw std::runtime_error("No mappable, coherent memory!");
            }

            if (vkBindBufferMemory(device.getDevice(), texObj.buffer, texObj.bufferMemory, 0) != VK_SUCCESS) {
                throw std::runtime_error("Failed to bind buffer memory");
            }
        }
        else {
            texObj.buffer = VK_NULL_HANDLE;
            texObj.bufferMemory = VK_NULL_HANDLE;

            vkGetImageSubresourceLayout(device.getDevice(), texObj.image, &subres, &layout);
        }

        VkDeviceMemory mappedMemory = texObj.needs_staging ? texObj.bufferMemory : texObj.imageMemory;
        memory::map(device.getDevice(), mappedMemory, 0, memReqs.size, &data);
        memcpy(data, pixels, static_cast<size_t>(texObj.width * texObj.height * 4));
        memory::unmap(device.getDevice(), mappedMemory);

        std::vector<VkCommandBuffer> cmdBuffers = commandBuffer::allocate(device.getDevice(), cmdPool.getCommandPool(), VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmdBuffers[0], &beginInfo);

        if (!texObj.needs_staging) {
            /* If we can use the linear tiled image as a texture, just do it */
            texObj.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            
            texture::setImageLayout(cmdBuffers[0], texObj.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_PREINITIALIZED, texObj.imageLayout,
                VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        }
        else {
            /* Since we're going to blit to the texture image, set its layout to
             * DESTINATION_OPTIMAL */
            texture::setImageLayout(cmdBuffers[0], texObj.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

            VkBufferImageCopy copy_region;
            copy_region.bufferOffset = 0;
            copy_region.bufferRowLength = texObj.width;
            copy_region.bufferImageHeight = texObj.height;
            copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy_region.imageSubresource.mipLevel = 0;
            copy_region.imageSubresource.baseArrayLayer = 0;
            copy_region.imageSubresource.layerCount = 1;
            copy_region.imageOffset.x = 0;
            copy_region.imageOffset.y = 0;
            copy_region.imageOffset.z = 0;
            copy_region.imageExtent.width = texObj.width;
            copy_region.imageExtent.height = texObj.height;
            copy_region.imageExtent.depth = 1;

            /* Put the copy command into the command buffer */
            vkCmdCopyBufferToImage(cmdBuffers[0], texObj.buffer, texObj.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

            /* Set the layout for the texture image from DESTINATION_OPTIMAL to
             * SHADER_READ_ONLY */
            texObj.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            setImageLayout(cmdBuffers[0], texObj.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texObj.imageLayout,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        }

        vkEndCommandBuffer(cmdBuffers[0]);
        texObj.view = renderer::createImageView(device.getDevice(),
            texObj.image,
            VK_IMAGE_VIEW_TYPE_2D,
            VK_FORMAT_R8G8B8A8_SRGB,
            { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
            { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

        texObj.sampler = texture::createSampler(device.getDevice(),
            VK_FILTER_LINEAR,
            VK_FILTER_LINEAR,
            VK_SAMPLER_MIPMAP_MODE_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_REPEAT,
            VK_SAMPLER_ADDRESS_MODE_REPEAT,
            VK_SAMPLER_ADDRESS_MODE_REPEAT,
            0.0,
            VK_TRUE,
            16.0f,
            VK_FALSE,
            VK_COMPARE_OP_ALWAYS,
            0.0,
            0.0,
            VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            VK_FALSE);

        return texObj;
    }
}