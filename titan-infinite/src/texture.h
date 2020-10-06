/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#pragma once
#include <vulkan/vulkan.hpp>

struct TextureObject
{
    VkSampler sampler;
    VkImage image;
    VkDeviceMemory imageMemory;
    VkImageLayout imageLayout;
    VkImageView view;
    VkDescriptorImageInfo descriptor;
    bool needs_staging;
    VkBuffer buffer;
    VkDeviceSize buffer_size;

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
        VkBorderColor borderColor, VkBool32 unnormalizedCoordinates);

    void setImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageAspectFlags aspectMask, VkImageLayout old_image_layout, VkImageLayout new_image_layout, VkPipelineStageFlags src_stages, VkPipelineStageFlags dest_stages);
    TextureObject loadTexture(const std::string& filename, Device* device, int channels);
    TextureObject loadTexture(void* buffer, VkDeviceSize bufferSize, VkFormat format, uint32_t texWidth, uint32_t texHeight, Device* device, VkQueue copyQueue, VkFilter filter, VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout);
}