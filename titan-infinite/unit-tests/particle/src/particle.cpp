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
#include "pch.h"
#include "gui.h"
#include "app.h"
#include <random>

#define PARTICLE_COUNT 256 * 1024
#define PARTICLE_VERTEX_BUFFER_BIND_ID 0

class Test_Particle : public App {
public:
    Test_Particle() {
        vkHelper::addDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }
    ~Test_Particle() {
        delete m_camera;
        delete m_device;
        m_window->destroy();
        delete m_window;
    }

    void getEnabledFeatures() override {

    }

    bool init() override { 
        initResource();
        return true;
    }

    void update(float deltaTime) override {
        
    }

    void run() override {
        int64_t lastCounter = getUSec();
        while (!m_window->getWindowShouldClose()) {
            int64_t counter = getUSec();
            float deltaTime = counterToSecondsElapsed(lastCounter, counter);
            lastCounter = counter;
            glfwPollEvents();
            update(deltaTime);
            render();
        }
        vkDeviceWaitIdle(m_device->getDevice());
        destroy();
    }

private:
    Window* m_window;
    Device* m_device;
    Camera* m_camera;

    float timer = 0.0f;
    float animStart = 20.0f;

    VkPipelineLayout m_pipelineLayout;
    VkPipeline m_pipeline;

    struct DescriptorSetLayouts
    {
        VkDescriptorSetLayout particle;
    } descriptorSetLayouts;

    VkDescriptorSet m_descriptorSet;
    VkSampler m_defaultSampler;

    // Mesuring frame time
    float frameTimer = 0.0f;
    uint32_t frameCounter = 0;
    uint32_t lastFPS = 0;
    std::chrono::time_point<std::chrono::high_resolution_clock> lastTimestamp;

    // Values show on UI
    Gui* gui;

    // Particle
    TextureObject particle;
    TextureObject gradient;
    VkPipelineVertexInputStateCreateInfo inputState;
    std::vector<VkVertexInputBindingDescription> bindingDescriptions;
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

    // Resources for the compute part of the example
    struct {
        uint32_t queueFamilyIndex;					
        Buffer storageBuffer;					    
        Buffer uniformBuffer;					    
        VkQueue queue;								
        VkCommandPool commandPool;					
        VkCommandBuffer commandBuffer;				
        VkSemaphore semaphore;                      
        VkDescriptorSetLayout descriptorSetLayout;	
        VkDescriptorSet descriptorSet;				
        VkPipelineLayout pipelineLayout;			
        VkPipeline pipeline;						
        struct computeUBO {							
            float deltaT;							
            float destX;							
            float destY;							
            int32_t particleCount = PARTICLE_COUNT;
        } ubo;
    } compute;

    uint32_t graphicsFamilyIndex;

    // SSBO
    struct Particle {
        glm::vec2 pos;								// Particle position
        glm::vec2 vel;								// Particle velocity
        glm::vec4 gradientPos;						// Texture coordinates for the gradient ramp map
    };
    

    void initResource()
    {
        // Camera
        m_camera = new Camera();
        m_camera->fov = 45.0f;
        m_camera->type = Camera::CameraType::lookat;
        m_camera->setPerspective(45.0f, (float)WIDTH / (float)HEIGHT, 0.1f, 1000.0f);
        m_camera->rotationSpeed = 0.25f;
        m_camera->movementSpeed = 1.0f;
        m_camera->setPosition({ 0.0f, 0.3f, 1.0f });
        m_camera->setRotation({ 0.0f, 0.0f, 0.0f });

        // glfw window
        m_window = new Window();
        m_window->setCamera(m_camera);
        m_window->create(WIDTH, HEIGHT);

        std::function<void()> getfeatures = [&]() { getEnabledFeatures(); };

        // Physical, Logical Device and Surface
        m_device = new Device();
        m_device->create(m_window, vkHelper::getInstanceExtensions(), vkHelper::getDeviceExtensions(), getfeatures);

        graphicsFamilyIndex = vkHelper::findQueueFamilies(m_device->getPhysicalDevice(), m_device->getSurface()).graphicsFamily.value();
        compute.queueFamilyIndex = vkHelper::findQueueFamilies(m_device->getPhysicalDevice(), m_device->getSurface()).computeFamily.value();
        // Gui
        gui = new Gui();
        gui->init(m_device);

        loadAssets();
        initStorageBuffers();
        initUniformBuffers();
        initDescriptorPool();
        initDescriptorSetLayout();
        initDescriptorSet();
        initPipelines();
        initCompute();
        buildCommandBuffers();
    }

