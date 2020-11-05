/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#pragma once
#include <vulkan/vulkan.hpp>

struct SubpassDescription {
    std::vector<VkAttachmentReference> inputAttachments;
    std::vector<VkAttachmentReference> colorAttachments;
    std::vector<VkAttachmentReference> resolveAttachments;
    VkAttachmentReference depthStencilAttachment;
    std::vector<uint32_t> preserveAttachments;
};

struct DescriptorSetLayoutBinding {
    uint32_t binding;
    VkDescriptorType descriptorType;
    uint32_t descriptorCount;
    VkShaderStageFlags stageFlags;
    const VkSampler* pImmutableSamplers;
};

struct ShaderStage {
    VkShaderStageFlagBits stage;
    VkShaderModule module;
    std::string pName;
};

struct VertexInputState {
    std::vector<VkVertexInputBindingDescription> vertexBindingDescriptions;
    std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
};

struct InputAssemblyState {
    VkPrimitiveTopology topology;
    VkBool32 primitiveRestartEnable;
};

struct ViewportState {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
};

struct RasterizationState {
    VkBool32 depthClampEnable;
    VkBool32 rasterizerDiscardEnable;
    VkPolygonMode polygonMode;
    VkCullModeFlags cullMode;
    VkFrontFace frontFace;
    VkBool32 depthBiasEnable;
    float depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor,
        lineWidth;
};

struct MultisampleState {
    VkSampleCountFlagBits rasterizationSamples;
    VkBool32 sampleShadingEnable;
    float minSampleShading;
    std::vector<VkSampleMask> sampleMask;
    VkBool32 alphaToCoverageEnable, alphaToOneEnable;
};

struct DepthStencilState {
    VkBool32 depthTestEnable, depthWriteEnable;
    VkCompareOp depthCompareOp;
    VkBool32 depthBoundsTestEnable, stencilTestEnable;
    VkStencilOpState front, back;
    float minDepthBounds, maxDepthBounds;
};

struct ColorBlendState {
    VkBool32 logicOpEnable;
    VkLogicOp logicOp;
    std::vector<VkPipelineColorBlendAttachmentState> attachments;
    float blendConstants[4];
};