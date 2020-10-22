/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#include "pch.h"
#include "texture.h"

namespace texture {

    TextureObject createTexture(
        Device* device, 
        uint32_t width, 
        uint32_t height, 
        uint32_t layers, 
        VkFormat format, 
        uint32_t mipLevels, 
        VkImageUsageFlags imageUsageFlags, 
        VkSampler sampler)
    {
        assert(width > 0 && height > 0);
        assert(layers > 0);

        TextureObject texObj;
        texObj.width = width;
        texObj.height = height;
        texObj.layers = layers;
        texObj.mipLevels = (mipLevels > 0) ? mipLevels : texture::numMipmapLevels(width, height);

        VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | imageUsageFlags;
        if (texObj.mipLevels > 1) {
            usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        }
        texObj.image = renderer::createImage(
            device->getDevice(),
            (layers == 6) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0,
            VK_IMAGE_TYPE_2D,
            format,
            { (uint32_t)texObj.width, (uint32_t)texObj.height, 1 },
            1,
            layers,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_TILING_OPTIMAL,
            (imageUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) ? imageUsageFlags : (imageUsageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT),
            VK_SHARING_MODE_EXCLUSIVE,
            VK_IMAGE_LAYOUT_UNDEFINED
        );

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(device->getDevice(), texObj.image, &memReqs);

        VkMemoryAllocateInfo memAllocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        memAllocInfo.allocationSize = memReqs.size;
        memAllocInfo.memoryTypeIndex = device->findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(device->getDevice(), &memAllocInfo, nullptr, &texObj.image_memory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate staging buffer memory!");
        }

        if (vkBindImageMemory(device->getDevice(), texObj.image, texObj.image_memory, 0) != VK_SUCCESS) {
            throw std::runtime_error("Failed to bind staging buffer memory");
        }
        texObj.buffer_size = memReqs.size;

        texObj.view = renderer::createImageView(device->getDevice(),
            texObj.image,
            (texObj.layers == 6) ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D,
            format,
            { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
            { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS }
        );

        texObj.descriptor.imageLayout = texObj.image_layout;
        texObj.descriptor.imageView = texObj.view;
        texObj.descriptor.sampler = sampler;
        return texObj;
    }

    TextureObject loadTexture(
        const std::string& filename, 
        VkFormat format, 
        Device* device, 
        int num_requested_components) {
        TextureObject texObj;
        int width, height, num_components, imageDataSize;
        bool is_hdr;
        texObj.data.reset(texture::loadTextureData(filename.c_str(), num_requested_components, &width, &height, &num_components, &imageDataSize, &is_hdr));
        texObj.width = width;
        texObj.height = height;
        texObj.num_components = num_components;
        texObj.is_hdr = is_hdr;

        VkFormatProperties formatProps;
        vkGetPhysicalDeviceFormatProperties(device->getPhysicalDevice(), format, &formatProps);
        texObj.needs_staging = (!(formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT));

        texObj.image = renderer::createImage(
            device->getDevice(),
            0,
            VK_IMAGE_TYPE_2D,
            format,
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
        vkGetImageMemoryRequirements(device->getDevice(), texObj.image, &memReqs);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.pNext = NULL;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = device->findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(device->getDevice(), &allocInfo, nullptr, &(texObj.image_memory)) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate buffer memory!");
        }

        if (vkBindImageMemory(device->getDevice(), texObj.image, texObj.image_memory, 0) != VK_SUCCESS) {
            throw std::runtime_error("Failed to bind image memory");
        }

        VkImageSubresource subres{};
        subres.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subres.mipLevel = 0;
        subres.arrayLayer = 0;

        VkSubresourceLayout layout{};
        if (texObj.needs_staging) {
            texObj.buffer = buffer::createBuffer(
                device->getDevice(),
                imageDataSize,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_SHARING_MODE_EXCLUSIVE
            );

            VkMemoryRequirements memReqs;
            vkGetBufferMemoryRequirements(device->getDevice(), texObj.buffer, &memReqs);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memReqs.size;
            allocInfo.memoryTypeIndex = device->findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            if (vkAllocateMemory(device->getDevice(), &allocInfo, nullptr, &texObj.buffer_memory) != VK_SUCCESS) {
                throw std::runtime_error("No mappable, coherent memory!");
            }

            if (vkBindBufferMemory(device->getDevice(), texObj.buffer, texObj.buffer_memory, 0) != VK_SUCCESS) {
                throw std::runtime_error("Failed to bind buffer memory");
            }
        }
        else {
            texObj.buffer = VK_NULL_HANDLE;
            texObj.buffer_memory = VK_NULL_HANDLE;

            vkGetImageSubresourceLayout(device->getDevice(), texObj.image, &subres, &layout);
        }

        VkDeviceMemory mappedMemory = texObj.needs_staging ? texObj.buffer_memory : texObj.image_memory;
        void* data;
        memory::map(device->getDevice(), mappedMemory, 0, memReqs.size, &data);
        memcpy(data, &texObj.data, imageDataSize);
        memory::unmap(device->getDevice(), mappedMemory);

        std::vector<VkCommandBuffer> cmdBuffers = device->createCommandBuffers(device->getDevice(), device->getCommandPool(), VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmdBuffers[0], &beginInfo);

        if (!texObj.needs_staging) {
            /* If we can use the linear tiled image as a texture, just do it */
            texObj.image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            texture::setImageLayout(cmdBuffers[0], texObj.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_PREINITIALIZED, texObj.image_layout,
                VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        }
        else {
            /* Since we're going to blit to the texture image, set its layout to DESTINATION_OPTIMAL */
            texture::setImageLayout(cmdBuffers[0], texObj.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, {});

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

            /* Set the layout for the texture image from DESTINATION_OPTIMAL to SHADER_READ_ONLY */
            texObj.image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            setImageLayout(cmdBuffers[0], texObj.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texObj.image_layout,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        }

        vkEndCommandBuffer(cmdBuffers[0]);
        texObj.view = renderer::createImageView(device->getDevice(),
            texObj.image,
            VK_IMAGE_VIEW_TYPE_2D,
            format,
            { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
            { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

        texObj.sampler = texture::createSampler(device->getDevice(),
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

        texObj.descriptor.imageLayout = texObj.image_layout;
        texObj.descriptor.imageView = texObj.view;
        texObj.descriptor.sampler = texObj.sampler;

        if (texObj.needs_staging) {
            /* Release the resources for the staging image */
            vkFreeMemory(device->getDevice(), texObj.buffer_memory, NULL);
            vkDestroyBuffer(device->getDevice(), texObj.buffer, NULL);
        }
        return texObj;
    }

    TextureObject loadTexture(
        void* buffer, 
        VkDeviceSize bufferSize, 
        VkFormat format, 
        uint32_t texWidth, 
        uint32_t texHeight, 
        Device* device, 
        VkQueue copyQueue, 
        VkFilter filter, 
        VkImageUsageFlags imageUsageFlags, 
        VkImageLayout image_layout)
    {
        assert(buffer);

        TextureObject texObj;
        texObj.width = texWidth;
        texObj.height = texHeight;

        VkMemoryAllocateInfo memAllocInfo{};
        memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        VkMemoryRequirements memReqs;

        VkCommandBuffer copyCmd = device->createCommandBuffer(device->getDevice(), device->getCommandPool(), VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        stagingBuffer = buffer::createBuffer(
            device->getDevice(),
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_SHARING_MODE_EXCLUSIVE
        );

        vkGetBufferMemoryRequirements(device->getDevice(), stagingBuffer, &memReqs);

        memAllocInfo.allocationSize = memReqs.size;
        memAllocInfo.memoryTypeIndex = device->findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(device->getDevice(), &memAllocInfo, nullptr, &stagingMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate staging buffer memory!");
        }

        if (vkBindBufferMemory(device->getDevice(), stagingBuffer, stagingMemory, 0) != VK_SUCCESS) {
            throw std::runtime_error("Failed to bind staging buffer memory");
        }

        void* data;
        memory::map(device->getDevice(), stagingMemory, 0, memReqs.size, (void**)&data);
        memcpy(data, buffer, bufferSize);
        memory::unmap(device->getDevice(), stagingMemory);

        VkBufferImageCopy bufferCopyRegion{};
        bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bufferCopyRegion.imageSubresource.mipLevel = 0;
        bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
        bufferCopyRegion.imageSubresource.layerCount = 1;
        bufferCopyRegion.imageExtent.width = texObj.width;
        bufferCopyRegion.imageExtent.height = texObj.height;
        bufferCopyRegion.imageExtent.depth = 1;
        bufferCopyRegion.bufferOffset = 0;

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

        texObj.image = renderer::createImage(
            device->getDevice(),
            0,
            VK_IMAGE_TYPE_2D,
            format,
            { (uint32_t)texObj.width, (uint32_t)texObj.height, 1 },
            1,
            1,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_TILING_OPTIMAL,
            (imageUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) ? imageUsageFlags : (imageUsageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT),
            VK_SHARING_MODE_EXCLUSIVE,
            VK_IMAGE_LAYOUT_UNDEFINED
        );

        vkGetImageMemoryRequirements(device->getDevice(), texObj.image, &memReqs);

        memAllocInfo.allocationSize = memReqs.size;
        memAllocInfo.memoryTypeIndex = device->findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(device->getDevice(), &memAllocInfo, nullptr, &texObj.buffer_memory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate buffer memory!");
        }

        if (vkBindImageMemory(device->getDevice(), texObj.image, texObj.buffer_memory, 0) != VK_SUCCESS) {
            throw std::runtime_error("Failed to bind image memory");
        }

        // Image barrier for optimal image (target)
        // Optimal image will be used as destination for the copy
        // Create an image barrier object
        texture::setImageLayout(copyCmd,
            texObj.image,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        );

        // Copy mip levels from staging buffer
        vkCmdCopyBufferToImage(
            copyCmd,
            stagingBuffer,
            texObj.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &bufferCopyRegion
        );

        // Change texture image layout to shader read after all mip levels have been copied
        texObj.image_layout = image_layout;
        texture::setImageLayout(copyCmd,
            texObj.image,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            texObj.image_layout,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        );

        //device->flushCommandBuffer(copyCmd, copyQueue);

        vkFreeMemory(device->getDevice(), stagingMemory, nullptr);
        vkDestroyBuffer(device->getDevice(), stagingBuffer, nullptr);

        texObj.view = renderer::createImageView(device->getDevice(),
            texObj.image,
            VK_IMAGE_VIEW_TYPE_2D,
            format,
            { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
            { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

        texObj.sampler = texture::createSampler(device->getDevice(),
            filter,
            filter,
            VK_SAMPLER_MIPMAP_MODE_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_REPEAT,
            VK_SAMPLER_ADDRESS_MODE_REPEAT,
            VK_SAMPLER_ADDRESS_MODE_REPEAT,
            0.0,
            VK_TRUE,
            1.0f,
            VK_FALSE,
            VK_COMPARE_OP_NEVER,
            0.0,
            0.0,
            VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            VK_FALSE);

        // Update descriptor image info member that can be used for setting up descriptor sets
        texObj.descriptor.imageLayout = texObj.image_layout;
        texObj.descriptor.imageView = texObj.view;
        texObj.descriptor.sampler = texObj.sampler;

        return texObj;
    }

    TextureObject copyTexture(
        Device* device, 
        const TextureObject& image_source, 
        VkFormat format,
        uint32_t mipLevels)
    {
        TextureObject texture = texture::createTexture(device, image_source.width, image_source.height, 1, format, mipLevels);

        const size_t pixelDataSize = image_source.pitch() * image_source.height;
        Resource<VkBuffer> stagingBuffer = buffer::createBuffer(device, pixelDataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

        const VkMappedMemoryRange flushRange = {
            VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            nullptr,
            stagingBuffer.memory,
            0,
            VK_WHOLE_SIZE
        };

        void* mappedMemory;
        memory::map(device->getDevice(), stagingBuffer.memory, 0, VK_WHOLE_SIZE, &mappedMemory);
        std::memcpy(mappedMemory, image_source.pixels<void>(), pixelDataSize);
        vkFlushMappedMemoryRanges(device->getDevice(), 1, &flushRange);
        memory::unmap(device->getDevice(), stagingBuffer.memory);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if (vkBeginCommandBuffer(device->getCommandBuffers()[device->getCurrentFrame()], &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin immediate command buffer (still in recording state?)");
        }

        VkCommandBuffer commandBuffer = device->getCommandBuffers()[device->getCurrentFrame()];
        {
            const auto barrier = ImageMemoryBarrier(texture, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL).mipLevels(0, 1);
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, reinterpret_cast<const VkImageMemoryBarrier*>(&barrier));
        }

        VkBufferImageCopy copyRegion{};
        copyRegion.bufferOffset = 0;
        copyRegion.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        copyRegion.imageExtent = { texture.width, texture.height, 1 };
        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.resource, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
        {
            const VkImageLayout finalBaseMipLayout =
                (texture.mipLevels > 1)
                ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
                : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            const auto barrier = ImageMemoryBarrier(texture, VK_ACCESS_TRANSFER_WRITE_BIT, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, finalBaseMipLayout).mipLevels(0, 1);
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, reinterpret_cast<const VkImageMemoryBarrier*>(&barrier));
        }

        device->flushCommandBuffer(device->getDevice(), commandBuffer, device->getGraphicsQueue(), device->getCommandPool(), false);

        if (vkResetCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT) != VK_SUCCESS) {
            throw std::runtime_error("Failed to reset immediate command buffer");
        }
        vkDestroyBuffer(device->getDevice(), stagingBuffer.resource, nullptr);
        if (texture.mipLevels > 1)
            generateMipmaps(device, texture);

        return texture;
    }

    VkSampler createSampler(const VkDevice& device,
        VkFilter magFilter, 
        VkFilter minFilter, 
        VkSamplerMipmapMode mipmapMode,
        VkSamplerAddressMode addressModeU, 
        VkSamplerAddressMode addressModeV,
        VkSamplerAddressMode addressModeW, 
        float mipLodBias,
        VkBool32 anisotropyEnable, 
        float maxAnisotropy, 
        VkBool32 compareEnable,
        VkCompareOp compareOp, 
        float minLod, 
        float maxLod,
        VkBorderColor borderColor, 
        VkBool32 unnormalizedCoordinates) {
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

    void setImageLayout(
        VkCommandBuffer commandBuffer, 
        VkImage image, 
        VkImageAspectFlags aspectMask, 
        VkImageLayout old_image_layout,
        VkImageLayout new_image_layout, 
        VkPipelineStageFlags src_stages, 
        VkPipelineStageFlags dest_stages, 
        VkImageSubresourceRange subresource_range) {

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
        image_memory_barrier.subresourceRange = subresource_range;

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

    void generateMipmaps(
        Device* device, 
        TextureObject& texture)
    {
        assert(texture.mipLevels > 1);

        VkCommandBuffer commandBuffer = device->beginImmediateCommandBuffer();

        // Iterate through mip chain and consecutively blit from previous level to next level with linear filtering.
        for (uint32_t level = 1, prevLevelWidth = texture.width, prevLevelHeight = texture.height; level < texture.mipLevels; ++level, prevLevelWidth /= 2, prevLevelHeight /= 2) {

            const auto preBlitBarrier = ImageMemoryBarrier(texture, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL).mipLevels(level, 1);
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, reinterpret_cast<const VkImageMemoryBarrier*>(&preBlitBarrier));
            VkImageBlit region = {};
            region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, level - 1, 0, texture.layers };
            region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, level,   0, texture.layers };
            region.srcOffsets[1] = { int32_t(prevLevelWidth),  int32_t(prevLevelHeight),   1 };
            region.dstOffsets[1] = { int32_t(prevLevelWidth / 2),int32_t(prevLevelHeight / 2), 1 };
            vkCmdBlitImage(commandBuffer,
                texture.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &region, VK_FILTER_LINEAR);

            const auto postBlitBarrier = ImageMemoryBarrier(texture, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL).mipLevels(level, 1);
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, reinterpret_cast<const VkImageMemoryBarrier*>(&postBlitBarrier));
        }

        // Transition whole mip chain to shader read only layout.
        {
            const auto barrier = ImageMemoryBarrier(texture, VK_ACCESS_TRANSFER_WRITE_BIT, 0, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, reinterpret_cast<const VkImageMemoryBarrier*>(&barrier));
        }

        device->executeImmediateCommandBuffer(commandBuffer);
    }

    unsigned char* loadTextureData(char const* filename,
        int num_requested_components,
        int* image_width,
        int* image_height,
        int* image_num_components,
        int* image_data_size,
        bool* is_hdr) {
        int width = 0;
        int height = 0;
        int num_components = 0;
        
        if (stbi_is_hdr(filename)) {
            std::unique_ptr<float, void(*)(void*)> stbi_data(stbi_loadf(filename, &width, &height, &num_components, num_requested_components), stbi_image_free);
            if ((!stbi_data) ||
                (0 >= width) ||
                (0 >= height) ||
                (0 >= num_components)) {
                std::cout << "Could not read image!" << std::endl;
                assert(false);
            }

            int data_size = width * sizeof(float) * (0 < num_requested_components ? num_requested_components : num_components);
            if (image_data_size) {
                *image_data_size = data_size;
            }
            if (image_width) {
                *image_width = width;
            }
            if (image_height) {
                *image_height = height;
            }
            if (image_num_components) {
                *image_num_components = num_components;
            }
            *is_hdr = true;
            return std::move(reinterpret_cast<unsigned char*>(stbi_data.get()));
        }
        else {
            std::unique_ptr<unsigned char, void(*)(void*)> stbi_data(stbi_load(filename, &width, &height, &num_components, num_requested_components), stbi_image_free);

            if ((!stbi_data) ||
                (0 >= width) ||
                (0 >= height) ||
                (0 >= num_components)) {
                std::cout << "Could not read image!" << std::endl;
                assert(false);
            }

            int data_size = width * height * (0 < num_requested_components ? num_requested_components : num_components);
            if (image_data_size) {
                *image_data_size = data_size;
            }
            if (image_width) {
                *image_width = width;
            }
            if (image_height) {
                *image_height = height;
            }
            if (image_num_components) {
                *image_num_components = num_components;
            }
            *is_hdr = false;
            return std::move(stbi_data.get());
        }
    }
}

void TextureObject::destroy(const VkDevice& device)
{
    if (sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, sampler, nullptr);
    }
    if (view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, view, nullptr);
    }
    if (image != VK_NULL_HANDLE) {
        vkDestroyImage(device, image, nullptr);
    }
    if (image_memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, image_memory, nullptr);
    }

    data.release();
}
