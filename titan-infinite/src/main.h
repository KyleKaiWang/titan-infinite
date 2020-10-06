/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */
#pragma once

// Enable the WSI extensions
#if defined(__ANDROID__)
#define VK_USE_PLATFORM_ANDROID_KHR
#elif defined(__linux__)
#define VK_USE_PLATFORM_XLIB_KHR
#elif defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <optional>
#include <set>
#include <chrono>
#include "device.h"
#include "camera.h"
#include "window.h"
#include "buffer.h"
#include "texture.h"
#include "mesh.h"

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 sceneRotationMatrix;
    glm::vec4 lightPos = glm::vec4(5.0f, 5.0f, -5.0f, 1.0f);
};

class Application {
public:
    void run() {
        initResource();
        mainLoop();
        cleanup();
    }

private:
    Window m_window;
    Device* m_device;

    VkImage m_depthImage;
    VkDeviceMemory m_depthImageMemory;
    VkImageView m_depthImageView;
    std::vector<Resource<VkBuffer>> m_uniformBuffers;

    size_t m_currentFrame = 0;

    VkRenderPass m_renderPass;
    std::vector<VkFramebuffer> m_framebuffers;

    std::vector<VkDescriptorSet> m_descriptorSets;
    VkDescriptorSet m_descriptorSet;
    VkDescriptorSetLayout m_descriptorSetLayout;
    VkPipelineLayout m_pipelineLayout;
    VkPipeline m_graphicsPipeline;
    VkPipelineCache m_pipelineCache;

    // glTF
    struct DemoModel {
        vkglTF::Model* glTF;
        VkPipeline* pipeline;
    };
    std::vector<DemoModel> demoModels;

    UniformBufferObject uboVS;

    struct {
        Resource<VkBuffer> meshVS;
    } uniformData;

    struct
    {
        TextureObject skybox;
    } textures;

    struct {
        VkPipeline logos;
        VkPipeline models;
        VkPipeline skybox;
    } pipelines;

