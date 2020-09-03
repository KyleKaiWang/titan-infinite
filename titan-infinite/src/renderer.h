/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#pragma once
#include "device.h"
#include "swapChain.h"
#include "memory.h"
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

struct Renderer {

private:
    VkDevice m_device;

public:
    VkRenderPass m_renderPass;
    std::vector<VkFramebuffer> m_framebuffers;
    VkDescriptorSetLayout m_descriptorSetLayout;
    VkPipelineLayout m_pipelineLayout;
    VkPipeline m_graphicsPipeline;

    const VkRenderPass& getRenderPass() { return m_renderPass; }
    const std::vector<VkFramebuffer>& getFramebuffers() { return m_framebuffers; }
    const VkDescriptorSetLayout& getDescritporSetLayout() { return m_descriptorSetLayout; }
    const VkPipelineLayout& getPipelineLayout() { return m_pipelineLayout; }
    const VkPipeline& getGraphicsPipeline() { return m_graphicsPipeline; }

    void create(VkDevice device) {
        m_device = device;
    }

    void destroy() {
        for (auto framebuffer : m_framebuffers) {
            vkDestroyFramebuffer(m_device, framebuffer, nullptr);
        }
        vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
    }

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
                     VkSharingMode sharingMode,
                     VkImageLayout initialLayout) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.flags = flags;
        imageInfo.imageType = imageType;
        imageInfo.extent.width = extent.width;
        imageInfo.extent.height = extent.height;
        imageInfo.extent.depth = extent.depth;
        imageInfo.mipLevels = mipLevels;
        imageInfo.arrayLayers = arrayLayers;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = initialLayout;
        imageInfo.usage = usage;
        imageInfo.samples = (VkSampleCountFlagBits)samples;
        imageInfo.sharingMode = sharingMode;

        VkImage image;
        if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image!");
        }
        return image;
    }

    VkImageView createImageView(const VkDevice& device, 
                                VkImage image, 
                                VkImageViewType viewType, 
                                VkFormat format,
                                const VkComponentMapping& components, 
                                const VkImageSubresourceRange& subresourceRange) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = viewType;
        viewInfo.format = format;
        viewInfo.components = components;
        viewInfo.subresourceRange = subresourceRange;

        VkImageView imageView;
        if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture image view!");
        }
        return imageView;
    }

    void createRenderPass(const VkDevice& device, 
                          const std::vector<VkAttachmentDescription>& attachmentDescriptions, 
                          const std::vector<SubpassDescription>& subpassDescriptions,
                          const std::vector<VkSubpassDependency>& subpassDependency) {
        
        std::vector<VkSubpassDescription> descriptions;
        std::transform(subpassDescriptions.begin(), subpassDescriptions.end(),
            std::back_inserter(descriptions),
            [](const SubpassDescription& description) {

            VkSubpassDescription subpass{};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.inputAttachmentCount = static_cast<uint32_t>(description.inputAttachments.size());
            subpass.pInputAttachments = description.inputAttachments.data();
            subpass.pResolveAttachments = description.resolveAttachments.data();
            subpass.preserveAttachmentCount = static_cast<uint32_t>(description.preserveAttachments.size());
            subpass.pPreserveAttachments = description.preserveAttachments.data();
            subpass.colorAttachmentCount = description.colorAttachments.size();
            subpass.pColorAttachments = description.colorAttachments.data();
            subpass.pDepthStencilAttachment = &description.depthStencilAttachment;
            return subpass;
        });

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
        renderPassInfo.pAttachments = attachmentDescriptions.empty() ? nullptr : &attachmentDescriptions.front();
        renderPassInfo.subpassCount = static_cast<uint32_t>(subpassDescriptions.size());
        renderPassInfo.pSubpasses = descriptions.empty() ? nullptr : &descriptions.front();
        renderPassInfo.dependencyCount = static_cast<uint32_t>(subpassDependency.size());
        renderPassInfo.pDependencies = subpassDependency.empty() ? nullptr : &subpassDependency.front();;

        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass!");
        }
    }

    VkFramebuffer createFramebuffer(const VkDevice& device,
        const VkRenderPass& renderPass,
        const std::vector<VkImageView>& imageViews,
        VkExtent2D extent,
        uint32_t layers) {
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(imageViews.size());
        framebufferInfo.pAttachments = imageViews.data();
        framebufferInfo.width = extent.width;
        framebufferInfo.height = extent.height;
        framebufferInfo.layers = 1;

        VkFramebuffer framebuffer;
        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
        return std::move(framebuffer);
    }

    VkBuffer createBuffer(const VkDevice& device,
                      VkDeviceSize size, 
                      VkBufferUsageFlags usage, 
                      VkSharingMode sharingMode) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = sharingMode;

        VkBuffer buffer;
        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer!");
        }
        return std::move(buffer);
    }

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

    std::vector<ShaderStage> createShader(Device device, const std::string& vertexShaderFile, const std::string& pixelShaderFile) {
        ShaderStage vertShaderStage{};
        vertShaderStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStage.module = createShaderModule(device.getDevice(), readFile(vertexShaderFile));
        vertShaderStage.pName = "main";
        
        ShaderStage fragShaderStage{};
        fragShaderStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStage.module = createShaderModule(device.getDevice(), readFile(pixelShaderFile));
        fragShaderStage.pName = "main";

        std::vector<ShaderStage> shaderStages{ vertShaderStage, fragShaderStage };
        return shaderStages;
    }

    VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }
        return shaderModule;
    }

    void createDescriptorSetLayout(const Device& device, const std::vector<DescriptorSetLayoutBinding>& descriptorSetLayoutBindings) {
        std::vector<VkDescriptorSetLayoutBinding> contertedBindings;
        for (const DescriptorSetLayoutBinding& binding: descriptorSetLayoutBindings) {
            VkDescriptorSetLayoutBinding convertedBinding;
            convertedBinding.binding = binding.binding;
            convertedBinding.descriptorType = binding.descriptorType;
            convertedBinding.descriptorCount = binding.descriptorCount;
            convertedBinding.stageFlags = binding.stageFlags;
            convertedBinding.pImmutableSamplers = nullptr;

            contertedBindings.emplace_back(convertedBinding);
        }
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindings.size());
        layoutInfo.pBindings = contertedBindings.data();

        if (vkCreateDescriptorSetLayout(device.getDevice(), &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }

    void createPipelineLayout(const Device& device, VkDescriptorSetLayout descriptorSetLayout) {
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

        if (vkCreatePipelineLayout(device.getDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }
    }

    void createGraphicsPipeline(
        const Device& device,
        const std::vector<ShaderStage>& shaderStages, 
        const VertexInputState& vertexInputState,
        const InputAssemblyState& inputAssemblyState,
        const ViewportState& viewportState,
        const RasterizationState& rasterizationState,
        const MultisampleState& multisampleState,
        const DepthStencilState& depthStencilState,
        const ColorBlendState& colorBlendState,
        const VkPipelineLayout& pipelineLayout,
        const VkRenderPass& renderPass) {
        
        std::vector<VkPipelineShaderStageCreateInfo> pipelineShaderStages;
        for (const ShaderStage& shaderStage : shaderStages) {
            VkPipelineShaderStageCreateInfo pipelineShaderStage{};
            pipelineShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            pipelineShaderStage.module = shaderStage.module;
            pipelineShaderStage.stage = shaderStage.stage;
            pipelineShaderStage.pName = shaderStage.pName.c_str();

            pipelineShaderStages.push_back(pipelineShaderStage);
        }

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = vertexInputState.vertexBindingDescriptions.size();
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputState.vertexAttributeDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions = vertexInputState.vertexBindingDescriptions.data();
        vertexInputInfo.pVertexAttributeDescriptions = vertexInputState.vertexAttributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = inputAssemblyState.topology;
        inputAssembly.primitiveRestartEnable = inputAssemblyState.primitiveRestartEnable;

        VkViewport viewport{};
        viewport.x = static_cast<float>(viewportState.topX);
        viewport.y = static_cast<float>(viewportState.topY) + static_cast<float>(viewportState.height);
        viewport.width = static_cast<float>(viewportState.width);
        viewport.height = -static_cast<float>(viewportState.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.extent.width = (uint32_t)(viewportState.width);
        scissor.extent.height = (uint32_t)(viewportState.height);
        scissor.offset.x = viewportState.topX;
        scissor.offset.y = viewportState.topY;

        VkPipelineViewportStateCreateInfo pipelineViewport{};
        pipelineViewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        pipelineViewport.viewportCount = 1;
        pipelineViewport.pViewports = &viewport;
        pipelineViewport.scissorCount = 1;
        pipelineViewport.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = rasterizationState.depthClampEnable;
        rasterizer.rasterizerDiscardEnable = rasterizationState.rasterizerDiscardEnable;
        rasterizer.polygonMode = rasterizationState.polygonMode;
        rasterizer.cullMode = rasterizationState.cullMode;
        rasterizer.frontFace = rasterizationState.frontFace;
        rasterizer.depthBiasEnable = rasterizationState.depthBiasEnable;
        rasterizer.depthBiasConstantFactor = rasterizationState.depthBiasConstantFactor;
        rasterizer.depthBiasClamp = rasterizationState.depthBiasClamp;
        rasterizer.depthBiasSlopeFactor = rasterizationState.depthBiasSlopeFactor;
        rasterizer.lineWidth = rasterizationState.lineWidth;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = multisampleState.rasterizationSamples;
        multisampling.sampleShadingEnable = multisampleState.sampleShadingEnable;
        multisampling.minSampleShading = multisampleState.minSampleShading;
        multisampling.pSampleMask = multisampleState.sampleMask.data();
        multisampling.alphaToCoverageEnable = multisampleState.alphaToCoverageEnable;
        multisampling.alphaToOneEnable = multisampleState.alphaToOneEnable;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = depthStencilState.depthTestEnable;
        depthStencil.depthWriteEnable = depthStencilState.depthWriteEnable;
        depthStencil.depthCompareOp = depthStencilState.depthCompareOp;
        depthStencil.depthBoundsTestEnable = depthStencilState.depthBoundsTestEnable;
        depthStencil.stencilTestEnable = depthStencilState.stencilTestEnable;
        depthStencil.front = depthStencilState.front;
        depthStencil.back = depthStencilState.back;
        depthStencil.minDepthBounds = depthStencilState.minDepthBounds;
        depthStencil.maxDepthBounds = depthStencilState.maxDepthBounds;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = colorBlendState.logicOpEnable;
        colorBlending.logicOp = colorBlendState.logicOp;
        colorBlending.attachmentCount = (uint32_t)colorBlendState.attachments.size();
        colorBlending.pAttachments = colorBlendState.attachments.data();
        std::copy(&colorBlendState.blendConstants[0],
                  &colorBlendState.blendConstants[0] + sizeof(colorBlendState.blendConstants) / sizeof(colorBlendState.blendConstants[0]),
                  &colorBlending.blendConstants[0]);

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = (uint32_t)shaderStages.size();
        pipelineInfo.pStages = pipelineShaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &pipelineViewport;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        if (vkCreateGraphicsPipelines(device.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        //vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
        //vkDestroyShaderModule(m_device, vertShaderModule, nullptr);

    }
};