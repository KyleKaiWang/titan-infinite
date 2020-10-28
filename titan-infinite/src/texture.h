/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#pragma once

class TextureObject
{
public:
    VkSampler sampler;
    VkImageView view;
    VkImage image;
    VkDeviceMemory image_memory;
    VkImageLayout image_layout;
    VkDeviceSize buffer_size;
    VkDescriptorImageInfo descriptor;
    
    bool is_hdr = false;
    bool needs_staging;

    uint32_t width, height;
    uint32_t num_components;
    uint32_t layers = 1;
    uint32_t mipLevels;
    
    int bytesPerPixel() const { return num_components * (is_hdr ? sizeof(float) : sizeof(unsigned char)); }
    int pitch() const { return width * bytesPerPixel(); }
    void updateDescriptor();
    void destroy(const VkDevice& device);
};

struct ImageMemoryBarrier
{
    ImageMemoryBarrier(TextureObject& texture, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        barrier.srcAccessMask = srcAccessMask;
        barrier.dstAccessMask = dstAccessMask;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = texture.image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
        texture.image_layout = newLayout;
        texture.updateDescriptor();
    }
    operator VkImageMemoryBarrier() const { return barrier; }
    VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };

    ImageMemoryBarrier& aspectMask(VkImageAspectFlags aspectMask)
    {
        barrier.subresourceRange.aspectMask = aspectMask;
        return *this;
    }
    ImageMemoryBarrier& mipLevels(uint32_t baseMipLevel, uint32_t levelCount = VK_REMAINING_MIP_LEVELS)
    {
        barrier.subresourceRange.baseMipLevel = baseMipLevel;
        barrier.subresourceRange.levelCount = levelCount;
        return *this;
    }
    ImageMemoryBarrier& arrayLayers(uint32_t baseArrayLayer, uint32_t layerCount = VK_REMAINING_ARRAY_LAYERS)
    {
        barrier.subresourceRange.baseArrayLayer = baseArrayLayer;
        barrier.subresourceRange.layerCount = layerCount;
        return *this;
    }
};

namespace texture {
    
    // Empty texture
    TextureObject createTexture(
        Device* device, 
        uint32_t width, 
        uint32_t height, 
        uint32_t layers, 
        VkFormat format,
        uint32_t mipLevels = 0,
        VkImageUsageFlags imageUsageFlags = 0,
        VkSampler sampler = VK_NULL_HANDLE);

    // From file
    TextureObject loadTexture(
        const std::string& filename,
        VkFormat format, 
        Device* device,
        int num_requested_components,
        VkFilter filter = VK_FILTER_LINEAR,
        VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
        VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // From file - Cube
    TextureObject loadTextureCube(
        const std::string& filename,
        VkFormat format,
        Device* device,
        VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
        VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // From buffer
    TextureObject loadTexture(
        void* buffer,
        VkDeviceSize bufferSize, 
        VkFormat format, 
        uint32_t texWidth,
        uint32_t texHeight,
        Device* device,
        VkQueue copyQueue, 
        VkFilter filter = VK_FILTER_LINEAR,
        VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT, 
        VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    
    bool loadTextureData(
        char const* filename, 
        int num_requested_components,
        std::vector<unsigned char>& image_data,
        int* image_width = nullptr, 
        int* image_height = nullptr, 
        int* image_num_components = nullptr,
        int* image_data_size = nullptr,
        bool* is_hdr = nullptr);
    
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
        VkBool32 unnormalizedCoordinates);

    void setImageLayout(
        VkCommandBuffer commandBuffer, 
        VkImage image,
        VkImageLayout old_image_layout, 
        VkImageLayout new_image_layout,
        VkPipelineStageFlags src_stages,
        VkPipelineStageFlags dest_stages, 
        VkImageSubresourceRange subresource_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        VkAccessFlags src_access_mask = 0,
        VkAccessFlags dst_access_mask = 0);

    void generateMipmaps(
        Device* device, 
        TextureObject& texture,
        VkFormat format);

    template<typename T> static constexpr T numMipmapLevels(T width, T height)
    {
        T levels = 1;
        while ((width | height) >> levels) {
            ++levels;
        }
        return levels;
    }
}