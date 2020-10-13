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

#include <windows.h>
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
#include "model.h"



class Application {
public:
    Application() {}
    ~Application() {}

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

    size_t m_currentFrame = 0;

    VkRenderPass m_renderPass;
    std::vector<VkFramebuffer> m_framebuffers;

    VkDescriptorSet m_descriptorSet;
    VkPipelineLayout m_pipelineLayout;
    VkPipelineCache m_pipelineCache;

    // glTF
    VulkanglTFModel glTFModel;

    struct Pipelines
    {
        VkPipeline solid;
        VkPipeline wireframe = VK_NULL_HANDLE;
    } pipelines;

    bool wireframe = false;

    struct DescriptorSetLayouts
    {
        VkDescriptorSetLayout matrices;
        VkDescriptorSetLayout textures;
        VkDescriptorSetLayout jointMatrices;
    } descriptorSetLayouts;

    struct UniformBufferObject {
        Resource<VkBuffer> buffer;
        struct Values {
            glm::mat4 view;
            glm::mat4 proj;
            glm::mat4 sceneRotationMatrix;
            glm::vec4 lightPos = glm::vec4(5.0f, 5.0f, 5.0f, 1.0f);
        }values;
    }shaderData;
    
    struct
    {
        TextureObject skybox;
    } textures;

    // Mesuring frame time
    float frameTimer = 1.0f;
    uint32_t frameCounter = 0;
    uint32_t lastFPS = 0;
    std::chrono::time_point<std::chrono::high_resolution_clock> lastTimestamp;

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

