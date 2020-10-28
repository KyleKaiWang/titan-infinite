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
    uint32_t topX;
    uint32_t topY;
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

namespace renderer {

    VkImage createImage(const VkDevice& device,
        VkImageCreateFlags flags,
        VkImageType imageType,
        VkFormat format,
        const VkExtent3D& extent,
        uint32_t mipLevels,
        uint32_t arrayLayers,
        VkSampleCountFlags samples,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED);

    VkImageView createImageView(const VkDevice& device,
        VkImage image,
        VkImageViewType viewType,
        VkFormat format,
        const VkComponentMapping& components,
        const VkImageSubresourceRange& subresourceRange);

    VkRenderPass createRenderPass(const VkDevice& device,
        const std::vector<VkAttachmentDescription>& attachmentDescriptions,
        const std::vector<SubpassDescription>& subpassDescriptions,
        const std::vector<VkSubpassDependency>& subpassDependency);

    VkFramebuffer createFramebuffer(const VkDevice& device,
        const VkRenderPass& renderPass,
        const std::vector<VkImageView>& imageViews,
        VkExtent2D extent,
        uint32_t layers);

    VkShaderModule createShaderModule(const VkDevice& device, const std::vector<char>& code);
    std::vector<ShaderStage> createShader(const VkDevice& device, const std::string& vertexShaderFile, const std::string& pixelShaderFile);
    ShaderStage createShader(const VkDevice& device, const std::string& computeShaderFile);
    VkDescriptorSetLayout createDescriptorSetLayout(const VkDevice& device, const std::vector<DescriptorSetLayoutBinding>& descriptorSetLayoutBindings);
    VkPipelineLayout createPipelineLayout(const VkDevice& device, const std::vector<VkDescriptorSetLayout>& descriptorSetLayout, const std::vector<VkPushConstantRange>& pushConstantRanges);
    
    VkPipeline createGraphicsPipeline(
        const VkDevice& device,
        VkPipelineCache pipelineCache,
        const std::vector<ShaderStage>& shaderStages,
        const VertexInputState& vertexInputState,
        const InputAssemblyState& inputAssemblyState,
        const ViewportState& viewportState,
        const RasterizationState& rasterizationState,
        const MultisampleState& multisampleState,
        const DepthStencilState& depthStencilState,
        const ColorBlendState& colorBlendState,
        const VkPipelineDynamicStateCreateInfo dynamicState,
        const VkPipelineLayout& pipelineLayout,
        const VkRenderPass& renderPass);

    VkPipeline createComputePipeline(const VkDevice& device, const std::string& computeShaderFile, VkPipelineLayout layout, const VkSpecializationInfo* specializationInfo = nullptr);
    std::vector<VkDescriptorSet> createDescriptorSets(const VkDevice& device, const VkDescriptorPool& descriptorPool, const std::vector<VkDescriptorSetLayout>& descriptorSetLayout);
    VkDescriptorSet createDescriptorSet(const VkDevice& device, const VkDescriptorPool& descriptorPool, const VkDescriptorSetLayout& descriptorSetLayout);
};