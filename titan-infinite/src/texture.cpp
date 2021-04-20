/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#include "pch.h"
#include "texture.h"
#include <filesystem>
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

        // Load data, width, height and num_components
        TextureObject texObj = loadTexture(filename);
        texObj.device = device;
        texObj.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texObj.width, texObj.height)))) + 1;
        auto image_data_size = texObj.data.size();

        VkFormatProperties formatProps;
        vkGetPhysicalDeviceFormatProperties(device->getPhysicalDevice(), format, &formatProps);

        VkBuffer stage_buffer;
        VkDeviceMemory stage_buffer_memory;

        stage_buffer = buffer::createBuffer(
            device->getDevice(),
            image_data_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
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
        memcpy(data, texObj.data.data(), image_data_size);
        memory::unmap(device->getDevice(), stage_buffer_memory);

        // Image
        texObj.image = device->createImage(
            device->getDevice(),
            0,
            VK_IMAGE_TYPE_2D,
            format,
            { (uint32_t)texObj.width, (uint32_t)texObj.height, 1 },
            texObj.mipLevels,
            1,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_TILING_OPTIMAL,
            (imageUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) ? imageUsageFlags : (imageUsageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
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

        // Use a separate command buffer for texture loading
        VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, device->getCommandPool(), true);

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
            subresourceRange,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            VK_ACCESS_TRANSFER_WRITE_BIT);

        //Generate first one and setup each mipmap later
        std::vector<VkBufferImageCopy> bufferImgCopyList;
        for (uint32_t i = 0; i < 1; ++i)
        {
            VkBufferImageCopy copy_region{};
            copy_region.bufferOffset = 0;
            copy_region.bufferRowLength = 0;
            copy_region.bufferImageHeight = 0;
            copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy_region.imageSubresource.mipLevel = i;
            copy_region.imageSubresource.baseArrayLayer = 0;
            copy_region.imageSubresource.layerCount = 1;
            copy_region.imageOffset = { 0, 0, 0 };
            copy_region.imageExtent = { 
                static_cast<uint32_t>(texObj.width),
                static_cast<uint32_t>(texObj.height),
                1 
            };
            bufferImgCopyList.push_back(copy_region);
        }
        /* Put the copy command into the command buffer */
        vkCmdCopyBufferToImage(
            copyCmd,
            stage_buffer,
            texObj.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            uint32_t(bufferImgCopyList.size()),
            bufferImgCopyList.data()
        );

        /* Set the layout for the texture image from DESTINATION_OPTIMAL to SHADER_READ_ONLY */
        texObj.image_layout = imageLayout;

        device->flushCommandBuffer(copyCmd, device->getGraphicsQueue(), true);

        vkFreeMemory(device->getDevice(), stage_buffer_memory, NULL);
        vkDestroyBuffer(device->getDevice(), stage_buffer, NULL);

        generateMipmaps(device, texObj, format);

        texObj.view = device->createImageView(device->getDevice(),
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
            (float)texObj.mipLevels,
            VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
            VK_FALSE);

        return texObj;
    }

    TextureObject loadTextureCube_gli(
        const std::string& filename,
        VkFormat format,
        Device* device,
        VkImageUsageFlags imageUsageFlags,
        VkImageLayout imageLayout)
    {
        gli::texture_cube texCube(gli::load(filename));
        assert(!texCube.empty());

        TextureObject texObj;
        uint32_t width, height, mip_levels;
        size_t cube_size;

        width = static_cast<uint32_t>(texCube.extent().x);
        height = static_cast<uint32_t>(texCube.extent().y);
        cube_size = texCube.size();
        mip_levels = static_cast<uint32_t>(texCube.levels());
        auto cube_data = texCube.data();

        texObj.width = width;
        texObj.height = height;
        texObj.mipLevels = mip_levels;
        texObj.device = device;

        VkFormatProperties formatProps;
        vkGetPhysicalDeviceFormatProperties(device->getPhysicalDevice(), format, &formatProps);

        // Use a separate command buffer for texture loading
        VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, device->getCommandPool(), true);

        VkBuffer stage_buffer;
        VkDeviceMemory stage_buffer_memory;

        stage_buffer = buffer::createBuffer(
            device->getDevice(),
            cube_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
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
        texObj.image = device->createImage(
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

        std::vector<VkBufferImageCopy> bufferCopyRegions;
        size_t offset = 0;
        for (uint32_t layer = 0; layer < texCube.faces(); ++layer) {
            for (uint32_t level = 0; level < mip_levels; ++level) {
                VkBufferImageCopy copy_region{};
                copy_region.bufferRowLength = 0;
                copy_region.bufferImageHeight = 0;
                copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copy_region.imageSubresource.mipLevel = level;
                copy_region.imageSubresource.baseArrayLayer = layer;
                copy_region.imageSubresource.layerCount = 1;
                copy_region.imageOffset = { 0, 0, 0 };
                copy_region.imageExtent = {
                    static_cast<uint32_t>(texCube[layer][level].extent().x),
                    static_cast<uint32_t>(texCube[layer][level].extent().y),
                    1 };
                copy_region.bufferOffset = offset;

                bufferCopyRegions.push_back(copy_region);
                offset += texCube[layer][level].size();
            }
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
            subresourceRange,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            VK_ACCESS_TRANSFER_WRITE_BIT);

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
            subresourceRange,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_TRANSFER_READ_BIT
        );

        device->flushCommandBuffer(copyCmd, device->getGraphicsQueue());

        texObj.view = device->createImageView(device->getDevice(),
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
            16.0f,
            VK_FALSE,
            VK_COMPARE_OP_NEVER,
            0.0,
            (float)texObj.mipLevels,
            VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
            VK_FALSE);

        vkFreeMemory(device->getDevice(), stage_buffer_memory, NULL);
        vkDestroyBuffer(device->getDevice(), stage_buffer, NULL);
        
        return texObj;
    }

    TextureObject loadTextureCube(
        const std::string& filename,
        VkFormat format,
        Device* device,
        VkImageUsageFlags imageUsageFlags,
        VkImageLayout imageLayout) {

        std::filesystem::path p(filename);
        if (p.extension() == ".ktx")
            return loadTextureCube_gli(filename, format, device, imageUsageFlags, imageLayout);
        else
            throw std::runtime_error("File type is not supported");
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

        VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, device->getCommandPool(), 1);

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        stagingBuffer = buffer::createBuffer(
            device->getDevice(),
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
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

        texObj.image = device->createImage(
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
            { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
            VK_PIPELINE_STAGE_TRANSFER_BIT
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
            { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT
        );

        device->flushCommandBuffer(copyCmd, copyQueue);

        vkFreeMemory(device->getDevice(), stagingMemory, nullptr);
        vkDestroyBuffer(device->getDevice(), stagingBuffer, nullptr);

        // view
        texObj.view = device->createImageView(device->getDevice(),
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
        VkImageSubresourceRange subresource_range,
        VkPipelineStageFlags src_stages,
        VkPipelineStageFlags dest_stages,
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

        switch (old_image_layout)
        {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            // Image layout is undefined (or does not matter)
            // Only valid as initial layout
            // No flags required, listed only for completeness
            image_memory_barrier.srcAccessMask = 0;
            break;

        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            // Image is preinitialized
            // Only valid as initial layout for linear images, preserves memory contents
            // Make sure host writes have been finished
            image_memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            // Image is a color attachment
            // Make sure any writes to the color buffer have been finished
            image_memory_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            // Image is a depth/stencil attachment
            // Make sure any writes to the depth/stencil buffer have been finished
            image_memory_barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            // Image is a transfer source
            // Make sure any reads from the image have been finished
            image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            // Image is a transfer destination
            // Make sure any writes to the image have been finished
            image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            // Image is read by a shader
            // Make sure any shader reads from the image have been finished
            image_memory_barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;
        default:
            // Other source layouts aren't handled (yet)
            break;
        }

        // Target layouts (new)
        // Destination access mask controls the dependency for the new image layout
        switch (new_image_layout)
        {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            // Image will be used as a transfer destination
            // Make sure any writes to the image have been finished
            image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            // Image will be used as a transfer source
            // Make sure any reads from the image have been finished
            image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            // Image will be used as a color attachment
            // Make sure any writes to the color buffer have been finished
            image_memory_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            // Image layout will be used as a depth/stencil attachment
            // Make sure any writes to depth/stencil buffer have been finished
            image_memory_barrier.dstAccessMask = image_memory_barrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            // Image will be read in a shader (sampler, input attachment)
            // Make sure any writes to the image have been finished
            if (image_memory_barrier.srcAccessMask == 0)
            {
                image_memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
            }
            image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;
        default:
            // Other source layouts aren't handled (yet)
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

        VkCommandBuffer commandBuffer = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, device->getCommandPool(), true);

        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = texture.image;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.subresourceRange.levelCount = 1;

        // Iterate through mip chain and consecutively blit from previous level to next level with linear filtering.
        for (uint32_t level = 1; level < texture.mipLevels; ++level) {
            barrier.subresourceRange.baseMipLevel = level - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            
            vkCmdPipelineBarrier(commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                0, nullptr,
                0, nullptr,
                1, &barrier);

            VkImageBlit region{};
            region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, level - 1, 0, 1 };
            region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, level,   0, 1 };
            region.srcOffsets[1] = { texture.width > 1 ? int32_t(texture.width >> (level - 1)) : 1,  texture.height > 1 ? int32_t(texture.height >> (level - 1)) : 1, 1 };
            region.dstOffsets[1] = { texture.width > 1 ? int32_t(texture.width >> (level)) : 1,  texture.height > 1 ? int32_t(texture.height >> (level)) : 1, 1 };
            vkCmdBlitImage(commandBuffer,
                texture.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &region, VK_FILTER_LINEAR);

            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                0, nullptr,
                0, nullptr,
                1, &barrier);
        }

        // Transition whole mip chain to shader read only layout.
        {
            barrier.subresourceRange.baseMipLevel = texture.mipLevels - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                0, nullptr,
                0, nullptr,
                1, &barrier);
        }
        device->flushCommandBuffer(commandBuffer, device->getGraphicsQueue(), true);
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

    TextureObject loadTexture(const std::string& filename)
    {
        TextureObject texObj{};
        auto data = vkHelper::readFile(filename);
        auto extension = vkHelper::getFileExtension(filename);

        if (extension == "png" || extension == "jpg")
        {
            int width;
            int height;
            int comp;
            int req_comp = 4;

            auto data_buffer = reinterpret_cast<const stbi_uc*>(data.data());
            auto data_size = static_cast<int>(data.size());
            auto raw_data = stbi_load_from_memory(data_buffer, data_size, &width, &height, &comp, req_comp);

            if (!raw_data) {
                throw std::runtime_error{ "Failed to load " + filename + ": " + stbi_failure_reason() };
            }

            assert(texObj.data.empty() && "Image data already set");

            texObj.data = { raw_data, raw_data + width * height * req_comp };
            stbi_image_free(raw_data);

            texObj.width = width;
            texObj.height = height;
            texObj.num_components = comp;
            texObj.format = VK_FORMAT_R8G8B8A8_UNORM;
        }
        else if (extension == "ktx")
        {
            gli::texture2d tex(gli::load(filename));
            assert(!tex.empty());
            texObj.data.resize(tex.size());
            memcpy(&texObj.data[0], tex.data(), tex.size());
            texObj.width = static_cast<uint32_t>(tex.extent().x);
            texObj.height = static_cast<uint32_t>(tex.extent().y);
            texObj.format = VK_FORMAT_R8G8B8A8_UNORM;
            texObj.mipLevels = tex.levels();
        }
        return texObj;
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
}