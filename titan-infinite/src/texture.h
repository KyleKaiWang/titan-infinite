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
    int32_t num_components;
    uint32_t layers;
    uint32_t mipLevels;
};

namespace texture {
    VkSampler createSampler(const VkDevice& device,
        VkFilter magFilter, VkFilter minFilter, VkSamplerMipmapMode mipmapMode,
        VkSamplerAddressMode addressModeU, VkSamplerAddressMode addressModeV,
        VkSamplerAddressMode addressModeW, float mipLodBias,
        VkBool32 anisotropyEnable, float maxAnisotropy, VkBool32 compareEnable,
        VkCompareOp compareOp, float minLod, float maxLod,
        VkBorderColor borderColor, VkBool32 unnormalizedCoordinates);

    void setImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageAspectFlags aspectMask, VkImageLayout old_image_layout, VkImageLayout new_image_layout, VkPipelineStageFlags src_stages, VkPipelineStageFlags dest_stages, VkImageSubresourceRange subresource_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
    TextureObject loadTexture(const std::string& filename, VkFormat format, Device* device, int num_requested_components);
    TextureObject loadTexture(void* buffer, VkDeviceSize bufferSize, VkFormat format, uint32_t texWidth, uint32_t texHeight, Device* device, VkQueue copyQueue, VkFilter filter = VK_FILTER_LINEAR, VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT, VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    bool loadTextureData(char const* filename, std::vector<unsigned char>& image_data, int num_requested_components, int* image_width = nullptr, int* image_height = nullptr, int* image_num_components = nullptr, int* image_data_size = nullptr);
}