/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#pragma once
#include <vulkan/vulkan.hpp>

namespace texture {

    VkSampler createTextureSampler(const VkDevice& device,
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
}