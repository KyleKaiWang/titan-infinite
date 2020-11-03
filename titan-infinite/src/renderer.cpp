/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#include "pch.h"
#include "renderer.h"

namespace renderer {

    VkImage createImage(
        const VkDevice& device,
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

    VkImageView createImageView(
        const VkDevice& device,
        VkImage image,
        VkImageViewType viewType,
        VkFormat format,
        const VkComponentMapping& components,
        const VkImageSubresourceRange& subresourceRange,
        VkImageViewCreateFlags flags) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.flags = flags;
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

    VkRenderPass createRenderPass(
        const VkDevice& device,
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
            subpass.colorAttachmentCount = static_cast<uint32_t>(description.colorAttachments.size());
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

        VkRenderPass renderPass;
        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass!");
        }
        return renderPass;
    }

    VkFramebuffer createFramebuffer(
        const VkDevice& device,
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
        return framebuffer;
    }

    VkShaderModule createShaderModule(const VkDevice& device, const std::vector<char>& code) {
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

    std::vector<ShaderStage> createShader(const VkDevice& device, const std::string& vertexShaderFile, const std::string& pixelShaderFile) {
        ShaderStage vertShaderStage{};
        vertShaderStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStage.module = renderer::createShaderModule(device, vkHelper::readFile(vertexShaderFile));
        vertShaderStage.pName = "main";

        ShaderStage fragShaderStage{};
        fragShaderStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStage.module = renderer::createShaderModule(device, vkHelper::readFile(pixelShaderFile));
        fragShaderStage.pName = "main";

        std::vector<ShaderStage> shaderStages{ vertShaderStage, fragShaderStage };
        return shaderStages;
    }

    ShaderStage createShader(const VkDevice& device, const std::string& computeShaderFile) {
        ShaderStage computeShaderStage{};
        computeShaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        computeShaderStage.module = renderer::createShaderModule(device, vkHelper::readFile(computeShaderFile));
        computeShaderStage.pName = "main";

        return computeShaderStage;
    }

    VkDescriptorSetLayout createDescriptorSetLayout(const VkDevice& device, const std::vector<DescriptorSetLayoutBinding>& descriptorSetLayoutBindings) {
        std::vector<VkDescriptorSetLayoutBinding> convertedBindings;
        for (const DescriptorSetLayoutBinding& binding : descriptorSetLayoutBindings) {
            VkDescriptorSetLayoutBinding convertedBinding;
            convertedBinding.binding = binding.binding;
            convertedBinding.descriptorType = binding.descriptorType;
            convertedBinding.descriptorCount = binding.descriptorCount;
            convertedBinding.stageFlags = binding.stageFlags;
            convertedBinding.pImmutableSamplers = binding.pImmutableSamplers;

            convertedBindings.emplace_back(convertedBinding);
        }
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindings.size());
        layoutInfo.pBindings = convertedBindings.data();

        VkDescriptorSetLayout descriptorSetLayout;
        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
        return descriptorSetLayout;
    }

    VkPipelineLayout createPipelineLayout(const VkDevice& device, const std::vector<VkDescriptorSetLayout>& descriptorSetLayout, const std::vector<VkPushConstantRange>& pushConstantRanges) {
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayout.size());
        pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
        pipelineLayoutInfo.pSetLayouts = descriptorSetLayout.data();
        pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();

        VkPipelineLayout pipelineLayout;
        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }
        return pipelineLayout;
    }

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
        vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputState.vertexBindingDescriptions.size());
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputState.vertexAttributeDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions = vertexInputState.vertexBindingDescriptions.data();
        vertexInputInfo.pVertexAttributeDescriptions = vertexInputState.vertexAttributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = inputAssemblyState.topology;
        inputAssembly.primitiveRestartEnable = inputAssemblyState.primitiveRestartEnable;

        VkViewport viewport{};
        viewport.x = static_cast<float>(viewportState.x);
        viewport.y = static_cast<float>(viewportState.y);
        viewport.width = static_cast<float>(viewportState.width);
        //viewport.height = -static_cast<float>(viewportState.height);
        viewport.height = static_cast<float>(viewportState.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.extent.width = (uint32_t)(viewportState.width);
        scissor.extent.height = (uint32_t)(viewportState.height);
        scissor.offset.x = viewportState.x;
        scissor.offset.y = viewportState.y;

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
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        VkPipeline pipeline;
        if (vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }
        return pipeline;
    }
    
    VkPipeline createComputePipeline(const VkDevice& device, const std::string& computeShaderFile, VkPipelineLayout layout, const VkSpecializationInfo* specializationInfo)
    {
        ShaderStage computeShader = renderer::createShader(device, computeShaderFile);

        VkPipelineShaderStageCreateInfo pipelineShaderStage{};
        pipelineShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipelineShaderStage.module = computeShader.module;
        pipelineShaderStage.stage = computeShader.stage;
        pipelineShaderStage.pName = computeShader.pName.c_str();
        pipelineShaderStage.pSpecializationInfo = specializationInfo;

        VkComputePipelineCreateInfo createInfo = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
        createInfo.stage = pipelineShaderStage;
        createInfo.layout = layout;

        VkPipeline pipeline;
        if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute pipeline");
        }

        vkDestroyShaderModule(device, computeShader.module, nullptr);
        return pipeline;
    }


    std::vector<VkDescriptorSet> createDescriptorSets(const VkDevice& device, const VkDescriptorPool& descriptorPool, const std::vector<VkDescriptorSetLayout>& descriptorSetLayout) {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = (uint32_t)descriptorSetLayout.size();
        allocInfo.pSetLayouts = descriptorSetLayout.data();

        std::vector<VkDescriptorSet> descriptorSets(descriptorSetLayout.size());
        if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }
        return descriptorSets;
    }

    VkDescriptorSet createDescriptorSet(const VkDevice& device, const VkDescriptorPool& descriptorPool, const VkDescriptorSetLayout& descriptorSetLayout) {
        
        std::vector<VkDescriptorSet> descriptorSet{ createDescriptorSets(device, descriptorPool, { descriptorSetLayout }) };       
        return descriptorSet[0];
    }
};