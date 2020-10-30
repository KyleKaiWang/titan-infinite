/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#include "pch.h"
#include "texture.h"
#include <gli.hpp>

namespace texture {

    TextureObject loadTexture(
        const std::string& filename,
        VkFormat format,
        Device* device,
        int num_requested_components,
        VkFilter filter,
        VkImageUsageFlags imageUsageFlags,
        VkImageLayout imageLayout) {
        TextureObject texObj;
        int width, height, num_components, image_data_size;
        bool is_hdr;
        std::vector<unsigned char> pixels;
        if (!texture::loadTextureData(filename.c_str(), num_requested_components, pixels, &width, &height, &num_components, &image_data_size, &is_hdr)) {
            assert(false);
        }

        texObj.width = width;
        texObj.height = height;
        texObj.num_components = num_components;
        texObj.is_hdr = is_hdr;
        texObj.mipLevels = texture::numMipmapLevels(width, height);


        VkFormatProperties formatProps;
        vkGetPhysicalDeviceFormatProperties(device->getPhysicalDevice(), format, &formatProps);

        VkBuffer stage_buffer;
        VkDeviceMemory stage_buffer_memory;

        stage_buffer = buffer::createBuffer(
            device->getDevice(),
            image_data_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_SHARING_MODE_EXCLUSIVE
        );

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device->getDevice(), stage_buffer, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = device->findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(device->getDevice(), &allocInfo, nullptr, &stage_buffer_memory) != VK_SUCCESS) {
            throw std::runtime_error("No mappable, coherent memory!");
        }

        if (vkBindBufferMemory(device->getDevice(), stage_buffer, stage_buffer_memory, 0) != VK_SUCCESS) {
            throw std::runtime_error("Failed to bind buffer memory");
        }

        // copy texture data to staging buffer
        void* data;
        memory::map(device->getDevice(), stage_buffer_memory, 0, memReqs.size, &data);
        memcpy(data, pixels.data(), image_data_size);
        memory::unmap(device->getDevice(), stage_buffer_memory);

        // Image
        texObj.image = renderer::createImage(
            device->getDevice(),
            0,
            VK_IMAGE_TYPE_2D,
            format,
            { (uint32_t)texObj.width, (uint32_t)texObj.height, 1 },
            texObj.mipLevels,
            1,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_TILING_OPTIMAL,
            (imageUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) ? imageUsageFlags : (imageUsageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT),
            VK_SHARING_MODE_EXCLUSIVE,
            VK_IMAGE_LAYOUT_UNDEFINED
        );

        vkGetImageMemoryRequirements(device->getDevice(), texObj.image, &memReqs);
        texObj.buffer_size = memReqs.size;

        // VkMemoryAllocateInfo
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = device->findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(device->getDevice(), &allocInfo, nullptr, &texObj.image_memory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate buffer memory!");
        }

        if (vkBindImageMemory(device->getDevice(), texObj.image, texObj.image_memory, 0) != VK_SUCCESS) {
            throw std::runtime_error("Failed to bind image memory");
        }

        // VkCommandBuffer copyCmd = device->beginImmediateCommandBuffer();
        // Use a separate command buffer for texture loading
        VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        VkImageSubresourceRange subresourceRange{};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = texObj.mipLevels;
        subresourceRange.layerCount = 1;

        /* Since we're going to blit to the texture image, set its layout to DESTINATION_OPTIMAL */
        texture::setImageLayout(
            copyCmd,
            texObj.image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            subresourceRange,
            0,
            VK_ACCESS_TRANSFER_WRITE_BIT);

        VkBufferImageCopy copy_region{};
        copy_region.bufferOffset = 0;
        copy_region.bufferRowLength = 0;
        copy_region.bufferImageHeight = 0;
        copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_region.imageSubresource.mipLevel = 0;
        copy_region.imageSubresource.baseArrayLayer = 0;
        copy_region.imageSubresource.layerCount = 1;
        copy_region.imageOffset = { 0, 0, 0 };
        copy_region.imageExtent = { texObj.width, texObj.height, 1 };

