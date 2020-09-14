/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

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
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <optional>
#include <set>
#include <chrono>

#include "device.h"
#include "swapchain.h"
#include "command_pool.h"
#include "camera.h"
#include "window.h"
#include "renderer.h"
#include "buffer.h"
#include "texture.h"
#include "descriptor_pool.h"

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::mat4 sceneRotationMatrix;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
        attributeDescriptions.resize(3);

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }
};

const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
    0, 1, 2, 2, 3, 0,
    4, 5, 6, 6, 7, 4
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
    Device m_device;
    SwapChain m_swapChain;
    CommandPool m_commandPool;

    TextureObject m_texObj;

    VkImage m_depthImage;
    VkDeviceMemory m_depthImageMemory;
    VkImageView m_depthImageView;

    VkBuffer m_vertexBuffer;
    VkDeviceMemory m_vertexBufferMemory;
    VkBuffer m_indexBuffer;
    VkDeviceMemory m_indexBufferMemory;

    std::vector<VkBuffer> m_uniformBuffers;
    std::vector<VkDeviceMemory> m_uniformBuffersMemory;

    DescriptorPool m_descriptorPool;
    size_t m_currentFrame = 0;

    VkRenderPass m_renderPass;
    std::vector<VkFramebuffer> m_framebuffers;

    VkDescriptorSetLayout m_descriptorSetLayout;
    VkPipelineLayout m_pipelineLayout;
    VkPipeline m_graphicsPipeline;

    

    void initResource() {
        m_window.create(WIDTH, HEIGHT);
        
        // Physical, Logical Device and Surface
        m_device.create(m_window.getNativeWindow());

        // Swapchain
        m_swapChain.create(m_device.getPhysicalDevice(), m_device.getDevice(), m_device.getSurface());
        
        // Create Swapchain Image Views
        {
            m_swapChain.m_imageViews.resize(m_swapChain.m_images.size());
            for (uint32_t i = 0; i < m_swapChain.m_images.size(); i++) {
                m_swapChain.m_imageViews[i] = renderer::createImageView(m_device.getDevice(),
                    m_swapChain.m_images[i], 
                    VK_IMAGE_VIEW_TYPE_2D, 
                    m_swapChain.getSwapChainImageFormat(), 
                    { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
                    { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
            }
        }

        // CommandPool
        m_commandPool.create(m_device.getDevice());
        m_commandPool.createCommandPool(m_device.getDevice(), vkHelper::findQueueFamilies(m_device.getPhysicalDevice(), m_device.getSurface()).graphicsFamily.value());
        m_commandPool.m_commandBuffers = commandBuffer::allocate(m_device.getDevice(), m_commandPool.getCommandPool(), VK_COMMAND_BUFFER_LEVEL_PRIMARY, (uint32_t)m_swapChain.getSwapChainimages().size());
        
        // Depthbuffer
        {
            auto depthFormat = vkHelper::findDepthFormat(m_device.getPhysicalDevice());
            m_depthImage = renderer::createImage(m_device.getDevice(), 0, VK_IMAGE_TYPE_2D, depthFormat, { WIDTH, HEIGHT, 1 }, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_IMAGE_LAYOUT_UNDEFINED);
            
            VkMemoryRequirements memRequirements = memory::getMemoryRequirements(m_device.getDevice(), m_depthImage);
            m_depthImageMemory = memory::allocate(m_device.getDevice(), memRequirements.size, m_device.findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
            memory::bind(m_device.getDevice(), m_depthImageMemory, 0, m_depthImage);
            m_depthImageView = renderer::createImageView(m_device.getDevice(),
                m_depthImage, 
                VK_IMAGE_VIEW_TYPE_2D, 
                depthFormat, 
                { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A }, 
                { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 });
        }
            
        // RenderPass
        {
            VkAttachmentDescription colorAttachment{};
            colorAttachment.format = m_swapChain.getSwapChainImageFormat();
            colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            VkAttachmentDescription depthAttachment{};
            depthAttachment.format = vkHelper::findDepthFormat(m_device.getPhysicalDevice());
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

            m_renderPass = renderer::createRenderPass(m_device.getDevice(), attachmentDesc, subpassDesc, dependencies);
        }

        // Framebuffers
        {
            std::vector<VkImageView> imageViews =  m_swapChain.getSwapChainimageViews();
            m_framebuffers.resize(imageViews.size());
            for (std::size_t i = 0; i < m_swapChain.getSwapChainimages().size(); ++i) {
                m_framebuffers[i] = renderer::createFramebuffer(m_device.getDevice(), m_renderPass, { imageViews[i], m_depthImageView }, m_swapChain.getSwapChainExtent(), 1);
            }
        }

        // Uniformbuffer
        {
            VkDeviceSize bufferSize = sizeof(UniformBufferObject);
            m_uniformBuffers.resize(m_swapChain.getSwapChainimages().size());
            m_uniformBuffersMemory.resize(m_swapChain.getSwapChainimages().size());

            for (size_t i = 0; i < m_swapChain.getSwapChainimages().size(); ++i) {
                m_uniformBuffers[i] = buffer::createBuffer(m_device.getDevice(), bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);

                VkMemoryRequirements memReqs;
                vkGetBufferMemoryRequirements(m_device.getDevice(), m_uniformBuffers[i], &memReqs);

                VkMemoryAllocateInfo allocInfo{};
                allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                allocInfo.allocationSize = memReqs.size;
                allocInfo.memoryTypeIndex = m_device.findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

                if (vkAllocateMemory(m_device.getDevice(), &allocInfo, nullptr, &m_uniformBuffersMemory[i]) != VK_SUCCESS) {
                    throw std::runtime_error("failed to allocate uniform buffer memory!");
                }
                vkBindBufferMemory(m_device.getDevice(), m_uniformBuffers[i], m_uniformBuffersMemory[i], 0);
            }
        }

        // Vertexbuffer
        {
            VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
            m_vertexBuffer = buffer::createBuffer(m_device.getDevice(), bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);
           
            VkMemoryRequirements memReqs;
            vkGetBufferMemoryRequirements(m_device.getDevice(), m_vertexBuffer, &memReqs);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.pNext = nullptr;
            allocInfo.allocationSize = memReqs.size;
            allocInfo.memoryTypeIndex = m_device.findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            
            if (vkAllocateMemory(m_device.getDevice(), &allocInfo, nullptr, &m_vertexBufferMemory) != VK_SUCCESS) {
                throw std::runtime_error("failed to allocate vertex buffer memory!");
            }

            uint8_t* data;
            memory::map(m_device.getDevice(), m_vertexBufferMemory, 0, memReqs.size, (void**)&data);
            memcpy(data, vertices.data(), (size_t)bufferSize);
            memory::unmap(m_device.getDevice(), m_vertexBufferMemory);

            vkBindBufferMemory(m_device.getDevice(), m_vertexBuffer, m_vertexBufferMemory, 0);
        }

        // Indexbuffer
        {
            VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();
            m_indexBuffer = buffer::createBuffer(m_device.getDevice(), bufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);

            VkMemoryRequirements memReqs;
            vkGetBufferMemoryRequirements(m_device.getDevice(), m_indexBuffer, &memReqs);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memReqs.size;
            allocInfo.memoryTypeIndex = m_device.findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            if (vkAllocateMemory(m_device.getDevice(), &allocInfo, nullptr, &m_indexBufferMemory) != VK_SUCCESS) {
                throw std::runtime_error("failed to allocate index buffer memory!");
            }

            uint8_t* data;
            memory::map(m_device.getDevice(), m_indexBufferMemory, 0, bufferSize, (void**)&data);
            memcpy(data, indices.data(), (size_t)bufferSize);
            memory::unmap(m_device.getDevice(), m_indexBufferMemory);

            vkBindBufferMemory(m_device.getDevice(), m_indexBuffer, m_indexBufferMemory, 0);
        }

        // Texture
        {
            m_texObj = texture::loadTexture("data/textures/Checkerboard.png", m_device, m_commandPool, 4);
        }

        // Pipeline Layout and Discriptor Set
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

            m_descriptorSetLayout = renderer::createDescriptorSetLayout(m_device.getDevice(), descriptorSetLayoutBindings);
            m_pipelineLayout =  renderer::createPipelineLayout(m_device.getDevice(), { m_descriptorSetLayout }, {});
        }

        // Descriptor Pool
        {
            m_descriptorPool.create(m_device.getDevice());
            m_descriptorPool.createDescriptorPool(m_device.getDevice(), m_swapChain);
            m_descriptorPool.createDescriptorSets(m_device.getDevice(), m_swapChain, m_descriptorSetLayout);
            for (size_t i = 0; i < m_swapChain.getSwapChainimages().size(); ++i) {
                VkDescriptorBufferInfo bufferInfo{};
                bufferInfo.buffer = m_uniformBuffers[i];
                bufferInfo.offset = 0;
                bufferInfo.range = sizeof(UniformBufferObject);

                VkDescriptorImageInfo imageInfo{};
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfo.imageView = m_texObj.view;
                imageInfo.sampler = m_texObj.sampler;

                std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

                descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrites[0].dstSet = m_descriptorPool.m_descriptorSets[i];
                descriptorWrites[0].dstBinding = 0;
                descriptorWrites[0].dstArrayElement = 0;
                descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                descriptorWrites[0].descriptorCount = 1;
                descriptorWrites[0].pBufferInfo = &bufferInfo;

                descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrites[1].dstSet = m_descriptorPool.m_descriptorSets[i];
                descriptorWrites[1].dstBinding = 1;
                descriptorWrites[1].dstArrayElement = 0;
                descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                descriptorWrites[1].descriptorCount = 1;
                descriptorWrites[1].pImageInfo = &imageInfo;

                vkUpdateDescriptorSets(m_device.getDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
            }
        }

        // Graphics Pipeline
        {
            std::vector<VkVertexInputBindingDescription> bindingDescription{ Vertex::getBindingDescription() };
            VertexInputState vertexInput{};
            vertexInput.vertexBindingDescriptions = { Vertex::getBindingDescription() };
            vertexInput.vertexAttributeDescriptions = Vertex::getAttributeDescriptions();

            InputAssemblyState inputAssembly{};
            inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            inputAssembly.primitiveRestartEnable = VK_FALSE;

            ViewportState viewport{};
            viewport.topX = 0;
            viewport.topY = 0;
            viewport.width = m_swapChain.getSwapChainExtent().width;
            viewport.height = m_swapChain.getSwapChainExtent().height;

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
            depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
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

            std::vector<ShaderStage> shaderStages = renderer::createShader(m_device.getDevice(), "data/shaders/vert.spv", "data/shaders/frag.spv");
            m_graphicsPipeline =  renderer::createGraphicsPipeline(m_device.getDevice(), shaderStages, vertexInput, inputAssembly, viewport, rasterizer, multisampling, depthStencil, colorBlending, m_pipelineLayout, m_renderPass);
        }

        // Command Buffer
        createCommandBuffers();
    }

    void mainLoop() {
        while (!m_window.getWindowShouldClose()) {
            // Poll for user input.
            glfwPollEvents();
            //render();
            drawFrame();
        }
        vkDeviceWaitIdle(m_device.getDevice());
    }

    void render() {
        const VkDeviceSize zeroOffset = 0;
        VkCommandBuffer commandBuffer = m_commandPool.getCommandBuffers()[m_currentFrame];
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
            beginInfo.renderArea.extent = m_swapChain.getSwapChainExtent();
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

    void drawFrame() {
        vkWaitForFences(m_device.getDevice(), 1, &m_swapChain.m_cmdBufExecutedFences[m_currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(m_device.getDevice(), m_swapChain.getSwapChain(), UINT64_MAX, m_swapChain.m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            //recreateSwapChain();
            return;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        updateUniformBuffer(imageIndex);

        if (m_swapChain.m_imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
            vkWaitForFences(m_device.getDevice(), 1, &m_swapChain.m_imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        }
        m_swapChain.m_imagesInFlight[imageIndex] = m_swapChain.m_cmdBufExecutedFences[m_currentFrame];
        
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = { m_swapChain.m_imageAvailableSemaphores[m_currentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        //submitInfo.commandBufferCount = m_command.m_commandBuffers.size();
        submitInfo.pCommandBuffers = &m_commandPool.m_commandBuffers[imageIndex];

        VkSemaphore signalSemaphores[] = { m_swapChain.m_renderFinishedSemaphores[m_currentFrame] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        vkResetFences(m_device.getDevice(), 1, &m_swapChain.m_cmdBufExecutedFences[m_currentFrame]);

        commandBuffer::submitCommandBuffer(m_device.getGraphicsQueue(), &submitInfo, m_swapChain.m_cmdBufExecutedFences[m_currentFrame]);

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = { m_swapChain.getSwapChain() };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;

        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(m_device.getPresentQueue(), &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_window.getFramebufferResized()) {
            m_window.setFramebufferResized(false);
            //recreateSwapChain();
        }
        else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        m_currentFrame = (m_currentFrame + 1) % 2;
    }

    void updateUniformBuffer(uint32_t currentImage) {
        UniformBufferObject ubo{};
        ubo.model = glm::mat4(1.0f);

        glm::mat4 sceneRotationMatrix = glm::mat4(1.0f);
        sceneRotationMatrix = glm::rotate(sceneRotationMatrix, glm::radians(m_window.getSceneSettings().pitch), glm::vec3(1.0f, 0.0f, 0.0f));
        sceneRotationMatrix = glm::rotate(sceneRotationMatrix, glm::radians(m_window.getSceneSettings().yaw), glm::vec3(0.0f, 1.0f, 0.0f));
        sceneRotationMatrix = glm::rotate(sceneRotationMatrix, glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

        ubo.view = glm::lookAt(glm::vec3(0.0f, 0.0f, m_window.getCamera().distance), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        ubo.proj = glm::perspective(m_window.getCamera().fov, m_swapChain.getSwapChainExtent().width / (float)m_swapChain.getSwapChainExtent().height, 0.1f, 10.0f);
        ubo.proj[1][1] *= -1;
        ubo.sceneRotationMatrix = sceneRotationMatrix;
        
        void* data;
        auto res = vkMapMemory(m_device.getDevice(), m_uniformBuffersMemory[currentImage], 0, sizeof(ubo), 0, &data);
        assert(res == VK_SUCCESS);

        memcpy(data, &ubo, sizeof(ubo));
        vkUnmapMemory(m_device.getDevice(), m_uniformBuffersMemory[currentImage]);
    }

    void cleanupSwapChain() {
        for (auto framebuffer : m_framebuffers) {
            vkDestroyFramebuffer(m_device.getDevice(), framebuffer, nullptr);
        }
        vkDestroyPipeline(m_device.getDevice(), m_graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(m_device.getDevice(), m_pipelineLayout, nullptr);
        vkDestroyRenderPass(m_device.getDevice(), m_renderPass, nullptr);
        m_commandPool.destroy();
        m_descriptorPool.destroy();
        for (size_t i = 0; i < m_swapChain.getSwapChainimages().size(); ++i) {
            vkDestroyBuffer(m_device.getDevice(), m_uniformBuffers[i], nullptr);
            vkFreeMemory(m_device.getDevice(), m_uniformBuffersMemory[i], nullptr);
        }
        m_swapChain.destroy();
    }

    void cleanup() {
    
        vkDestroySampler(m_device.getDevice(), m_texObj.sampler, nullptr);
        vkDestroyImageView(m_device.getDevice(), m_texObj.view, nullptr);
    
        vkDestroyImage(m_device.getDevice(), m_texObj.image, nullptr);
        vkFreeMemory(m_device.getDevice(), m_texObj.bufferMemory, nullptr);
    
    
        vkDestroyBuffer(m_device.getDevice(), m_indexBuffer, nullptr);
        vkFreeMemory(m_device.getDevice(), m_indexBufferMemory, nullptr);
    
        vkDestroyBuffer(m_device.getDevice(), m_vertexBuffer, nullptr);
        vkFreeMemory(m_device.getDevice(), m_vertexBufferMemory, nullptr);
        
        vkDestroyDescriptorSetLayout(m_device.getDevice(), m_descriptorSetLayout, nullptr);
        cleanupSwapChain();
        m_device.destroy();
        m_window.destroy();
    }

    void createCommandBuffers() {
        m_commandPool.m_commandBuffers.resize(m_framebuffers.size());
        for (size_t i = 0; i < m_commandPool.getCommandBuffers().size(); ++i) {
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = 0;
            beginInfo.pInheritanceInfo = nullptr;

            if (vkBeginCommandBuffer(m_commandPool.getCommandBuffers()[i], &beginInfo) != VK_SUCCESS) {
                throw std::runtime_error("failed to begin recording command buffer!");
            }

            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = m_renderPass;
            renderPassInfo.framebuffer = m_framebuffers[i];
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = m_swapChain.getSwapChainExtent();

            std::array<VkClearValue, 2> clearValues{};
            clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
            clearValues[1].depthStencil = { 1.0f, 0 };

            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(m_commandPool.m_commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(m_commandPool.m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

            VkBuffer vertexBuffers[] = { m_vertexBuffer };
            VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(m_commandPool.m_commandBuffers[i], 0, 1, vertexBuffers, offsets);

            vkCmdBindIndexBuffer(m_commandPool.m_commandBuffers[i], m_indexBuffer, 0, VK_INDEX_TYPE_UINT16);

            vkCmdBindDescriptorSets(m_commandPool.m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorPool.m_descriptorSets[i], 0, nullptr);

            vkCmdDrawIndexed(m_commandPool.m_commandBuffers[i], static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

            vkCmdEndRenderPass(m_commandPool.m_commandBuffers[i]);

            if (vkEndCommandBuffer(m_commandPool.m_commandBuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to record command buffer!");
            }
        }
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