    void initResource() {
        m_window.create(WIDTH, HEIGHT);
        
        // Physical, Logical Device and Surface
        m_device = new Device();
        m_device->create(m_window.getNativeWindow());

        // Depthbuffer
        {
            auto depthFormat = vkHelper::findDepthFormat(m_device->getPhysicalDevice());
            m_depthImage = renderer::createImage(m_device->getDevice(), 0, VK_IMAGE_TYPE_2D, depthFormat, { WIDTH, HEIGHT, 1 }, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_IMAGE_LAYOUT_UNDEFINED);
            
            VkMemoryRequirements memRequirements = memory::getMemoryRequirements(m_device->getDevice(), m_depthImage);
            m_depthImageMemory = memory::allocate(m_device->getDevice(), memRequirements.size, m_device->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
            memory::bind(m_device->getDevice(), m_depthImageMemory, 0, m_depthImage);
            m_depthImageView = renderer::createImageView(m_device->getDevice(),
                m_depthImage, 
                VK_IMAGE_VIEW_TYPE_2D, 
                depthFormat, 
                { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A }, 
                { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 });
        }
            
        // RenderPass
        {
            VkAttachmentDescription colorAttachment{};
            colorAttachment.format = m_device->getSwapChainImageFormat();
            colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            VkAttachmentDescription depthAttachment{};
            depthAttachment.format = vkHelper::findDepthFormat(m_device->getPhysicalDevice());
            depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            std::vector<VkAttachmentDescription> attachmentDesc{ colorAttachment, depthAttachment };

            VkAttachmentReference colorAttachmentRef{};
            colorAttachmentRef.attachment = 0;
            colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            std::vector<VkAttachmentReference> colorAttachmentRefs{ colorAttachmentRef };

            VkAttachmentReference depthAttachmentRef{};
            depthAttachmentRef.attachment = 1;
            depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            SubpassDescription subpass{};
            subpass.colorAttachments = colorAttachmentRefs;
            subpass.depthStencilAttachment = depthAttachmentRef;

            std::vector<SubpassDescription> subpassDesc{ subpass };

            VkSubpassDependency dependency{};
            dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstSubpass = 0;
            dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.srcAccessMask = 0;
            dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            std::vector<VkSubpassDependency> dependencies{ dependency };

            m_renderPass = renderer::createRenderPass(m_device->getDevice(), attachmentDesc, subpassDesc, dependencies);
        }

        // Framebuffers
        {
            std::vector<VkImageView> imageViews = m_device->getSwapChainimageViews();
            m_framebuffers.resize(imageViews.size());
            for (std::size_t i = 0; i < m_device->getSwapChainimages().size(); ++i) {
                m_framebuffers[i] = renderer::createFramebuffer(m_device->getDevice(), m_renderPass, { imageViews[i], m_depthImageView }, m_device->getSwapChainExtent(), 1);
            }
        }
        loadAssets();
        initUniformBuffers();
        initDescriptorPool();
        initDescriptorSetLayout();
        initDescriptorSet();
        initPipelines();
        buildCommandBuffers();
    }

    void loadAssets() {
        // Models
        std::vector<std::string> modelFiles = { "DamagedHelmet/glTF-Embedded/DamagedHelmet.gltf" };
        std::vector<VkPipeline*> modelPipelines = { &pipelines.logos, &pipelines.models, &pipelines.models, &pipelines.skybox };
        for (auto i = 0; i < modelFiles.size(); i++) {
            DemoModel model;
            const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
            model.pipeline = modelPipelines[i];
            model.glTF = new vkglTF::Model();
            model.glTF->loadFromFile("data/models/" + modelFiles[i], m_device, m_device->getGraphicsQueue(), glTFLoadingFlags);
            demoModels.push_back(model);
        }
        // Textures
        //TODO : Cubemap loading
        textures.skybox = texture::loadTexture("data/textures/Checkerboard.png", m_device, 4);
    }

    void initUniformBuffers() {
        // Vertex shader uniform buffer block
        uniformData.meshVS = buffer::createBuffer(
            m_device,
            sizeof(uboVS),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            );
        memory::map(m_device->getDevice(), uniformData.meshVS.memory, 0, VK_WHOLE_SIZE, &uniformData.meshVS.mapped);
        updateUniformBuffer();
    }

    void updateUniformBuffer() {
        uboVS.model = glm::mat4(1.0f);

        glm::mat4 sceneRotationMatrix = glm::mat4(1.0f);
        sceneRotationMatrix = glm::rotate(sceneRotationMatrix, glm::radians(m_window.getSceneSettings().pitch), glm::vec3(1.0f, 0.0f, 0.0f));
        sceneRotationMatrix = glm::rotate(sceneRotationMatrix, glm::radians(m_window.getSceneSettings().yaw), glm::vec3(0.0f, 1.0f, 0.0f));
        sceneRotationMatrix = glm::rotate(sceneRotationMatrix, glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

        uboVS.view = glm::lookAt(glm::vec3(0.0f, 0.0f, m_window.getCamera().distance), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        uboVS.proj = glm::perspective(m_window.getCamera().fov, m_device->getSwapChainExtent().width / (float)m_device->getSwapChainExtent().height, 0.1f, 10.0f);
        uboVS.proj[1][1] *= -1;
        uboVS.sceneRotationMatrix = sceneRotationMatrix;
        
        memcpy(uniformData.meshVS.mapped, &uboVS, sizeof(uboVS));
    }

    void initDescriptorSetLayout() {

        {
            DescriptorSetLayoutBinding uboLayoutBinding{};
            uboLayoutBinding.binding = 0;
            uboLayoutBinding.descriptorCount = 1;
            uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

            DescriptorSetLayoutBinding samplerLayoutBinding{};
            samplerLayoutBinding.binding = 1;
            samplerLayoutBinding.descriptorCount = 1;
            samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

            std::vector<DescriptorSetLayoutBinding> descriptorSetLayoutBindings{ uboLayoutBinding, samplerLayoutBinding };

            m_descriptorSetLayout = renderer::createDescriptorSetLayout(m_device->getDevice(), descriptorSetLayoutBindings);

            VkPushConstantRange pushConstantRange{};
            pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            pushConstantRange.size = sizeof(glm::mat4);
            pushConstantRange.offset = 0;

            m_pipelineLayout = renderer::createPipelineLayout(m_device->getDevice(), { m_descriptorSetLayout }, { pushConstantRange });
        }
    }

    void initDescriptorSet()
    {
        m_descriptorSets = renderer::createDescriptorSets(m_device->getDevice(), m_device->getDescriptorPool(), { m_descriptorSetLayout });
        m_descriptorSet = m_descriptorSets[0];

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
        
        // Binding 0 : Vertex shader uniform buffer
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = m_descriptorSet;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &uniformData.meshVS.descriptor;

        // Binding 1 : Fragment shader image sampler
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = m_descriptorSet;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &textures.skybox.descriptor;

        vkUpdateDescriptorSets(m_device->getDevice(), descriptorWrites.size(), descriptorWrites.data(), 0, NULL);
    }

    void initPipelines()
    {
        auto vertexInput = vkglTF::Vertex::getVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV, vkglTF::VertexComponent::Color });

        InputAssemblyState inputAssembly{};
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        ViewportState viewport{};
        viewport.topX = 0;
        viewport.topY = 0;
        viewport.width = m_device->getSwapChainExtent().width;
        viewport.height = m_device->getSwapChainExtent().height;

        RasterizationState rasterizer{};
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        MultisampleState multisampling{};
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        DepthStencilState depthStencil{};
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        std::vector<VkPipelineColorBlendAttachmentState> attachments{ colorBlendAttachment };

        ColorBlendState colorBlending{};
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachments = attachments;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.pDynamicStates = dynamicStates.data();
        dynamicState.dynamicStateCount = 0;

        // Default mesh rendering pipeline
        std::vector<ShaderStage> shaderStages_mesh = renderer::createShader(m_device->getDevice(), "data/shaders/mesh.vert.spv", "data/shaders/mesh.frag.spv");
        pipelines.models = renderer::createGraphicsPipeline(m_device->getDevice(), m_pipelineCache, shaderStages_mesh, *vertexInput, inputAssembly, viewport, rasterizer, multisampling, depthStencil, colorBlending, dynamicState, m_pipelineLayout, m_renderPass);

        // Pipeline for the logos
        std::vector<ShaderStage> shaderStages_logos = renderer::createShader(m_device->getDevice(), "data/shaders/logo.vert.spv", "data/shaders/logo.frag.spv");
        pipelines.logos = renderer::createGraphicsPipeline(m_device->getDevice(), m_pipelineCache, shaderStages_logos, *vertexInput, inputAssembly, viewport, rasterizer, multisampling, depthStencil, colorBlending, dynamicState, m_pipelineLayout, m_renderPass);

        // Pipeline for the sky sphere
        rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
        depthStencil.depthWriteEnable = VK_FALSE;
        std::vector<ShaderStage> shaderStages_skybox = renderer::createShader(m_device->getDevice(), "data/shaders/skybox.vert.spv", "data/shaders/skybox.frag.spv");
        pipelines.skybox = renderer::createGraphicsPipeline(m_device->getDevice(), m_pipelineCache, shaderStages_skybox, *vertexInput, inputAssembly, viewport, rasterizer, multisampling, depthStencil, colorBlending, dynamicState, m_pipelineLayout, m_renderPass);
    }

    void initDescriptorPool()
    {
        // Example uses one ubo and one image sampler
        std::vector<VkDescriptorPoolSize> poolSizes =
        {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }
        };
        m_device->createDescriptorPool(
            m_device->getDevice(),
            poolSizes,
            2
            );
    }