        {
            VkPipelineCacheCreateInfo pipelineCacheCreateInfo{};
            pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
            vkCreatePipelineCache(m_device->getDevice(), &pipelineCacheCreateInfo, nullptr, &m_pipelineCache);
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
        tinygltf::Model    glTFInput;
        tinygltf::TinyGLTF gltfContext;
        std::string        error, warning;
        bool fileLoaded = gltfContext.LoadASCIIFromFile(&glTFInput, &error, &warning, "data/models/glTF-Embedded/CesiumMan.gltf");

        // Pass some Vulkan resources required for setup and rendering to the glTF model loading class
        glTFModel.device = m_device;
        glTFModel.copyQueue = m_device->getGraphicsQueue();

        std::vector<uint32_t>                indexBuffer;
        std::vector<VulkanglTFModel::Vertex> vertexBuffer;

        if (fileLoaded)
        {
            glTFModel.loadImages(glTFInput);
            glTFModel.loadMaterials(glTFInput);
            glTFModel.loadTextures(glTFInput);
            const tinygltf::Scene& scene = glTFInput.scenes[0];
            for (size_t i = 0; i < scene.nodes.size(); i++)
            {
                const tinygltf::Node node = glTFInput.nodes[scene.nodes[i]];
                glTFModel.loadNode(node, glTFInput, nullptr, scene.nodes[i], indexBuffer, vertexBuffer);
            }
            glTFModel.loadSkins(glTFInput);
            glTFModel.loadAnimations(glTFInput);
            // Calculate initial pose
            for (auto node : glTFModel.nodes)
            {
                glTFModel.updateJoints(node);
            }
        }
        else
        {
            throw("Could not open the glTF file.\n\nThe file is part of the additional asset pack.\n\nRun \"download_assets.py\" in the repository root to download the latest version.", -1);
            return;
        }

        // Create and upload vertex and index buffer
        size_t vertexBufferSize = vertexBuffer.size() * sizeof(VulkanglTFModel::Vertex);
        size_t indexBufferSize = indexBuffer.size() * sizeof(uint32_t);
        glTFModel.indices.count = static_cast<uint32_t>(indexBuffer.size());

        struct StagingBuffer
        {
            VkBuffer       buffer;
            VkDeviceMemory memory;
        } vertexStaging, indexStaging;

        // Create host visible staging buffers (source)
        buffer::createBuffer(
            m_device,
            vertexBufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &vertexStaging.buffer,
            &vertexStaging.memory,
            vertexBuffer.data());

        // Index data
        buffer::createBuffer(
            m_device,
            indexBufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &indexStaging.buffer,
            &indexStaging.memory,
            indexBuffer.data());

        // Create device local buffers (target)
        buffer::createBuffer(
            m_device,
            vertexBufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &glTFModel.vertices.buffer,
            &glTFModel.vertices.memory);


        buffer::createBuffer(
            m_device,
            indexBufferSize,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &glTFModel.indices.buffer,
            &glTFModel.indices.memory);

        // Copy data from staging buffers (host) do device local buffer (gpu)
        VkCommandBuffer copyCmd = renderer::createCommandBuffer(m_device->getDevice(), m_device->getCommandPool(), VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        VkBufferCopy    copyRegion = {};
        copyRegion.size = vertexBufferSize;
        vkCmdCopyBuffer(copyCmd, vertexStaging.buffer, glTFModel.vertices.buffer, 1, &copyRegion);
        copyRegion.size = indexBufferSize;
        vkCmdCopyBuffer(copyCmd, indexStaging.buffer, glTFModel.indices.buffer, 1, &copyRegion);
        renderer::flushCommandBuffer(m_device->getDevice(), copyCmd, m_device->getGraphicsQueue(), m_device->getCommandPool(), true);

        // Free staging resources
        vkDestroyBuffer(m_device->getDevice(), vertexStaging.buffer, nullptr);
        vkFreeMemory(m_device->getDevice(), vertexStaging.memory, nullptr);
        vkDestroyBuffer(m_device->getDevice(), indexStaging.buffer, nullptr);
        vkFreeMemory(m_device->getDevice(), indexStaging.memory, nullptr);

        // Textures
        //TODO : Cubemap loading
        //textures.skybox = texture::loadTexture("data/textures/Checkerboard.png", VK_FORMAT_R8G8B8A8_UNORM, m_device, 4);
    }

    void initUniformBuffers() {
        // Vertex shader uniform buffer block
        shaderData.buffer = buffer::createBuffer(
            m_device,
            sizeof(shaderData.values),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            );
        memory::map(m_device->getDevice(), shaderData.buffer.memory, 0, VK_WHOLE_SIZE, &shaderData.buffer.mapped);
        updateUniformBuffer();
    }

    void updateUniformBuffer() {
        glm::mat4 sceneRotationMatrix = glm::mat4(1.0f);
        sceneRotationMatrix = glm::rotate(sceneRotationMatrix, glm::radians(m_window.getSceneSettings().pitch), glm::vec3(1.0f, 0.0f, 0.0f));
        sceneRotationMatrix = glm::rotate(sceneRotationMatrix, glm::radians(m_window.getSceneSettings().yaw), glm::vec3(0.0f, 1.0f, 0.0f));
        sceneRotationMatrix = glm::rotate(sceneRotationMatrix, glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

        shaderData.values.view = glm::lookAt(glm::vec3(0.0f, 0.0f, m_window.getCamera().distance), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        shaderData.values.proj = glm::perspective(m_window.getCamera().fov, m_device->getSwapChainExtent().width / (float)m_device->getSwapChainExtent().height, 0.1f, 10.0f);
        shaderData.values.proj[1][1] *= -1;
        shaderData.values.sceneRotationMatrix = sceneRotationMatrix;
        
        memcpy(shaderData.buffer.mapped, &shaderData.values, sizeof(shaderData.values));
    }

    void initDescriptorPool()
    {
        std::vector<VkDescriptorPoolSize> poolSizes =
        {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(glTFModel.images.size()) },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<uint32_t>(glTFModel.skins.size()) }
        };
        m_device->createDescriptorPool(
            m_device->getDevice(),
            poolSizes,
            static_cast<uint32_t>(glTFModel.images.size()) + static_cast<uint32_t>(glTFModel.skins.size()) + 1
        );
    }

    void initDescriptorSetLayout() {

        {
            DescriptorSetLayoutBinding uboLayoutBinding{};
            uboLayoutBinding.binding = 0;
            uboLayoutBinding.descriptorCount = 1;
            uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

            DescriptorSetLayoutBinding jointLayoutBinding{};
            jointLayoutBinding.binding = 0;
            jointLayoutBinding.descriptorCount = 1;
            jointLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            jointLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            
            DescriptorSetLayoutBinding samplerLayoutBinding{};
            samplerLayoutBinding.binding = 0;
            samplerLayoutBinding.descriptorCount = 1;
            samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;


            descriptorSetLayouts.matrices = renderer::createDescriptorSetLayout(m_device->getDevice(), { uboLayoutBinding });
            descriptorSetLayouts.jointMatrices = renderer::createDescriptorSetLayout(m_device->getDevice(), { jointLayoutBinding });
            descriptorSetLayouts.textures = renderer::createDescriptorSetLayout(m_device->getDevice(), { samplerLayoutBinding });

            VkPushConstantRange pushConstantRange{};
            pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            pushConstantRange.size = sizeof(glm::mat4);
            pushConstantRange.offset = 0;

            m_pipelineLayout = renderer::createPipelineLayout(m_device->getDevice(), { descriptorSetLayouts.matrices, descriptorSetLayouts.jointMatrices, descriptorSetLayouts.textures }, { pushConstantRange });
        }
    }

    void initDescriptorSet()
    {
        {
            m_descriptorSet = renderer::createDescriptorSet(m_device->getDevice(), m_device->getDescriptorPool(), descriptorSetLayouts.matrices);
            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = m_descriptorSet;
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &shaderData.buffer.descriptor;
            vkUpdateDescriptorSets(m_device->getDevice(), 1, &descriptorWrite, 0, nullptr);
        }

        for (auto& skin : glTFModel.skins)
        {
            skin.descriptorSet = renderer::createDescriptorSet(m_device->getDevice(), m_device->getDescriptorPool(), descriptorSetLayouts.jointMatrices);
            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = skin.descriptorSet;
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &skin.ssbo.descriptor;
            vkUpdateDescriptorSets(m_device->getDevice(), 1, &descriptorWrite, 0, nullptr);
        }

        for (auto& image : glTFModel.images)
        {
            image.descriptorSet = renderer::createDescriptorSet(m_device->getDevice(), m_device->getDescriptorPool(), descriptorSetLayouts.textures);
            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = image.descriptorSet;
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pImageInfo = &image.texture.descriptor;
            vkUpdateDescriptorSets(m_device->getDevice(), 1, &descriptorWrite, 0, nullptr);
        }
    }

    void initPipelines()
    {
        VertexInputState vertexInputState = { 
            { { 0, sizeof(VulkanglTFModel::Vertex), VK_VERTEX_INPUT_RATE_VERTEX } },
            
            { {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VulkanglTFModel::Vertex, pos)},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VulkanglTFModel::Vertex, normal)},
            {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VulkanglTFModel::Vertex, uv)},
            {3, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VulkanglTFModel::Vertex, color)},
            
            // POI: Per-Vertex Joint indices and weights are passed to the vertex shader
            {4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VulkanglTFModel::Vertex, jointIndices)},
            {5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VulkanglTFModel::Vertex, jointWeights)} }
        };

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

        // Solid rendering pipeline
        std::vector<ShaderStage> shaderStages_mesh = renderer::createShader(m_device->getDevice(), "data/shaders/mesh.vert.spv", "data/shaders/mesh.frag.spv");
        pipelines.solid = renderer::createGraphicsPipeline(m_device->getDevice(), m_pipelineCache, shaderStages_mesh, vertexInputState, inputAssembly, viewport, rasterizer, multisampling, depthStencil, colorBlending, dynamicState, m_pipelineLayout, m_renderPass);

        // Wire frame rendering pipeline
        if (wireframe) {
            rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
            rasterizer.lineWidth = 1.0f;
            pipelines.wireframe= renderer::createGraphicsPipeline(m_device->getDevice(), m_pipelineCache, shaderStages_mesh, vertexInputState, inputAssembly, viewport, rasterizer, multisampling, depthStencil, colorBlending, dynamicState, m_pipelineLayout, m_renderPass);            
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
            vkCmdBindDescriptorSets(m_device->m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
            vkCmdBindPipeline(m_device->m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, wireframe ? pipelines.wireframe : pipelines.solid);
            glTFModel.draw(m_device->m_commandBuffers[i], m_pipelineLayout);
            vkCmdEndRenderPass(m_device->m_commandBuffers[i]);
            vkEndCommandBuffer(m_device->m_commandBuffers[i]);
        }
    }

    void mainLoop() {

        while (!m_window.getWindowShouldClose()) {
            // Poll for user input.
            glfwPollEvents();
            render();
        }
        vkDeviceWaitIdle(m_device->getDevice());
    }

    void render() {
        auto tStart = std::chrono::high_resolution_clock::now();
        drawFrame();
        frameCounter++;
        auto tEnd = std::chrono::high_resolution_clock::now();
        auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
        frameTimer = (float)tDiff / 1000.0f;
    
       float fpsTimer = (float)(std::chrono::duration<double, std::milli>(tEnd - lastTimestamp).count());
       if (fpsTimer > 1000.0f)
       {
           lastFPS = static_cast<uint32_t>((float)frameCounter * (1000.0f / fpsTimer));
       }
       frameCounter = 0;
       lastTimestamp = tEnd;
       
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
        glTFModel.updateAnimation(frameTimer);
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

    void cleanup() {
        vkDestroyDescriptorSetLayout(m_device->getDevice(), descriptorSetLayouts.matrices, nullptr);
        vkDestroyDescriptorSetLayout(m_device->getDevice(), descriptorSetLayouts.textures, nullptr);
        vkDestroyDescriptorSetLayout(m_device->getDevice(), descriptorSetLayouts.jointMatrices, nullptr);
        vkDestroyBuffer(m_device->getDevice(), shaderData.buffer.resource, nullptr);
        for (auto framebuffer : m_framebuffers) {
            vkDestroyFramebuffer(m_device->getDevice(), framebuffer, nullptr);
        }
        vkDestroyPipelineLayout(m_device->getDevice(), m_pipelineLayout, nullptr);
        vkDestroyRenderPass(m_device->getDevice(), m_renderPass, nullptr);

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