    void loadAssets() {
        m_defaultSampler = texture::createSampler(
            m_device->getDevice(),
            VK_FILTER_LINEAR,
            VK_FILTER_LINEAR,
            VK_SAMPLER_MIPMAP_MODE_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_REPEAT,
            VK_SAMPLER_ADDRESS_MODE_REPEAT,
            VK_SAMPLER_ADDRESS_MODE_REPEAT,
            0.0,
            VK_TRUE,
            16.0f,
            VK_FALSE,
            VK_COMPARE_OP_ALWAYS,
            0.0,
            FLT_MAX,
            VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
            VK_FALSE);

        //emptyTexture = texture::loadTexture("../../data/textures/empty.jpg", VK_FORMAT_R8G8B8A8_UNORM, m_device, 4);

        particle = texture::loadTexture("../../data/textures/particle01_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, m_device, 4);
        gradient = texture::loadTexture("../../data/textures/particle_gradient_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, m_device, 4);
    }

    void initCompute()
    {
        // Create a compute capable device queue
        // The VulkanDevice::createLogicalDevice functions finds a compute capable queue and prefers queue families that only support compute
        // Depending on the implementation this may result in different queue family indices for graphics and computes,
        // requiring proper synchronization (see the memory and pipeline barriers)
        vkGetDeviceQueue(m_device->getDevice(), compute.queueFamilyIndex, 0, &compute.queue);

        // Create compute pipeline
        // Compute pipelines are created separate from graphics pipelines even if they use the same queue (family index)

        // Particle position storage buffer
        {
            std::vector<DescriptorSetLayoutBinding> computeLayoutBindings = {
                { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
                { 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
            };
            compute.descriptorSetLayout = m_device->createDescriptorSetLayout(m_device->getDevice(), { computeLayoutBindings });
        }
        compute.pipelineLayout = m_device->createPipelineLayout(m_device->getDevice(), { compute.descriptorSetLayout }, {});

        // Compute descriptor set
        {
            compute.descriptorSet = m_device->createDescriptorSet(m_device->getDevice(), m_device->getDescriptorPool(), compute.descriptorSetLayout);
            std::array<VkWriteDescriptorSet, 2> writeDescriptorSets{};

            writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writeDescriptorSets[0].descriptorCount = 1;
            writeDescriptorSets[0].dstSet = compute.descriptorSet;
            writeDescriptorSets[0].dstBinding = 0;
            writeDescriptorSets[0].pBufferInfo = &compute.storageBuffer.descriptor;

            writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writeDescriptorSets[1].descriptorCount = 1;
            writeDescriptorSets[1].dstSet = compute.descriptorSet;
            writeDescriptorSets[1].dstBinding = 1;
            writeDescriptorSets[1].pBufferInfo = &compute.uniformBuffer.descriptor;

            vkUpdateDescriptorSets(m_device->getDevice(), static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
        }

        // Create pipeline
        compute.pipeline = m_device->createComputePipeline(m_device->getDevice(), "../../data/shaders/particle.comp.spv", compute.pipelineLayout);

        // Separate command pool as queue family for compute
        VkCommandPoolCreateInfo cmdPoolInfo{};
        cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmdPoolInfo.queueFamilyIndex = compute.queueFamilyIndex;
        cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        VK_CHECK(vkCreateCommandPool(m_device->getDevice(), &cmdPoolInfo, nullptr, &compute.commandPool));

        // Create a command buffer for compute operations
        compute.commandBuffer = m_device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, compute.commandPool);

        // Semaphore for compute & graphics sync
        VkSemaphoreCreateInfo semaphoreCreateInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VK_CHECK(vkCreateSemaphore(m_device->getDevice(), &semaphoreCreateInfo, nullptr, &compute.semaphore));

        // Signal the semaphore
        VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &compute.semaphore;
        VK_CHECK(vkQueueSubmit(m_device->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE));
        VK_CHECK(vkQueueWaitIdle(m_device->getGraphicsQueue()));

        // Build a single command buffer containing the compute dispatch commands
        buildComputeCommandBuffer();

        // If graphics and compute queue family indices differ, acquire and immediately release the storage buffer, so that the initial acquire from the graphics command buffers are matched up properly
        if (graphicsFamilyIndex != compute.queueFamilyIndex)
        {
            // Create a transient command buffer for setting up the initial buffer transfer state
            VkCommandBuffer transferCmd = m_device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, compute.commandPool, true);

            VkBufferMemoryBarrier acquire_buffer_barrier =
            {
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                nullptr,
                0,
                VK_ACCESS_SHADER_WRITE_BIT,
                graphicsFamilyIndex,
                compute.queueFamilyIndex,
                compute.storageBuffer.buffer,
                0,
                compute.storageBuffer.bufferSize
            };
            vkCmdPipelineBarrier(
                transferCmd,
                VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                0, nullptr,
                1, &acquire_buffer_barrier,
                0, nullptr);

            VkBufferMemoryBarrier release_buffer_barrier =
            {
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                nullptr,
                VK_ACCESS_SHADER_WRITE_BIT,
                0,
                compute.queueFamilyIndex,
                graphicsFamilyIndex,
                compute.storageBuffer.buffer,
                0,
                compute.storageBuffer.bufferSize
            };
            vkCmdPipelineBarrier(
                transferCmd,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                0,
                0, nullptr,
                1, &release_buffer_barrier,
                0, nullptr);

            m_device->flushCommandBuffer(transferCmd, compute.queue, compute.commandPool);
        }
    }

    void buildComputeCommandBuffer()
    {
        VkCommandBufferBeginInfo cmdBufInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };

        VK_CHECK(vkBeginCommandBuffer(compute.commandBuffer, &cmdBufInfo));

        // Compute particle movement

        // Add memory barrier to ensure that the (graphics) vertex shader has fetched attributes before compute starts to write to the buffer
        if (graphicsFamilyIndex!= compute.queueFamilyIndex)
        {
            VkBufferMemoryBarrier buffer_barrier =
            {
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                nullptr,
                0,
                VK_ACCESS_SHADER_WRITE_BIT,
                graphicsFamilyIndex,
                compute.queueFamilyIndex,
                compute.storageBuffer.buffer,
                0,
                compute.storageBuffer.bufferSize
            };

            vkCmdPipelineBarrier(
                compute.commandBuffer,
                VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                0, nullptr,
                1, &buffer_barrier,
                0, nullptr);
        }

        // Dispatch the compute job
        vkCmdBindPipeline(compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute.pipeline);
        vkCmdBindDescriptorSets(compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute.pipelineLayout, 0, 1, &compute.descriptorSet, 0, 0);
        vkCmdDispatch(compute.commandBuffer, PARTICLE_COUNT / 256, 1, 1);

        // Add barrier to ensure that compute shader has finished writing to the buffer
        // Without this the (rendering) vertex shader may display incomplete results (partial data from last frame)
        if (graphicsFamilyIndex != compute.queueFamilyIndex)
        {
            VkBufferMemoryBarrier buffer_barrier =
            {
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                nullptr,
                VK_ACCESS_SHADER_WRITE_BIT,
                0,
                compute.queueFamilyIndex,
                graphicsFamilyIndex,
                compute.storageBuffer.buffer,
                0,
                compute.storageBuffer.bufferSize
            };
            vkCmdPipelineBarrier(
                compute.commandBuffer,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                0,
                0, nullptr,
                1, &buffer_barrier,
                0, nullptr);
        }
        vkEndCommandBuffer(compute.commandBuffer);
    }

    // Setup and fill the compute shader storage buffers containing the particles
    void initStorageBuffers()
    {
        std::default_random_engine rndEngine((unsigned)time(nullptr));
        std::uniform_real_distribution<float> rndDist(-1.0f, 1.0f);

        // Initial particle positions
        std::vector<Particle> particleBuffer(PARTICLE_COUNT);
        for (auto& particle : particleBuffer) {
            particle.pos = glm::vec2(rndDist(rndEngine), rndDist(rndEngine));
            particle.vel = glm::vec2(0.0f);
            particle.gradientPos.x = particle.pos.x / 2.0f;
        }

        VkDeviceSize storageBufferSize = particleBuffer.size() * sizeof(Particle);

        // Staging
        Buffer stagingBuffer;

        stagingBuffer = buffer::createBuffer(
            m_device,
            storageBufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            particleBuffer.data()
        );

        compute.storageBuffer = buffer::createBuffer(
            m_device,
            storageBufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            particleBuffer.data()
        );

        // Copy from staging buffer to storage buffer
        VkCommandBuffer copyCmd = m_device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, m_device->getCommandPool(), true);
        VkBufferCopy copyRegion = {};
        copyRegion.size = storageBufferSize;
        vkCmdCopyBuffer(copyCmd, stagingBuffer.buffer, compute.storageBuffer.buffer, 1, &copyRegion);
        // Execute a transfer barrier to the compute queue, if necessary
        if (graphicsFamilyIndex!= compute.queueFamilyIndex)
        {
            VkBufferMemoryBarrier buffer_barrier =
            {
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                nullptr,
                VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
                0,
                graphicsFamilyIndex,
                compute.queueFamilyIndex,
                compute.storageBuffer.buffer,
                0,
                compute.storageBuffer.bufferSize
            };

            vkCmdPipelineBarrier(
                copyCmd,
                VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                0, nullptr,
                1, &buffer_barrier,
                0, nullptr);
        }
        m_device->flushCommandBuffer(copyCmd, m_device->getGraphicsQueue(), true);

        stagingBuffer.destroy();
    }

    void initUniformBuffers() {

        compute.uniformBuffer = buffer::createBuffer(
            m_device,
            sizeof(compute.ubo),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        compute.uniformBuffer.map();
        updateUniformBuffer();
    }

    void updateUniformBuffer() {

        compute.ubo.deltaT = frameTimer * 2.5f;

        compute.ubo.destX = sin(glm::radians(timer * 360.0f)) * 0.75f;
        compute.ubo.destY = 0.0f;

        //auto cursorPos = m_window->getCurCursorPos();
        //float normalizedMx = (cursorPos.x - static_cast<float>(WIDTH/ 2)) / static_cast<float>(WIDTH / 2);
        //float normalizedMy = (cursorPos.y - static_cast<float>(HEIGHT / 2)) / static_cast<float>(HEIGHT/ 2);
        //compute.ubo.destX = normalizedMx;
        //compute.ubo.destY = normalizedMy;

        memcpy(compute.uniformBuffer.mapped, &compute.ubo, sizeof(compute.ubo));
    }

    void initDescriptorPool()
    {
        std::vector<VkDescriptorPoolSize> poolSizes = {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2}
        };

        m_device->m_descriptorPool = m_device->createDescriptorPool(
            m_device->getDevice(),
            poolSizes,
            2
        );
    }

    void initDescriptorSetLayout() {
        // color
        {
            std::vector<DescriptorSetLayoutBinding> particleLayoutBindings = {
                { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
                { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
            };
            descriptorSetLayouts.particle = m_device->createDescriptorSetLayout(m_device->getDevice(), { particleLayoutBindings });
        }

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange.size = sizeof(glm::vec2);
        pushConstantRange.offset = 0;

        m_pipelineLayout = m_device->createPipelineLayout(m_device->getDevice(), { descriptorSetLayouts.particle }, { pushConstantRange });
    }

    void initDescriptorSet()
    {
        m_descriptorSet = m_device->createDescriptorSet(m_device->getDevice(), m_device->getDescriptorPool(), descriptorSetLayouts.particle);
        std::array<VkWriteDescriptorSet, 2> writeDescriptorSets{};

        VkDescriptorImageInfo particle_descriptor = particle.getDescriptorImageInfo();
        writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeDescriptorSets[0].descriptorCount = 1;
        writeDescriptorSets[0].dstSet = m_descriptorSet;
        writeDescriptorSets[0].dstBinding = 0;
        writeDescriptorSets[0].pImageInfo = &particle_descriptor;

        VkDescriptorImageInfo gradient_descriptor = gradient.getDescriptorImageInfo();
        writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeDescriptorSets[1].descriptorCount = 1;
        writeDescriptorSets[1].dstSet = m_descriptorSet;
        writeDescriptorSets[1].dstBinding = 1;
        writeDescriptorSets[1].pImageInfo = &gradient_descriptor;

        vkUpdateDescriptorSets(m_device->getDevice(), static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
        
    }

    void initPipelines()
    {
        VertexInputState vertexInputState = {
            {
                { PARTICLE_VERTEX_BUFFER_BIND_ID, sizeof(Particle), VK_VERTEX_INPUT_RATE_VERTEX }
            },
            {
                {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Particle, pos)},
                {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Particle, gradientPos)},
            }
        };

        InputAssemblyState inputAssembly{};
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        ViewportState viewport{};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = m_device->getSwapChainExtent().width;
        viewport.height = m_device->getSwapChainExtent().height;

        RasterizationState rasterizer{};
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE;;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        MultisampleState multisampling{};
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        DepthStencilState depthStencil{};
        depthStencil.depthTestEnable = VK_FALSE;
        depthStencil.depthWriteEnable = VK_FALSE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.front = depthStencil.back;
        depthStencil.back.compareOp = VK_COMPARE_OP_ALWAYS;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;

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
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());;

        // Solid rendering pipeline
        std::vector<ShaderStage> shaderStages_particle = m_device->createShader(m_device->getDevice(), "../../data/shaders/particle.vert.spv", "../../data/shaders/particle.frag.spv");
        m_pipeline = m_device->createGraphicsPipeline(m_device->getDevice(), m_device->getPipelineCache(), shaderStages_particle, vertexInputState, inputAssembly, viewport, rasterizer, multisampling, depthStencil, colorBlending, dynamicState, m_pipelineLayout, m_device->getRenderPass());

        for (auto shaderStage : shaderStages_particle)
            vkDestroyShaderModule(m_device->getDevice(), shaderStage.module, nullptr);
    }

    void buildCommandBuffers()
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.pInheritanceInfo = nullptr;

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_device->getRenderPass();
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = m_device->getSwapChainExtent();

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = { 0.2f, 0.2f, 0.2f, 1.0f };
        clearValues[1].depthStencil = { 1.0f, 0 };

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        for (size_t i = 0; i < m_device->getCommandBuffers().size(); ++i)
        {
            renderPassInfo.framebuffer = m_device->getFramebuffers()[i];
            VkDeviceSize offsets[1] = { 0 };

            VkCommandBuffer currentCB = m_device->m_commandBuffers[i];
            if (vkBeginCommandBuffer(currentCB, &beginInfo) != VK_SUCCESS) {
                throw std::runtime_error("failed to begin recording command buffer!");
            }

            // Acquire barrier
            if (graphicsFamilyIndex != compute.queueFamilyIndex)
            {
                VkBufferMemoryBarrier buffer_barrier =
                {
                    VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                    nullptr,
                    0,
                    VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
                    compute.queueFamilyIndex,
                    graphicsFamilyIndex,
                    compute.storageBuffer.buffer,
                    0,
                    compute.storageBuffer.bufferSize
                };

                vkCmdPipelineBarrier(
                    currentCB,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                    0,
                    0, nullptr,
                    1, &buffer_barrier,
                    0, nullptr);
            }

            vkCmdBeginRenderPass(currentCB, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            VkViewport viewport{};
            viewport.width = (float)WIDTH;
            viewport.height = (float)HEIGHT;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            viewport.x = 0;
            viewport.y = 0;
            //viewport.y = viewport.height;
            vkCmdSetViewport(currentCB, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.extent = { WIDTH, HEIGHT };
            vkCmdSetScissor(currentCB, 0, 1, &scissor);

            {
                vkCmdBindPipeline(currentCB, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
                vkCmdBindDescriptorSets(currentCB, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, NULL);

                glm::vec2 screendim = glm::vec2((float)WIDTH, (float)HEIGHT);
                vkCmdPushConstants(
                    currentCB,
                    m_pipelineLayout,
                    VK_SHADER_STAGE_VERTEX_BIT,
                    0,
                    sizeof(glm::vec2),
                    &screendim);
                vkCmdBindVertexBuffers(currentCB, PARTICLE_VERTEX_BUFFER_BIND_ID, 1, &compute.storageBuffer.buffer, offsets);
                vkCmdDraw(currentCB, PARTICLE_COUNT, 1, 0, 0);
            }

            //auto update_gui = std::bind(&Test_Particle::updateGUI, this);
            vkCmdEndRenderPass(currentCB);
            
            // Release barrier
            if (graphicsFamilyIndex != compute.queueFamilyIndex)
            {
                VkBufferMemoryBarrier buffer_barrier =
                {
                    VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                    nullptr,
                    VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
                    0,
                    graphicsFamilyIndex,
                    compute.queueFamilyIndex,
                    compute.storageBuffer.buffer,
                    0,
                    compute.storageBuffer.bufferSize
                };

                vkCmdPipelineBarrier(
                    currentCB,
                    VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    0,
                    0, nullptr,
                    1, &buffer_barrier,
                    0, nullptr);
            }
            VK_CHECK(vkEndCommandBuffer(currentCB));
        }
    }

    void render() override {
        auto tStart = std::chrono::high_resolution_clock::now();

        drawFrame();
        frameCounter++;
        auto tEnd = std::chrono::high_resolution_clock::now();
        auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
        frameTimer = (float)tDiff / 1000.0f;

        // particle
        if (animStart > 0.0f)
        {
            animStart -= frameTimer * 5.0f;
        }
        else if (animStart <= 0.0f)
        {
            timer += frameTimer * 0.04f;
            if (timer > 1.f)
                timer = 0.f;
        }

        m_camera->update(frameTimer);
        float fpsTimer = (float)(std::chrono::duration<double, std::milli>(tEnd - lastTimestamp).count());
        if (fpsTimer > 1000.0f)
        {
            lastFPS = static_cast<uint32_t>((float)frameCounter * (1000.0f / fpsTimer));
        }
        frameCounter = 0;
        lastTimestamp = tEnd;
    }

    void drawFrame() {
        vkWaitForFences(m_device->getDevice(), 1, &m_device->m_waitFences[m_device->getCurrentFrame()], VK_TRUE, UINT64_MAX);
        vkResetFences(m_device->getDevice(), 1, &m_device->m_waitFences[m_device->getCurrentFrame()]);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(m_device->getDevice(), m_device->getSwapChain(), UINT64_MAX, m_device->m_imageAvailableSemaphores[m_device->getCurrentFrame()], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            return;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        if (m_device->m_imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
            vkWaitForFences(m_device->getDevice(), 1, &m_device->m_imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        }
        m_device->m_imagesInFlight[imageIndex] = m_device->m_waitFences[m_device->getCurrentFrame()];

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = { m_device->m_imageAvailableSemaphores[m_device->getCurrentFrame()] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_device->m_commandBuffers[imageIndex];

        VkSemaphore signalSemaphores[] = { m_device->m_renderFinishedSemaphores[m_device->getCurrentFrame()] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        updateUniformBuffer();
        m_device->submitCommandBuffer(m_device->getGraphicsQueue(), &submitInfo, m_device->m_waitFences[m_device->getCurrentFrame()]);

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = { m_device->getSwapChain() };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(m_device->getPresentQueue(), &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_window->getFramebufferResized()) {
            m_window->setFramebufferResized(false);
        }
        else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }
        m_device->m_currentFrame = (m_device->m_currentFrame + 1) % 2;
    }

    void destroy() {
        gui->destroy();
        vkDestroySampler(m_device->getDevice(), m_defaultSampler, nullptr);
        vkDestroyDescriptorSetLayout(m_device->getDevice(), descriptorSetLayouts.particle, nullptr);

        vkDestroyPipeline(m_device->getDevice(), m_pipeline, nullptr);
        vkDestroyPipelineLayout(m_device->getDevice(), m_pipelineLayout, nullptr);
    }

    void updateGUI() {
        ImGui::Begin("Scene Settings");
        ImGui::End();
    }
};

App* create_application()
{
    return new Test_Particle();
}