    void mainLoop() {
        while (!m_window.getWindowShouldClose()) {
            // Poll for user input.
            glfwPollEvents();
            drawFrame();
        }
        vkDeviceWaitIdle(m_device->getDevice());
    }

    void render() {
        const VkDeviceSize zeroOffset = 0;
        VkCommandBuffer commandBuffer = m_device->getCommandBuffers()[m_currentFrame];
        VkFramebuffer framebuffer = m_framebuffers[m_currentFrame];

        // Begin recording current frame command buffer.
        {
            VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

            vkResetCommandBuffer(commandBuffer, 0);
            vkBeginCommandBuffer(commandBuffer, &beginInfo);
        }

        // Begin render pass
        {
            std::array<VkClearValue, 2> clearValues = {};
            clearValues[1].depthStencil.depth = 1.0f;

            VkRenderPassBeginInfo beginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
            beginInfo.renderPass = m_renderPass;
            beginInfo.framebuffer = framebuffer;
            beginInfo.renderArea.offset = { 0, 0 };
            beginInfo.renderArea.extent = m_device->getSwapChainExtent();
            beginInfo.clearValueCount = (uint32_t)clearValues.size();
            beginInfo.pClearValues = clearValues.data();

            // Begin in main draw subpass
            vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
        }

        vkCmdEndRenderPass(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void buildCommandBuffers()
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        beginInfo.pInheritanceInfo = nullptr;

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_renderPass;
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = m_device->getSwapChainExtent();

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
        clearValues[1].depthStencil = { 1.0f, 0 };

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        for (int32_t i = 0; i < m_device->getCommandBuffers().size(); ++i)
        {
            renderPassInfo.framebuffer = m_framebuffers[i];
            
            if (vkBeginCommandBuffer(m_device->m_commandBuffers[i], &beginInfo) != VK_SUCCESS) {
                throw std::runtime_error("failed to begin recording command buffer!");
            }

            vkCmdBeginRenderPass(m_device->m_commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindDescriptorSets(m_device->m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_device->m_descriptorSets[i], 0, nullptr);

            vkCmdBindDescriptorSets(m_device->m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, m_descriptorSets.data(), 0, NULL);

            for (auto model : demoModels) {
                vkCmdBindPipeline(m_device->m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, *model.pipeline);
                model.glTF->draw(m_device->m_commandBuffers[i]);
            }

            vkCmdEndRenderPass(m_device->m_commandBuffers[i]);

            vkEndCommandBuffer(m_device->m_commandBuffers[i]);
        }
    }

    void drawFrame() {
        vkWaitForFences(m_device->getDevice(), 1, &m_device->m_cmdBufExecutedFences[m_currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(m_device->getDevice(), m_device->getSwapChain(), UINT64_MAX, m_device->m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            //recreateSwapChain();
            return;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        updateUniformBuffer();

        if (m_device->m_imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
            vkWaitForFences(m_device->getDevice(), 1, &m_device->m_imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        }
        m_device->m_imagesInFlight[imageIndex] = m_device->m_cmdBufExecutedFences[m_currentFrame];
        
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = { m_device->m_imageAvailableSemaphores[m_currentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_device->m_commandBuffers[imageIndex];

        VkSemaphore signalSemaphores[] = { m_device->m_renderFinishedSemaphores[m_currentFrame] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        vkResetFences(m_device->getDevice(), 1, &m_device->m_cmdBufExecutedFences[m_currentFrame]);

        renderer::submitCommandBuffer(m_device->getGraphicsQueue(), &submitInfo, m_device->m_cmdBufExecutedFences[m_currentFrame]);

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = { m_device->getSwapChain() };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;

        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(m_device->getPresentQueue(), &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_window.getFramebufferResized()) {
            m_window.setFramebufferResized(false);
            //recreateSwapChain();
        }
        else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        m_currentFrame = (m_currentFrame + 1) % 2;
    }

    void cleanupSwapChain() {
        for (auto framebuffer : m_framebuffers) {
            vkDestroyFramebuffer(m_device->getDevice(), framebuffer, nullptr);
        }
        vkDestroyPipeline(m_device->getDevice(), m_graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(m_device->getDevice(), m_pipelineLayout, nullptr);
        vkDestroyRenderPass(m_device->getDevice(), m_renderPass, nullptr);
        m_device->destroyCommandPool();
        m_device->destroyDescriptorPool();
        for (size_t i = 0; i < m_device->getSwapChainimages().size(); ++i) {
            vkDestroyBuffer(m_device->getDevice(), m_uniformBuffers[i].resource, nullptr);
            vkFreeMemory(m_device->getDevice(), m_uniformBuffers[i].memory, nullptr);
        }
        m_device->destroy();
    }

    void cleanup() {
        vkDestroyDescriptorSetLayout(m_device->getDevice(), m_descriptorSetLayout, nullptr);
        cleanupSwapChain();
        m_device->destroy();
        delete m_device;
        m_window.destroy();
    }

    void destroyBuffer(const VkDevice& device, Resource<VkBuffer>& buffer) const
    {
        if (buffer.resource != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, buffer.resource, nullptr);
        }
        if (buffer.memory != VK_NULL_HANDLE) {
            vkFreeMemory(device, buffer.memory, nullptr);
        }
        buffer = {};
    }
};

int main()
{
    Application app;
    try {
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        system("pause");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