        /* Put the copy command into the command buffer */
        vkCmdCopyBufferToImage(
            copyCmd,
            stage_buffer,
            texObj.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &copy_region
        );

        /* Set the layout for the texture image from DESTINATION_OPTIMAL to SHADER_READ_ONLY */
        texObj.image_layout = imageLayout;
        setImageLayout(
            copyCmd,
            texObj.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            texObj.image_layout,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            subresourceRange,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_TRANSFER_READ_BIT
        );

        device->flushCommandBuffer(copyCmd, device->getGraphicsQueue(), true);
        //device->executeImmediateCommandBuffer(copyCmd);

        vkFreeMemory(device->getDevice(), stage_buffer_memory, NULL);
        vkDestroyBuffer(device->getDevice(), stage_buffer, NULL);

        texObj.view = renderer::createImageView(device->getDevice(),
            texObj.image,
            VK_IMAGE_VIEW_TYPE_2D,
            format,
            { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
            { VK_IMAGE_ASPECT_COLOR_BIT, 0, texObj.mipLevels, 0, 1 });

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
            (float)texObj.mipLevels,
            VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
            VK_FALSE);

        texObj.descriptor.imageLayout = texObj.image_layout;
        texObj.descriptor.imageView = texObj.view;
        texObj.descriptor.sampler = texObj.sampler;

        return texObj;
    }

    TextureObject loadTextureCube(
        const std::string& filename,
        VkFormat format,
        Device* device,
        VkImageUsageFlags imageUsageFlags,
        VkImageLayout imageLayout) {
        TextureObject texObj;
        uint32_t width, height, mip_levels;
        size_t cube_size;

        gli::texture_cube texCube(gli::load(filename));
        assert(!texCube.empty());

        width = static_cast<uint32_t>(texCube.extent().x);
        height = static_cast<uint32_t>(texCube.extent().y);
        cube_size = texCube.size();
        mip_levels = static_cast<uint32_t>(texCube.levels());
        auto cube_data = texCube.data();

        texObj.width = width;
        texObj.height = height;
        texObj.mipLevels = mip_levels;

        VkFormatProperties formatProps;
        vkGetPhysicalDeviceFormatProperties(device->getPhysicalDevice(), format, &formatProps);

        // Use a separate command buffer for texture loading
        VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        VkBuffer stage_buffer;
        VkDeviceMemory stage_buffer_memory;

        stage_buffer = buffer::createBuffer(
            device->getDevice(),
            cube_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_SHARING_MODE_EXCLUSIVE
        );

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device->getDevice(), stage_buffer, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = device->findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(device->getDevice(), &allocInfo, nullptr, &stage_buffer_memory) != VK_SUCCESS) {
            throw std::runtime_error("No mappable, coherent memory!");
        }

        if (vkBindBufferMemory(device->getDevice(), stage_buffer, stage_buffer_memory, 0) != VK_SUCCESS) {
            throw std::runtime_error("Failed to bind buffer memory");
        }

        // copy texture data to staging buffer
        void* data;
        memory::map(device->getDevice(), stage_buffer_memory, 0, memReqs.size, &data);
        memcpy(data, cube_data, cube_size);
        memory::unmap(device->getDevice(), stage_buffer_memory);

        // Image
        texObj.image = renderer::createImage(
            device->getDevice(),
            VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
            VK_IMAGE_TYPE_2D,
            format,
            { width, height, 1 },
            texObj.mipLevels,
            6,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_TILING_OPTIMAL,
            (imageUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) ? imageUsageFlags : (imageUsageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT),
            VK_SHARING_MODE_EXCLUSIVE,
            VK_IMAGE_LAYOUT_UNDEFINED
        );

        vkGetImageMemoryRequirements(device->getDevice(), texObj.image, &memReqs);
        texObj.buffer_size = memReqs.size;

        // VkMemoryAllocateInfo
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = device->findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(device->getDevice(), &allocInfo, nullptr, &texObj.image_memory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate buffer memory!");
        }

        if (vkBindImageMemory(device->getDevice(), texObj.image, texObj.image_memory, 0) != VK_SUCCESS) {
            throw std::runtime_error("Failed to bind image memory");
        }

        VkImageSubresourceRange subresourceRange{};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = texObj.mipLevels;
        subresourceRange.layerCount = 6;

        /* Since we're going to blit to the texture image, set its layout to DESTINATION_OPTIMAL */
        texture::setImageLayout(
            copyCmd,
            texObj.image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            subresourceRange,
            0,
            VK_ACCESS_TRANSFER_WRITE_BIT);


        std::vector<VkBufferImageCopy> bufferCopyRegions;
        size_t offset = 0;
        for (uint32_t face = 0; face < texCube.faces(); ++face) {
            for (uint32_t level = 0; level < mip_levels; ++level) {
                VkBufferImageCopy copy_region{};
                copy_region.bufferRowLength = 0;
                copy_region.bufferImageHeight = 0;
                copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copy_region.imageSubresource.mipLevel = 0;
                copy_region.imageSubresource.baseArrayLayer = 0;
                copy_region.imageSubresource.layerCount = 1;
                copy_region.imageOffset = { 0, 0, 0 };
                copy_region.imageExtent = { 
                    static_cast<uint32_t>(texCube[face][level].extent().x), 
                    static_cast<uint32_t>(texCube[face][level].extent().y), 
                    1 };
                copy_region.bufferOffset = offset;

                bufferCopyRegions.push_back(copy_region);
                offset += texCube[face][level].size();
            }
        }

        /* Put the copy command into the command buffer */
        vkCmdCopyBufferToImage(
            copyCmd,
            stage_buffer,
            texObj.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(bufferCopyRegions.size()),
            bufferCopyRegions.data()
        );

        /* Set the layout for the texture image from DESTINATION_OPTIMAL to SHADER_READ_ONLY */
        texObj.image_layout = imageLayout;
        setImageLayout(
            copyCmd,
            texObj.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            texObj.image_layout,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            subresourceRange,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_TRANSFER_READ_BIT
        );

        device->flushCommandBuffer(copyCmd, device->getGraphicsQueue());
        vkFreeMemory(device->getDevice(), stage_buffer_memory, NULL);
        vkDestroyBuffer(device->getDevice(), stage_buffer, NULL);

        texObj.view = renderer::createImageView(device->getDevice(),
            texObj.image,
            VK_IMAGE_VIEW_TYPE_CUBE,
            format,
            { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
            { VK_IMAGE_ASPECT_COLOR_BIT, 0, texObj.mipLevels, 0, 6 });

        texObj.sampler = texture::createSampler(device->getDevice(),
            VK_FILTER_LINEAR,
            VK_FILTER_LINEAR,
            VK_SAMPLER_MIPMAP_MODE_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            0.0,
            VK_TRUE,
            1.0f,
            VK_FALSE,
            VK_COMPARE_OP_NEVER,
            0.0,
            (float)texObj.mipLevels,
            VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
            VK_FALSE);

        texObj.descriptor.imageLayout = texObj.image_layout;
        texObj.descriptor.imageView = texObj.view;
        texObj.descriptor.sampler = texObj.sampler;

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

        VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);

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
        memory::map(device->getDevice(), stagingMemory, 0, memReqs.size, &data);
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

        if (vkAllocateMemory(device->getDevice(), &memAllocInfo, nullptr, &texObj.image_memory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate buffer memory!");
        }

        if (vkBindImageMemory(device->getDevice(), texObj.image, texObj.image_memory, 0) != VK_SUCCESS) {
            throw std::runtime_error("Failed to bind image memory");
        }

        // Image barrier for optimal image (target)
        // Optimal image will be used as destination for the copy
        // Create an image barrier object
        texture::setImageLayout(copyCmd,
            texObj.image,
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
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            texObj.image_layout,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        );

        device->flushCommandBuffer(copyCmd, copyQueue);

        vkFreeMemory(device->getDevice(), stagingMemory, nullptr);
        vkDestroyBuffer(device->getDevice(), stagingBuffer, nullptr);

        // view
        texObj.view = renderer::createImageView(device->getDevice(),
            texObj.image,
            VK_IMAGE_VIEW_TYPE_2D,
            format,
            { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
            { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

        // sampler
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
        VkImageLayout old_image_layout,
        VkImageLayout new_image_layout,
        VkPipelineStageFlags src_stages,
        VkPipelineStageFlags dest_stages,
        VkImageSubresourceRange subresource_range,
        VkAccessFlags src_access_mask,
        VkAccessFlags dst_access_mask) {

        assert(commandBuffer != VK_NULL_HANDLE);

        VkImageMemoryBarrier image_memory_barrier = {};
        image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        image_memory_barrier.pNext = NULL;
        image_memory_barrier.srcAccessMask = src_access_mask;
        image_memory_barrier.dstAccessMask = dst_access_mask;
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
        TextureObject& texture,
        VkFormat format)
    {
        assert(texture.mipLevels > 1);

        // Check if image format supports linear blitting
        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(device->getPhysicalDevice(), format, &formatProperties);

        if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
            throw std::runtime_error("texture image format does not support linear blitting!");
        }

        VkCommandBuffer blitCommand = device->beginImmediateCommandBuffer();

        // Iterate through mip chain and consecutively blit from previous level to next level with linear filtering.
        for (uint32_t level = 1; level < texture.mipLevels; ++level) {
            texture::setImageLayout(
                blitCommand,
                texture.image, 
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_TRANSFER_BIT, 
                VK_PIPELINE_STAGE_TRANSFER_BIT, 
                { VK_IMAGE_ASPECT_COLOR_BIT, level, 1, 0, 1 },
                0,
                VK_ACCESS_TRANSFER_WRITE_BIT
            );

            VkImageBlit region{};
            region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, level - 1, 0, texture.layers };
            region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, level,   0, texture.layers };
            region.srcOffsets[1] = { int32_t(texture.width >> (level - 1)),  int32_t(texture.height >> (level - 1)), 1 };
            region.dstOffsets[1] = { int32_t(texture.width >> (level)),  int32_t(texture.height >> (level)), 1 };
            vkCmdBlitImage(blitCommand,
                texture.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &region, VK_FILTER_LINEAR);

            texture::setImageLayout(
                blitCommand,
                texture.image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                { VK_IMAGE_ASPECT_COLOR_BIT, level, 1, 0, 1 },
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_TRANSFER_READ_BIT
            );
        }

        // Transition whole mip chain to shader read only layout.
        {
            texture::setImageLayout(
                blitCommand,
                texture.image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                { VK_IMAGE_ASPECT_COLOR_BIT, 0, texture.mipLevels, 0, 1 },
                VK_ACCESS_TRANSFER_READ_BIT,
                VK_ACCESS_SHADER_READ_BIT
            );
        }
        device->flushCommandBuffer(blitCommand, device->getGraphicsQueue(), true);
    }

    bool loadTextureData(char const* filename,
        int num_requested_components,
        std::vector<unsigned char>& image_data,
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
                return false;
            }

            int data_size = width * height * sizeof(float) * (0 < num_requested_components ? num_requested_components : num_components);
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
            image_data.resize(data_size);
            std::memcpy(image_data.data(), stbi_data.get(), data_size);
            return true;
        }
        else {
            std::unique_ptr<unsigned char, void(*)(void*)> stbi_data(stbi_load(filename, &width, &height, &num_components, num_requested_components), stbi_image_free);

            if ((!stbi_data) ||
                (0 >= width) ||
                (0 >= height) ||
                (0 >= num_components)) {
                std::cout << "Could not read image!" << std::endl;
                return false;
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
            image_data.resize(data_size);
            std::memcpy(image_data.data(), stbi_data.get(), data_size);
            return true;
        }
    }
}

void TextureObject::updateDescriptor()
{
    descriptor.sampler = sampler;
    descriptor.imageView = view;
    descriptor.imageLayout = image_layout;
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
}