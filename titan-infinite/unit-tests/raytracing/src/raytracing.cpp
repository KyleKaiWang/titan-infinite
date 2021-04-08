/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2021 Kyle Wang
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

class Test_RayTracing : public App {
public:
    PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
    PFN_vkBuildAccelerationStructuresKHR vkBuildAccelerationStructuresKHR;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;

    Test_RayTracing() {
        // Add extension
        {
            // Instance
            vkHelper::addInstanceExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
            vkHelper::addInstanceExtension(VK_KHR_DEVICE_GROUP_CREATION_EXTENSION_NAME);

            // Swap chain
            vkHelper::addDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
            vkHelper::addDeviceExtension(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
            vkHelper::addDeviceExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);

            // Ray tracing related extensions required by this sample
            vkHelper::addDeviceExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
            vkHelper::addDeviceExtension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);

            // Required by VK_KHR_acceleration_structure
            vkHelper::addDeviceExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
            vkHelper::addDeviceExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
            vkHelper::addDeviceExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
            vkHelper::addDeviceExtension("VK_KHR_ray_query");
            
            // Required for VK_KHR_ray_tracing_pipeline
            vkHelper::addDeviceExtension(VK_KHR_SPIRV_1_4_EXTENSION_NAME);

            // Required by VK_KHR_spirv_1_4
            vkHelper::addDeviceExtension(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);

            vkHelper::addDeviceExtension(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
            vkHelper::addDeviceExtension(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
        }
    }
    ~Test_RayTracing() {
        delete m_camera;
        delete m_device;
        m_window->destroy();
        delete m_window;
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

    void getEnabledFeatures() override {

        // These are passed to device creation via a pNext structure chain
        enabled_buffer_device_addres_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
        enabled_buffer_device_addres_features.bufferDeviceAddress = VK_TRUE;

        enabled_ray_tracing_pipeline_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
        enabled_ray_tracing_pipeline_features.rayTracingPipeline = VK_TRUE;
        enabled_ray_tracing_pipeline_features.pNext = &enabled_buffer_device_addres_features;

        enabled_acceleration_structure_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        enabled_acceleration_structure_features.accelerationStructure = VK_TRUE;
        enabled_acceleration_structure_features.pNext = &enabled_ray_tracing_pipeline_features;

        m_device->m_deviceCreatepNextChain = &enabled_acceleration_structure_features;
    }

private:
    Window* m_window;
    Device* m_device;
    Camera* m_camera;

    struct DescriptorSets {
        VkDescriptorSet scene;
        VkDescriptorSet debug;
    };

    VkPipelineLayout m_pipelineLayout;

    // glTF
    vkglTF::VulkanglTFModel meshModel;

    struct Pipelines
    {
        VkPipeline solid;
        VkPipeline enable_wireframe = VK_NULL_HANDLE;
    } pipelines;

    struct DescriptorSetLayouts
    {
        VkDescriptorSetLayout scene;
        VkDescriptorSetLayout materials;
        VkDescriptorSetLayout node;
    } descriptorSetLayouts;

    struct UniformBufferSet {
        Buffer scene;
        Buffer params;
        Buffer debug;
    };

    struct UBOMatrices {
        glm::mat4 projection = glm::mat4(1.0f);
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 view = glm::mat4(1.0f);
        glm::vec3 camPos = glm::vec3(0.0f);
    }shaderValuesScene, shaderValuesDebug;

    std::vector<DescriptorSets> descriptorSets;
    std::vector<UniformBufferSet> uniformBuffers;

    struct shaderValuesParams {
        glm::vec4 lightDir = glm::vec4(10.0f, 10.0f, 10.0f, 1.0f);
        float exposure = 4.5f;
        float gamma = 2.2f;
        float prefilteredCubeMipLevels = 1.0f;
        float scaleIBLAmbient = 1.0f;
        float debugViewInputs = 0;
        float debugViewEquation = 0;
        float enableDebugBones = 0.0f;
        glm::vec3 camPos = glm::vec3(10.0f, 10.0f, 10.0f);
    } shaderValuesParams;

    enum PBRWorkflows { PBR_WORKFLOW_METALLIC_ROUGHNESS = 0, PBR_WORKFLOW_SPECULAR_GLOSINESS = 1 };

    struct PushConstBlockMaterial {
        glm::vec4 baseColorFactor;
        glm::vec4 emissiveFactor;
        glm::vec4 diffuseFactor;
        glm::vec4 specularFactor;
        float workflow;
        int colorTextureSet;
        int PhysicalDescriptorTextureSet;
        int normalTextureSet;
        int occlusionTextureSet;
        int emissiveTextureSet;
        float metallicFactor;
        float roughnessFactor;
        float alphaMask;
        float alphaMaskCutoff;
    } pushConstBlockMaterial;

    TextureObject emptyTexture;
    //TextureObject textureCube;
    VkSampler m_defaultSampler;

    struct SpecularFilterPushConstants
    {
        uint32_t level = 1;
        float roughness = 1.0f;
    };

    // Mesuring frame time
    float frameTimer = 0.0f;
    uint32_t frameCounter = 0;
    uint32_t lastFPS = 0;
    std::chrono::time_point<std::chrono::high_resolution_clock> lastTimestamp;

    //Skybox
    Skybox m_skybox;

    // Animation 
    int32_t animationIndex = 0;
    float animationTimer = 0.0f;
    float animationSpeed = 1.0f;

    // Values show on UI
    Gui* gui;
    bool enable_wireframe = false;
    bool enable_animate = false;
    bool enable_slerp = true;
    bool enable_debug_joints = false;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR  ray_tracing_pipeline_properties{};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features{};

    VkPhysicalDeviceBufferDeviceAddressFeatures enabled_buffer_device_addres_features{};
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR enabled_ray_tracing_pipeline_features{};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR enabled_acceleration_structure_features{};

    Buffer vertex_buffer;
    Buffer index_buffer;

    struct UniformData {
        glm::mat4 view_inverse;
        glm::mat4 proj_inverse;
    };

    UniformData uniform_data;
    Buffer ubo;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shader_groups{};
    Buffer raygen_shader_binding_table;
    Buffer miss_shader_binding_table;
    Buffer hit_shader_binding_table;

    struct StorageImage
    {
        VkDeviceMemory memory;
        VkImage        image = VK_NULL_HANDLE;
        VkImageView    view;
        VkFormat       format;
        uint32_t       width;
        uint32_t       height;
    };

    StorageImage storage_image;

    struct ScratchBuffer
    {
        uint64_t       device_address;
        VkBuffer       handle;
        VkDeviceMemory memory;
    };

    struct AccelerationStructure
    {
        VkAccelerationStructureKHR handle;
        uint64_t                   device_address;
        std::unique_ptr<Buffer>    buffer;
    };

    AccelerationStructure bottom_level_acceleration_structure;
    AccelerationStructure top_level_acceleration_structure;

    VkPipeline            pipeline;
    VkPipelineLayout      pipeline_layout;
    VkDescriptorSet       descriptor_set;
    VkDescriptorSetLayout descriptor_set_layout;


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

        // Ray Tracing init
        {
            // Get the ray tracing pipeline properties, which we'll need later on in the sample
            ray_tracing_pipeline_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
            VkPhysicalDeviceProperties2 device_properties{};
            device_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            device_properties.pNext = &ray_tracing_pipeline_properties;
            vkGetPhysicalDeviceProperties2(m_device->getPhysicalDevice(), &device_properties);

            // Get the acceleration structure features, which we'll need later on in the sample
            acceleration_structure_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
            VkPhysicalDeviceFeatures2 device_features{};
            device_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            device_features.pNext = &acceleration_structure_features;
            vkGetPhysicalDeviceFeatures2(m_device->getPhysicalDevice(), &device_features);

            // Get the function pointers required for ray tracing
            vkGetBufferDeviceAddressKHR = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(vkGetDeviceProcAddr(m_device->getDevice(), "vkGetBufferDeviceAddressKHR"));
            vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(m_device->getDevice(), "vkCmdBuildAccelerationStructuresKHR"));
            vkBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(m_device->getDevice(), "vkBuildAccelerationStructuresKHR"));
            vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(m_device->getDevice(), "vkCreateAccelerationStructureKHR"));
            vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(m_device->getDevice(), "vkDestroyAccelerationStructureKHR"));
            vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(m_device->getDevice(), "vkGetAccelerationStructureBuildSizesKHR"));
            vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(m_device->getDevice(), "vkGetAccelerationStructureDeviceAddressKHR"));
            vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(m_device->getDevice(), "vkCmdTraceRaysKHR"));
            vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(m_device->getDevice(), "vkGetRayTracingShaderGroupHandlesKHR"));
            vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(m_device->getDevice(), "vkCreateRayTracingPipelinesKHR"));

            // Create the acceleration structures used to render the ray traced scene
            initStorageImage();
            initUniformBuffers();
            initButtomLevelAccelerationStructure();
            initTopLevelAccelerationStructure();
            initDescriptorSets();
            buildCommandBuffers();
        }


        // Gui
        //gui = new Gui();
        //gui->init(m_device);
        //
        //uniformBuffers.resize(m_device->getSwapChainimages().size());
        //descriptorSets.resize(m_device->getSwapChainimages().size());

        //loadAssets();
        //initUniformBuffers();
        //initDescriptorPool();

        // Skybox
        //m_skybox.create(m_device, "../../data/models/glTF-Embedded/cube.gltf", "../../data/textures/cubemap_yokohama_rgba.ktx");
        //m_skybox.initDescriptorSet();
        //m_skybox.initPipelines(m_device->getRenderPass());
        //
        //initDescriptorSetLayout();
        //initDescriptorSet();
        //initPipelines();
        //buildCommandBuffers();
    }

    void initStorageImage()
    {
        storage_image.width = m_window->getWidth();
        storage_image.height = m_window->getHeight();
        storage_image.image = m_device->createImage(m_device->getDevice(),
            0,
            VK_IMAGE_TYPE_2D,
            VK_FORMAT_B8G8R8A8_UNORM,
            { storage_image.width,  storage_image.height, 1 },
            1,
            1,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT);

        VkMemoryRequirements memory_requirements;
        vkGetImageMemoryRequirements(m_device->getDevice(), storage_image.image, &memory_requirements);
        VkMemoryAllocateInfo memory_allocate_info{};
        memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memory_allocate_info.allocationSize = memory_requirements.size;
        memory_allocate_info.memoryTypeIndex = m_device->findMemoryType(memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK_RESULT(vkAllocateMemory(m_device->getDevice(), &memory_allocate_info, nullptr, &storage_image.memory));
        VK_CHECK_RESULT(vkBindImageMemory(m_device->getDevice(), storage_image.image, storage_image.memory, 0));

        storage_image.view = m_device->createImageView(m_device->getDevice(),
            storage_image.image,
            VK_IMAGE_VIEW_TYPE_2D,
            VK_FORMAT_B8G8R8A8_UNORM,
            { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
            { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

        VkCommandBuffer command_buffer = m_device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        
        texture::setImageLayout(
            command_buffer,
            storage_image.image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
        m_device->flushCommandBuffer(command_buffer, m_device->getGraphicsQueue());
    }

    void initButtomLevelAccelerationStructure() {
        // Setup vertices and indices for a single triangle
        struct Vertex
        {
            float pos[3];
        };
        std::vector<Vertex> vertices = {
            {{1.0f, 1.0f, 0.0f}},
            {{-1.0f, 1.0f, 0.0f}},
            {{0.0f, -1.0f, 0.0f}} };
        std::vector<uint32_t> indices = { 0, 1, 2 };

        auto vertex_buffer_size = vertices.size() * sizeof(Vertex);
        auto index_buffer_size = indices.size() * sizeof(uint32_t);

        // Note that the buffer usage flags for buffers consumed by the bottom level acceleration structure require special flags
        const VkBufferUsageFlags buffer_usage_flags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        vertex_buffer = buffer::createBuffer(
            m_device, 
            vertex_buffer_size, 
            buffer_usage_flags, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
            VK_SHARING_MODE_EXCLUSIVE, 
            vertices.data());

        index_buffer = buffer::createBuffer(
            m_device,
            index_buffer_size,
            buffer_usage_flags,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            vertices.data());

        // Setup a single transformation matrix that can be used to transform the whole geometry for a single bottom level acceleration structure
        VkTransformMatrixKHR transform_matrix = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f };

        Buffer transform_matrix_buffer = buffer::createBuffer(
            m_device,
            sizeof(transform_matrix),
            buffer_usage_flags,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            &transform_matrix);
        
        VkDeviceOrHostAddressConstKHR vertex_data_device_address{};
        VkDeviceOrHostAddressConstKHR index_data_device_address{};
        VkDeviceOrHostAddressConstKHR transform_matrix_device_address{};

        vertex_data_device_address.deviceAddress = getBufferDeviceAddress(vertex_buffer.buffer);
        index_data_device_address.deviceAddress = getBufferDeviceAddress(index_buffer.buffer);
        transform_matrix_device_address.deviceAddress = getBufferDeviceAddress(transform_matrix_buffer.buffer);

        // The bottom level acceleration structure contains one set of triangles as the input geometry
        VkAccelerationStructureGeometryKHR acceleration_structure_geometry{};
        acceleration_structure_geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        acceleration_structure_geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        acceleration_structure_geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        acceleration_structure_geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        acceleration_structure_geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        acceleration_structure_geometry.geometry.triangles.vertexData = vertex_data_device_address;
        acceleration_structure_geometry.geometry.triangles.maxVertex = 3;
        acceleration_structure_geometry.geometry.triangles.vertexStride = sizeof(Vertex);
        acceleration_structure_geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
        acceleration_structure_geometry.geometry.triangles.indexData = index_data_device_address;
        acceleration_structure_geometry.geometry.triangles.transformData = transform_matrix_device_address;

        // Get the size requirements for buffers involved in the acceleration structure build process
        VkAccelerationStructureBuildGeometryInfoKHR acceleration_structure_build_geometry_info{};
        acceleration_structure_build_geometry_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        acceleration_structure_build_geometry_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        acceleration_structure_build_geometry_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        acceleration_structure_build_geometry_info.geometryCount = 1;
        acceleration_structure_build_geometry_info.pGeometries = &acceleration_structure_geometry;

        const uint32_t primitive_count = 1;

        VkAccelerationStructureBuildSizesInfoKHR acceleration_structure_build_sizes_info{};
        acceleration_structure_build_sizes_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(
            m_device->getDevice(),
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &acceleration_structure_build_geometry_info,
            &primitive_count,
            &acceleration_structure_build_sizes_info);

        // Create a buffer to hold the acceleration structure
        createAccelerationStructureBuffer(bottom_level_acceleration_structure, acceleration_structure_build_sizes_info);

        // Create the acceleration structure
        VkAccelerationStructureCreateInfoKHR acceleration_structure_create_info{};
        acceleration_structure_create_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        acceleration_structure_create_info.buffer = bottom_level_acceleration_structure.buffer->buffer;
        acceleration_structure_create_info.size = acceleration_structure_build_sizes_info.accelerationStructureSize;
        acceleration_structure_create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        vkCreateAccelerationStructureKHR(m_device->getDevice(), &acceleration_structure_create_info, nullptr, &bottom_level_acceleration_structure.handle);

        // The actual build process starts here

        // Create a scratch buffer as a temporary storage for the acceleration structure build
        ScratchBuffer scratch_buffer = createScratchBuffer(acceleration_structure_build_sizes_info.buildScratchSize);

        VkAccelerationStructureBuildGeometryInfoKHR acceleration_build_geometry_info{};
        acceleration_build_geometry_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        acceleration_build_geometry_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        acceleration_build_geometry_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        acceleration_build_geometry_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        acceleration_build_geometry_info.dstAccelerationStructure = bottom_level_acceleration_structure.handle;
        acceleration_build_geometry_info.geometryCount = 1;
        acceleration_build_geometry_info.pGeometries = &acceleration_structure_geometry;
        acceleration_build_geometry_info.scratchData.deviceAddress = scratch_buffer.device_address;

        VkAccelerationStructureBuildRangeInfoKHR acceleration_structure_build_range_info;
        acceleration_structure_build_range_info.primitiveCount = 1;
        acceleration_structure_build_range_info.primitiveOffset = 0;
        acceleration_structure_build_range_info.firstVertex = 0;
        acceleration_structure_build_range_info.transformOffset = 0;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR*> acceleration_build_structure_range_infos = { &acceleration_structure_build_range_info };

        // Build the acceleration structure on the device via a one-time command buffer submission
        // Some implementations may support acceleration structure building on the host (VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructureHostCommands), but we prefer device builds
        VkCommandBuffer command_buffer = m_device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        vkCmdBuildAccelerationStructuresKHR(
            command_buffer,
            1,
            &acceleration_build_geometry_info,
            acceleration_build_structure_range_infos.data());
        m_device->flushCommandBuffer(command_buffer, m_device->getGraphicsQueue());

        deleteScratchBuffer(scratch_buffer);

        // Get the bottom acceleration structure's handle, which will be used during the top level acceleration build
        VkAccelerationStructureDeviceAddressInfoKHR acceleration_device_address_info{};
        acceleration_device_address_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        acceleration_device_address_info.accelerationStructure = bottom_level_acceleration_structure.handle;
        bottom_level_acceleration_structure.device_address = vkGetAccelerationStructureDeviceAddressKHR(m_device->getDevice(), &acceleration_device_address_info);

        transform_matrix_buffer.destroy();
    }

    void initTopLevelAccelerationStructure() {
        VkTransformMatrixKHR transform_matrix = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f };

        VkAccelerationStructureInstanceKHR acceleration_structure_instance{};
        acceleration_structure_instance.transform = transform_matrix;
        acceleration_structure_instance.instanceCustomIndex = 0;
        acceleration_structure_instance.mask = 0xFF;
        acceleration_structure_instance.instanceShaderBindingTableRecordOffset = 0;
        acceleration_structure_instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        acceleration_structure_instance.accelerationStructureReference = bottom_level_acceleration_structure.device_address;
        
        Buffer instances_buffer = buffer::createBuffer(
            m_device,
            sizeof(VkAccelerationStructureInstanceKHR),
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            &acceleration_structure_instance);

        VkDeviceOrHostAddressConstKHR instance_data_device_address{};
        instance_data_device_address.deviceAddress = getBufferDeviceAddress(instances_buffer.buffer);

        // The top level acceleration structure contains (bottom level) instance as the input geometry
        VkAccelerationStructureGeometryKHR acceleration_structure_geometry{};
        acceleration_structure_geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        acceleration_structure_geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        acceleration_structure_geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        acceleration_structure_geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        acceleration_structure_geometry.geometry.instances.arrayOfPointers = VK_FALSE;
        acceleration_structure_geometry.geometry.instances.data = instance_data_device_address;

        // Get the size requirements for buffers involved in the acceleration structure build process
        VkAccelerationStructureBuildGeometryInfoKHR acceleration_structure_build_geometry_info{};
        acceleration_structure_build_geometry_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        acceleration_structure_build_geometry_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        acceleration_structure_build_geometry_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        acceleration_structure_build_geometry_info.geometryCount = 1;
        acceleration_structure_build_geometry_info.pGeometries = &acceleration_structure_geometry;

        const uint32_t primitive_count = 1;

        VkAccelerationStructureBuildSizesInfoKHR acceleration_structure_build_sizes_info{};
        acceleration_structure_build_sizes_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(
            m_device->getDevice(), 
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &acceleration_structure_build_geometry_info,
            &primitive_count,
            &acceleration_structure_build_sizes_info);

        // Create a buffer to hold the acceleration structure
        createAccelerationStructureBuffer(top_level_acceleration_structure, acceleration_structure_build_sizes_info);

        // Create the acceleration structure
        VkAccelerationStructureCreateInfoKHR acceleration_structure_create_info{};
        acceleration_structure_create_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        acceleration_structure_create_info.buffer = top_level_acceleration_structure.buffer->buffer;;
        acceleration_structure_create_info.size = acceleration_structure_build_sizes_info.accelerationStructureSize;
        acceleration_structure_create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        vkCreateAccelerationStructureKHR(m_device->getDevice(), &acceleration_structure_create_info, nullptr, &top_level_acceleration_structure.handle);

        // The actual build process starts here

        // Create a scratch buffer as a temporary storage for the acceleration structure build
        ScratchBuffer scratch_buffer = createScratchBuffer(acceleration_structure_build_sizes_info.buildScratchSize);

        VkAccelerationStructureBuildGeometryInfoKHR acceleration_build_geometry_info{};
        acceleration_build_geometry_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        acceleration_build_geometry_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        acceleration_build_geometry_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        acceleration_build_geometry_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        acceleration_build_geometry_info.dstAccelerationStructure = top_level_acceleration_structure.handle;
        acceleration_build_geometry_info.geometryCount = 1;
        acceleration_build_geometry_info.pGeometries = &acceleration_structure_geometry;
        acceleration_build_geometry_info.scratchData.deviceAddress = scratch_buffer.device_address;

        VkAccelerationStructureBuildRangeInfoKHR acceleration_structure_build_range_info;
        acceleration_structure_build_range_info.primitiveCount = 1;
        acceleration_structure_build_range_info.primitiveOffset = 0;
        acceleration_structure_build_range_info.firstVertex = 0;
        acceleration_structure_build_range_info.transformOffset = 0;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR*> acceleration_build_structure_range_infos = { &acceleration_structure_build_range_info };

        // Build the acceleration structure on the device via a one-time command buffer submission
        // Some implementations may support acceleration structure building on the host (VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructureHostCommands), but we prefer device builds
        VkCommandBuffer command_buffer = m_device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        vkCmdBuildAccelerationStructuresKHR(
            command_buffer,
            1,
            &acceleration_build_geometry_info,
            acceleration_build_structure_range_infos.data());
        m_device->flushCommandBuffer(command_buffer, m_device->getGraphicsQueue());

        deleteScratchBuffer(scratch_buffer);

        // Get the top acceleration structure's handle, which will be used to setup it's descriptor
        VkAccelerationStructureDeviceAddressInfoKHR acceleration_device_address_info{};
        acceleration_device_address_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        acceleration_device_address_info.accelerationStructure = top_level_acceleration_structure.handle;
        top_level_acceleration_structure.device_address = vkGetAccelerationStructureDeviceAddressKHR(m_device->getDevice(), &acceleration_device_address_info);
    }

    void initRayTracingPipeline()
    {
        // Slot for binding top level acceleration structures to the ray generation shader
        VkDescriptorSetLayoutBinding acceleration_structure_layout_binding{};
        acceleration_structure_layout_binding.binding = 0;
        acceleration_structure_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        acceleration_structure_layout_binding.descriptorCount = 1;
        acceleration_structure_layout_binding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

        VkDescriptorSetLayoutBinding result_image_layout_binding{};
        result_image_layout_binding.binding = 1;
        result_image_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        result_image_layout_binding.descriptorCount = 1;
        result_image_layout_binding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

        VkDescriptorSetLayoutBinding uniform_buffer_binding{};
        uniform_buffer_binding.binding = 2;
        uniform_buffer_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uniform_buffer_binding.descriptorCount = 1;
        uniform_buffer_binding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

        std::vector<VkDescriptorSetLayoutBinding> bindings = {
            acceleration_structure_layout_binding,
            result_image_layout_binding,
            uniform_buffer_binding };

        VkDescriptorSetLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
        layout_info.pBindings = bindings.data();
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device->getDevice(), &layout_info, nullptr, &descriptor_set_layout));

        VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
        pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_create_info.setLayoutCount = 1;
        pipeline_layout_create_info.pSetLayouts = &descriptor_set_layout;

        VK_CHECK_RESULT(vkCreatePipelineLayout(m_device->getDevice(), &pipeline_layout_create_info, nullptr, &pipeline_layout));

        /*
            Setup ray tracing shader groups
            Each shader group points at the corresponding shader in the pipeline
        */
        std::vector<ShaderStage> shader_stages;

        // Ray generation group
        {
            shader_stages.push_back(m_device->createRayTracingShader("khr_ray_tracing_basic/raygen.rgen", VK_SHADER_STAGE_RAYGEN_BIT_KHR));
            //shader_stages.push_back(load_shader("khr_ray_tracing_basic/raygen.rgen", VK_SHADER_STAGE_RAYGEN_BIT_KHR));
            VkRayTracingShaderGroupCreateInfoKHR raygen_group_ci{};
            raygen_group_ci.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            raygen_group_ci.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
            raygen_group_ci.generalShader = static_cast<uint32_t>(shader_stages.size()) - 1;
            raygen_group_ci.closestHitShader = VK_SHADER_UNUSED_KHR;
            raygen_group_ci.anyHitShader = VK_SHADER_UNUSED_KHR;
            raygen_group_ci.intersectionShader = VK_SHADER_UNUSED_KHR;
            shader_groups.push_back(raygen_group_ci);
        }

        // Ray miss group
        {
            shader_stages.push_back(m_device->createRayTracingShader("khr_ray_tracing_basic/miss.rmiss", VK_SHADER_STAGE_MISS_BIT_KHR));
            //shader_stages.push_back(load_shader("khr_ray_tracing_basic/miss.rmiss", VK_SHADER_STAGE_MISS_BIT_KHR));
            VkRayTracingShaderGroupCreateInfoKHR miss_group_ci{};
            miss_group_ci.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            miss_group_ci.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
            miss_group_ci.generalShader = static_cast<uint32_t>(shader_stages.size()) - 1;
            miss_group_ci.closestHitShader = VK_SHADER_UNUSED_KHR;
            miss_group_ci.anyHitShader = VK_SHADER_UNUSED_KHR;
            miss_group_ci.intersectionShader = VK_SHADER_UNUSED_KHR;
            shader_groups.push_back(miss_group_ci);
        }

        // Ray closest hit group
        {
            shader_stages.push_back(m_device->createRayTracingShader("khr_ray_tracing_basic/closesthit.rchit", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR));
            //shader_stages.push_back(load_shader("khr_ray_tracing_basic/closesthit.rchit", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR));
            VkRayTracingShaderGroupCreateInfoKHR closes_hit_group_ci{};
            closes_hit_group_ci.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            closes_hit_group_ci.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
            closes_hit_group_ci.generalShader = VK_SHADER_UNUSED_KHR;
            closes_hit_group_ci.closestHitShader = static_cast<uint32_t>(shader_stages.size()) - 1;
            closes_hit_group_ci.anyHitShader = VK_SHADER_UNUSED_KHR;
            closes_hit_group_ci.intersectionShader = VK_SHADER_UNUSED_KHR;
            shader_groups.push_back(closes_hit_group_ci);
        }

        std::vector<VkPipelineShaderStageCreateInfo> pipeline_shader_stages;
        for (const ShaderStage& shaderStage : shader_stages) {
            VkPipelineShaderStageCreateInfo pipelineShaderStage{};
            pipelineShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            pipelineShaderStage.module = shaderStage.module;
            pipelineShaderStage.stage = shaderStage.stage;
            pipelineShaderStage.pName = shaderStage.pName.c_str();

            pipeline_shader_stages.push_back(pipelineShaderStage);
        }

        /*
            Create the ray tracing pipeline
        */
        VkRayTracingPipelineCreateInfoKHR raytracing_pipeline_create_info{};
        raytracing_pipeline_create_info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
        raytracing_pipeline_create_info.stageCount = static_cast<uint32_t>(shader_stages.size());
        raytracing_pipeline_create_info.pStages = pipeline_shader_stages.data();
        raytracing_pipeline_create_info.groupCount = static_cast<uint32_t>(shader_groups.size());
        raytracing_pipeline_create_info.pGroups = shader_groups.data();
        raytracing_pipeline_create_info.maxPipelineRayRecursionDepth = 1;
        raytracing_pipeline_create_info.layout = pipeline_layout;
        VK_CHECK_RESULT(vkCreateRayTracingPipelinesKHR(m_device->getDevice(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &raytracing_pipeline_create_info, nullptr, &pipeline));
    }

    void initShaderBindingTables()
    {
        const uint32_t           handle_size = ray_tracing_pipeline_properties.shaderGroupHandleSize;
        const uint32_t           handle_size_aligned = alignedSize(ray_tracing_pipeline_properties.shaderGroupHandleSize, ray_tracing_pipeline_properties.shaderGroupHandleAlignment);
        const uint32_t           handle_alignment = ray_tracing_pipeline_properties.shaderGroupHandleAlignment;
        const uint32_t           group_count = static_cast<uint32_t>(shader_groups.size());
        const uint32_t           sbt_size = group_count * handle_size_aligned;
        const VkBufferUsageFlags sbt_buffer_usafge_flags = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        const VkMemoryPropertyFlags memory_usage_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        raygen_shader_binding_table = buffer::createBuffer(m_device, handle_size, sbt_buffer_usafge_flags, memory_usage_flags, VK_SHARING_MODE_EXCLUSIVE, 0);
        miss_shader_binding_table = buffer::createBuffer(m_device, handle_size, sbt_buffer_usafge_flags, memory_usage_flags, VK_SHARING_MODE_EXCLUSIVE, 0);
        hit_shader_binding_table = buffer::createBuffer(m_device, handle_size, sbt_buffer_usafge_flags, memory_usage_flags, VK_SHARING_MODE_EXCLUSIVE, 0);

        // Raygen
        // Create binding table buffers for each shader type
        raygen_shader_binding_table.map();
        miss_shader_binding_table.map();        
        hit_shader_binding_table.map();

        // Copy the pipeline's shader handles into a host buffer
        std::vector<uint8_t> shader_handle_storage(sbt_size);
        VK_CHECK_RESULT(vkGetRayTracingShaderGroupHandlesKHR(m_device->getDevice(), pipeline, 0, group_count, sbt_size, shader_handle_storage.data()));

        // Copy the shader handles from the host buffer to the binding tables
        memcpy(raygen_shader_binding_table.mapped, shader_handle_storage.data(), handle_size);
        memcpy(miss_shader_binding_table.mapped, shader_handle_storage.data() + handle_size_aligned, handle_size);
        memcpy(hit_shader_binding_table.mapped, shader_handle_storage.data() + handle_size_aligned * 2, handle_size);
        
        //raygen_shader_binding_table.unmap();
        //miss_shader_binding_table.unmap();
        //hit_shader_binding_table.unmap();
    }

    void initDescriptorSets()
    {
        std::vector<VkDescriptorPoolSize> pool_sizes = {
            {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1} };
        m_device->m_descriptorPool = m_device->createDescriptorPool(m_device->getDevice(), pool_sizes, 1);        
        m_device->createDescriptorSets(m_device->getDevice(), m_device->getDescriptorPool(), { descriptor_set_layout });

        // Setup the descriptor for binding our top level acceleration structure to the ray tracing shaders
        VkWriteDescriptorSetAccelerationStructureKHR descriptor_acceleration_structure_info{};
        descriptor_acceleration_structure_info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        descriptor_acceleration_structure_info.accelerationStructureCount = 1;
        descriptor_acceleration_structure_info.pAccelerationStructures = &top_level_acceleration_structure.handle;

        VkWriteDescriptorSet acceleration_structure_write{};
        acceleration_structure_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        acceleration_structure_write.dstSet = descriptor_set;
        acceleration_structure_write.dstBinding = 0;
        acceleration_structure_write.descriptorCount = 1;
        acceleration_structure_write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        // The acceleration structure descriptor has to be chained via pNext
        acceleration_structure_write.pNext = &descriptor_acceleration_structure_info;

        VkDescriptorImageInfo image_descriptor{};
        image_descriptor.imageView = storage_image.view;
        image_descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        
        VkWriteDescriptorSet result_image_write{};
        result_image_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        result_image_write.dstSet = descriptor_set;
        result_image_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        result_image_write.dstBinding = 1;
        result_image_write.pImageInfo = &image_descriptor;
        
        
        VkWriteDescriptorSet uniform_buffer_write{};
        uniform_buffer_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        uniform_buffer_write.dstSet = descriptor_set;
        uniform_buffer_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uniform_buffer_write.dstBinding = 2;
        uniform_buffer_write.pBufferInfo = &ubo.descriptor;

        std::vector<VkWriteDescriptorSet> write_descriptor_sets = {
            acceleration_structure_write,
            result_image_write,
            uniform_buffer_write };
        vkUpdateDescriptorSets(m_device->getDevice(), static_cast<uint32_t>(write_descriptor_sets.size()), write_descriptor_sets.data(), 0, VK_NULL_HANDLE);
    }

    void buildCommandBuffers()
    {
        auto width = m_window->getWidth();
        auto height = m_window->getHeight();
        if (width != storage_image.width || height != storage_image.height)
        {
            // If the view port size has changed, we need to recreate the storage image
            vkDestroyImageView(m_device->getDevice(), storage_image.view, nullptr);
            vkDestroyImage(m_device->getDevice(), storage_image.image, nullptr);
            vkFreeMemory(m_device->getDevice(), storage_image.memory, nullptr);
            initStorageImage();
            // The descriptor also needs to be updated to reference the new image
            VkDescriptorImageInfo image_descriptor{};
            image_descriptor.imageView = storage_image.view;
            image_descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            
            VkWriteDescriptorSet result_image_write{};
            result_image_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            result_image_write.dstSet = descriptor_set;
            result_image_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            result_image_write.dstBinding = 1;
            result_image_write.pImageInfo = &image_descriptor;

            vkUpdateDescriptorSets(m_device->getDevice(), 1, &result_image_write, 0, VK_NULL_HANDLE);
            buildCommandBuffers();
        }

        VkCommandBufferBeginInfo command_buffer_begin_info{};
        command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        command_buffer_begin_info.pInheritanceInfo = nullptr;

        VkImageSubresourceRange subresource_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        for (int32_t i = 0; i < m_device->getCommandBuffers().size(); ++i)
        {
            VkCommandBuffer currentCB = m_device->m_commandBuffers[i];
            VK_CHECK_RESULT(vkBeginCommandBuffer(currentCB, &command_buffer_begin_info));

            /*
                Setup the strided device address regions pointing at the shader identifiers in the shader binding table
            */

            const uint32_t handle_size_aligned = alignedSize(ray_tracing_pipeline_properties.shaderGroupHandleSize, ray_tracing_pipeline_properties.shaderGroupHandleAlignment);

            VkStridedDeviceAddressRegionKHR raygen_shader_sbt_entry{};
            raygen_shader_sbt_entry.deviceAddress = getBufferDeviceAddress(raygen_shader_binding_table.buffer);
            raygen_shader_sbt_entry.stride = handle_size_aligned;
            raygen_shader_sbt_entry.size = handle_size_aligned;

            VkStridedDeviceAddressRegionKHR miss_shader_sbt_entry{};
            miss_shader_sbt_entry.deviceAddress = getBufferDeviceAddress(miss_shader_binding_table.buffer);
            miss_shader_sbt_entry.stride = handle_size_aligned;
            miss_shader_sbt_entry.size = handle_size_aligned;

            VkStridedDeviceAddressRegionKHR hit_shader_sbt_entry{};
            hit_shader_sbt_entry.deviceAddress = getBufferDeviceAddress(hit_shader_binding_table.buffer);
            hit_shader_sbt_entry.stride = handle_size_aligned;
            hit_shader_sbt_entry.size = handle_size_aligned;

            VkStridedDeviceAddressRegionKHR callable_shader_sbt_entry{};

            /*
                Dispatch the ray tracing commands
            */
            vkCmdBindPipeline(currentCB, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
            vkCmdBindDescriptorSets(currentCB, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline_layout, 0, 1, &descriptor_set, 0, 0);

            vkCmdTraceRaysKHR(
                currentCB,
                &raygen_shader_sbt_entry,
                &miss_shader_sbt_entry,
                &hit_shader_sbt_entry,
                &callable_shader_sbt_entry,
                width,
                height,
                1);

            /*
                Copy ray tracing output to swap chain image
            */

            // Prepare current swap chain image as transfer destination
            texture::setImageLayout(
                currentCB,
                m_device->getSwapChainimages()[i],
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                subresource_range
            );

            // Prepare ray tracing output image as transfer source
            texture::setImageLayout(
                currentCB,
                m_device->getSwapChainimages()[i],
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                subresource_range
            );

            VkImageCopy copy_region{};
            copy_region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            copy_region.srcOffset = { 0, 0, 0 };
            copy_region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            copy_region.dstOffset = { 0, 0, 0 };
            copy_region.extent = { width, height, 1 };
            vkCmdCopyImage(currentCB, storage_image.image, 
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                m_device->getSwapChainimages()[i], 
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
                1, 
                &copy_region
            );

            // Transition swap chain image back for presentation
            texture::setImageLayout(
                currentCB,
                m_device->getSwapChainimages()[i],
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                subresource_range
            );

            // Transition ray tracing output image back to general layout
            texture::setImageLayout(
                currentCB,
                m_device->getSwapChainimages()[i],
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                subresource_range
            );
            VK_CHECK_RESULT(vkEndCommandBuffer(currentCB));
        }
    }

    void loadAssets() {
        meshModel.loadFromFile("../../data/models/glTF-Embedded/CesiumMan.gltf", m_device, m_device->getGraphicsQueue());

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

        emptyTexture = texture::loadTexture("../../data/textures/empty.jpg", VK_FORMAT_R8G8B8A8_UNORM, m_device, 4);
    }

    void initUniformBuffers() {

        ubo = buffer::createBuffer(
            m_device,
            sizeof(uniform_data),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            &uniform_data
        );
        ubo.map();

        updateUniformBuffer();
        //for (auto& uniformBuffer : uniformBuffers) {
        //    uniformBuffer.scene = buffer::createBuffer(
        //        m_device,
        //        sizeof(shaderValuesScene),
        //        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        //        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        //    );
        //    memory::map(m_device->getDevice(), uniformBuffer.scene.memory, 0, uniformBuffer.scene.bufferSize, &uniformBuffer.scene.mapped);
        //    uniformBuffer.debug = buffer::createBuffer(
        //        m_device,
        //        sizeof(shaderValuesDebug),
        //        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        //        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        //    );
        //    memory::map(m_device->getDevice(), uniformBuffer.debug.memory, 0, uniformBuffer.debug.bufferSize, &uniformBuffer.debug.mapped);
        //    uniformBuffer.params = buffer::createBuffer(
        //        m_device,
        //        sizeof(shaderValuesParams),
        //        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        //        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        //    );
        //    memory::map(m_device->getDevice(), uniformBuffer.params.memory, 0, uniformBuffer.params.bufferSize, &uniformBuffer.params.mapped);
        //}
    }

    float ParametricBlend(float t)
    {
        float sqt = t * t;
        return sqt / (2.0f * (sqt - t) + 1.0f);
    }

    void updateUniformBuffer() {
        uniform_data.proj_inverse = glm::inverse(m_camera->matrices.perspective);
        uniform_data.view_inverse = glm::inverse(m_camera->matrices.view);
        memcpy(ubo.mapped, &uniform_data, sizeof(uniform_data));

        // Scene
        //shaderValuesScene.projection = m_camera->matrices.perspective;
        //shaderValuesScene.view = m_camera->matrices.view;
        //shaderValuesScene.camPos = m_camera->position;

        // Mesh
        //float scale = (1.0f / (std::max)(meshModel.aabb[0][0], (std::max)(meshModel.aabb[1][1], meshModel.aabb[2][2]))) * 0.5f;
        //glm::vec3 translate = -glm::vec3(meshModel.aabb[3][0], meshModel.aabb[3][1], meshModel.aabb[3][2]);
        //translate += -0.5f * glm::vec3(meshModel.aabb[0][0], meshModel.aabb[1][1], meshModel.aabb[2][2]);

        //shaderValuesScene.model = glm::mat4(1.0f);
        //shaderValuesScene.model[0][0] = scale;
        //shaderValuesScene.model[1][1] = scale;
        //shaderValuesScene.model[2][2] = scale;
        //shaderValuesScene.model = glm::translate(shaderValuesScene.model, translate);
    }

    //void updateDebugUniformBuffer(glm::mat4 model) {
    //    shaderValuesDebug.projection = m_camera->matrices.perspective;
    //    shaderValuesDebug.view = m_camera->matrices.view;
    //    shaderValuesDebug.camPos = m_camera->position;
    //    shaderValuesDebug.model = glm::scale(model, glm::vec3(0.01));
    //}
    //
    //void initDescriptorPool()
    //{
    //    uint32_t imageSamplerCount = 0;
    //    uint32_t materialCount = 0;
    //    uint32_t meshCount = 0;
    //
    //    std::vector<vkglTF::VulkanglTFModel*> modellist = { &meshModel };
    //    for (auto& model : modellist) {
    //        for (auto& material : model->materials) {
    //            imageSamplerCount += 5;
    //            materialCount++;
    //        }
    //        for (auto node : model->linearNodes) {
    //            if (node->mesh) {
    //                meshCount++;
    //            }
    //        }
    //    }
    //
    //    std::vector<VkDescriptorPoolSize> poolSizes = {
    //        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, (4 + meshCount) * (uint32_t)m_device->getSwapChainimages().size() },
    //        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageSamplerCount * (uint32_t)m_device->getSwapChainimages().size()}
    //    };
    //
    //    m_device->m_descriptorPool = m_device->createDescriptorPool(
    //        m_device->getDevice(),
    //        poolSizes,
    //        (2 + materialCount + meshCount) * (uint32_t)m_device->getSwapChainimages().size()
    //    );
    //}
    //
    //void initDescriptorSetLayout() {
    //    // Scene
    //    {
    //        std::vector<DescriptorSetLayoutBinding> sceneLayoutBindings = {
    //            { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
    //            { 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
    //        };
    //        descriptorSetLayouts.scene = m_device->createDescriptorSetLayout(m_device->getDevice(), { sceneLayoutBindings });
    //    }
    //    // Material
    //    {
    //        std::vector<DescriptorSetLayoutBinding> materialLayoutBindings = {
    //            { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
    //            { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
    //            { 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
    //            { 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
    //            { 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
    //        };
    //        descriptorSetLayouts.materials = m_device->createDescriptorSetLayout(m_device->getDevice(), { materialLayoutBindings });
    //    }
    //    // Model node (matrices)
    //    {
    //        std::vector<DescriptorSetLayoutBinding> nodeSetLayoutBindings = {
    //            { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
    //        };
    //        descriptorSetLayouts.node = m_device->createDescriptorSetLayout(m_device->getDevice(), { nodeSetLayoutBindings });
    //
    //        // Per-Node descriptor set
    //        for (auto& node : meshModel.nodes) {
    //            meshModel.initNodeDescriptor(node, descriptorSetLayouts.node);
    //        }
    //    }
    //
    //    VkPushConstantRange pushConstantRange{};
    //    pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    //    pushConstantRange.size = sizeof(PushConstBlockMaterial);
    //    pushConstantRange.offset = 0;
    //
    //    m_pipelineLayout = m_device->createPipelineLayout(m_device->getDevice(), { descriptorSetLayouts.scene, descriptorSetLayouts.materials, descriptorSetLayouts.node }, { pushConstantRange });
    //}
    //
    //void initDescriptorSet()
    //{
    //    // Scene
    //    for (auto i = 0; i < descriptorSets.size(); i++) {
    //        descriptorSets[i].scene = m_device->createDescriptorSet(m_device->getDevice(), m_device->getDescriptorPool(), descriptorSetLayouts.scene);
    //        std::array<VkWriteDescriptorSet, 2> writeDescriptorSets{};
    //
    //        writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    //        writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    //        writeDescriptorSets[0].descriptorCount = 1;
    //        writeDescriptorSets[0].dstSet = descriptorSets[i].scene;
    //        writeDescriptorSets[0].dstBinding = 0;
    //        writeDescriptorSets[0].pBufferInfo = &uniformBuffers[i].scene.descriptor;
    //
    //        writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    //        writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    //        writeDescriptorSets[1].descriptorCount = 1;
    //        writeDescriptorSets[1].dstSet = descriptorSets[i].scene;
    //        writeDescriptorSets[1].dstBinding = 1;
    //        writeDescriptorSets[1].pBufferInfo = &uniformBuffers[i].params.descriptor;
    //
    //        vkUpdateDescriptorSets(m_device->getDevice(), static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
    //    }
    //    // Debug Bone
    //    for (auto i = 0; i < descriptorSets.size(); i++) {
    //        descriptorSets[i].debug = m_device->createDescriptorSet(m_device->getDevice(), m_device->getDescriptorPool(), descriptorSetLayouts.scene);
    //        std::array<VkWriteDescriptorSet, 2> writeDescriptorSets{};
    //
    //        writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    //        writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    //        writeDescriptorSets[0].descriptorCount = 1;
    //        writeDescriptorSets[0].dstSet = descriptorSets[i].debug;
    //        writeDescriptorSets[0].dstBinding = 0;
    //        writeDescriptorSets[0].pBufferInfo = &uniformBuffers[i].debug.descriptor;
    //
    //        writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    //        writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    //        writeDescriptorSets[1].descriptorCount = 1;
    //        writeDescriptorSets[1].dstSet = descriptorSets[i].debug;
    //        writeDescriptorSets[1].dstBinding = 1;
    //        writeDescriptorSets[1].pBufferInfo = &uniformBuffers[i].params.descriptor;
    //
    //        vkUpdateDescriptorSets(m_device->getDevice(), static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
    //    }
    //    // Material
    //    {
    //        // Per-Material descriptor sets
    //        for (auto& material : meshModel.materials) {
    //            material.descriptorSet = m_device->createDescriptorSet(m_device->getDevice(), m_device->getDescriptorPool(), descriptorSetLayouts.materials);
    //            VkDescriptorImageInfo emptyDescriptorImageInfo{ m_defaultSampler, emptyTexture.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    //
    //            std::vector<VkDescriptorImageInfo> imageDescriptors = {
    //                emptyDescriptorImageInfo,
    //                emptyDescriptorImageInfo,
    //                material.normalTexture ? material.normalTexture->getDescriptorImageInfo() : emptyDescriptorImageInfo,
    //                material.occlusionTexture ? material.occlusionTexture->getDescriptorImageInfo() : emptyDescriptorImageInfo,
    //                material.emissiveTexture ? material.emissiveTexture->getDescriptorImageInfo() : emptyDescriptorImageInfo
    //            };
    //
    //            if (material.pbrWorkflows.metallicRoughness) {
    //                if (material.baseColorTexture) {
    //                    imageDescriptors[0] = material.baseColorTexture->getDescriptorImageInfo();
    //                }
    //                if (material.metallicRoughnessTexture) {
    //                    imageDescriptors[1] = material.metallicRoughnessTexture->getDescriptorImageInfo();
    //                }
    //            }
    //            else if (material.pbrWorkflows.specularGlossiness) {
    //                if (material.extension.diffuseTexture) {
    //                    imageDescriptors[0] = material.extension.diffuseTexture->getDescriptorImageInfo();
    //                }
    //                if (material.extension.specularGlossinessTexture) {
    //                    imageDescriptors[1] = material.extension.specularGlossinessTexture->getDescriptorImageInfo();
    //                }
    //            }
    //
    //            std::array<VkWriteDescriptorSet, 5> writeDescriptorSets{};
    //            for (size_t i = 0; i < imageDescriptors.size(); i++) {
    //                writeDescriptorSets[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    //                writeDescriptorSets[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    //                writeDescriptorSets[i].descriptorCount = 1;
    //                writeDescriptorSets[i].dstSet = material.descriptorSet;
    //                writeDescriptorSets[i].dstBinding = static_cast<uint32_t>(i);
    //                writeDescriptorSets[i].pImageInfo = &imageDescriptors[i];
    //            }
    //            vkUpdateDescriptorSets(m_device->getDevice(), static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
    //        }
    //    }
    //}
    //
    //void initPipelines()
    //{
    //    VertexInputState vertexInputState = {
    //        {
    //            { 0, sizeof(vkglTF::Vertex), VK_VERTEX_INPUT_RATE_VERTEX }
    //        },
    //        {
    //            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vkglTF::Vertex, pos)},
    //            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vkglTF::Vertex, normal)},
    //            {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(vkglTF::Vertex, uv0)},
    //            {3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(vkglTF::Vertex, uv1)},
    //            {4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vkglTF::Vertex, joint0)},
    //            {5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vkglTF::Vertex, weight0)}
    //        }
    //    };
    //
    //    InputAssemblyState inputAssembly{};
    //    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    //    inputAssembly.primitiveRestartEnable = VK_FALSE;
    //
    //    ViewportState viewport{};
    //    viewport.x = 0;
    //    viewport.y = 0;
    //    viewport.width = m_device->getSwapChainExtent().width;
    //    viewport.height = m_device->getSwapChainExtent().height;
    //
    //    RasterizationState rasterizer{};
    //    rasterizer.depthClampEnable = VK_FALSE;
    //    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    //    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    //    rasterizer.lineWidth = 1.0f;
    //    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;;
    //    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    //    rasterizer.depthBiasEnable = VK_FALSE;
    //
    //    MultisampleState multisampling{};
    //    multisampling.sampleShadingEnable = VK_FALSE;
    //    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    //
    //    DepthStencilState depthStencil{};
    //    depthStencil.depthTestEnable = VK_TRUE;
    //    depthStencil.depthWriteEnable = VK_TRUE;
    //    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    //    depthStencil.depthBoundsTestEnable = VK_FALSE;
    //    depthStencil.stencilTestEnable = VK_FALSE;
    //    depthStencil.front = depthStencil.back;
    //    depthStencil.back.compareOp = VK_COMPARE_OP_ALWAYS;
    //
    //    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    //    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    //    colorBlendAttachment.blendEnable = VK_FALSE;
    //
    //    std::vector<VkPipelineColorBlendAttachmentState> attachments{ colorBlendAttachment };
    //
    //    ColorBlendState colorBlending{};
    //    colorBlending.logicOpEnable = VK_FALSE;
    //    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    //    colorBlending.attachments = attachments;
    //    colorBlending.blendConstants[0] = 0.0f;
    //    colorBlending.blendConstants[1] = 0.0f;
    //    colorBlending.blendConstants[2] = 0.0f;
    //    colorBlending.blendConstants[3] = 0.0f;
    //
    //    std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    //    VkPipelineDynamicStateCreateInfo dynamicState{};
    //    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    //    dynamicState.pDynamicStates = dynamicStates.data();
    //    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());;
    //
    //    // Solid rendering pipeline
    //    std::vector<ShaderStage> shaderStages_mesh = m_device->createShader(m_device->getDevice(), "../../data/shaders/pbr.vert.spv", "../../data/shaders/pbr.frag.spv");
    //    pipelines.solid = m_device->createGraphicsPipeline(m_device->getDevice(), m_device->getPipelineCache(), shaderStages_mesh, vertexInputState, inputAssembly, viewport, rasterizer, multisampling, depthStencil, colorBlending, dynamicState, m_pipelineLayout, m_device->getRenderPass());
    //
    //    // Wire frame rendering pipeline
    //    //if (enable_wireframe) {
    //    rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
    //    rasterizer.lineWidth = 1.0f;
    //    pipelines.enable_wireframe = m_device->createGraphicsPipeline(m_device->getDevice(), m_device->getPipelineCache(), shaderStages_mesh, vertexInputState, inputAssembly, viewport, rasterizer, multisampling, depthStencil, colorBlending, dynamicState, m_pipelineLayout, m_device->getRenderPass());
    //    //}
    //    for (auto shaderStage : shaderStages_mesh)
    //        vkDestroyShaderModule(m_device->getDevice(), shaderStage.module, nullptr);
    //}

    //void buildCommandBuffers()
    //{
    //    VkCommandBufferBeginInfo beginInfo{};
    //    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    //    beginInfo.pInheritanceInfo = nullptr;
    //
    //    VkRenderPassBeginInfo renderPassInfo{};
    //    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    //    renderPassInfo.renderPass = m_device->getRenderPass();
    //    renderPassInfo.renderArea.offset = { 0, 0 };
    //    renderPassInfo.renderArea.extent = m_device->getSwapChainExtent();
    //
    //    std::array<VkClearValue, 2> clearValues{};
    //    clearValues[0].color = { 0.2f, 0.2f, 0.2f, 1.0f };
    //    clearValues[1].depthStencil = { 1.0f, 0 };
    //
    //    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    //    renderPassInfo.pClearValues = clearValues.data();
    //
    //    for (size_t i = 0; i < m_device->getCommandBuffers().size(); ++i)
    //    {
    //        renderPassInfo.framebuffer = m_device->getFramebuffers()[i];
    //        VkDeviceSize offsets[1] = { 0 };
    //
    //        VkCommandBuffer currentCB = m_device->m_commandBuffers[i];
    //        if (vkBeginCommandBuffer(currentCB, &beginInfo) != VK_SUCCESS) {
    //            throw std::runtime_error("failed to begin recording command buffer!");
    //        }
    //
    //        vkCmdBeginRenderPass(currentCB, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    //
    //        VkViewport viewport{};
    //        viewport.width = (float)WIDTH;
    //        viewport.height = (float)HEIGHT;
    //        viewport.minDepth = 0.0f;
    //        viewport.maxDepth = 1.0f;
    //        viewport.x = 0;
    //        viewport.y = 0;
    //        //viewport.y = viewport.height;
    //        vkCmdSetViewport(currentCB, 0, 1, &viewport);
    //
    //        VkRect2D scissor{};
    //        scissor.extent = { WIDTH, HEIGHT };
    //        vkCmdSetScissor(currentCB, 0, 1, &scissor);
    //
    //        // Skybox
    //        m_skybox.draw(currentCB);
    //
    //        // Model
    //        {
    //            vkCmdBindPipeline(currentCB, VK_PIPELINE_BIND_POINT_GRAPHICS, enable_wireframe ? pipelines.enable_wireframe : pipelines.solid);
    //            vkCmdBindVertexBuffers(currentCB, 0, 1, &meshModel.vertices.buffer, offsets);
    //            if (meshModel.indices.count > 0) {
    //                vkCmdBindIndexBuffer(currentCB, meshModel.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
    //            }
    //
    //            for (auto node : meshModel.nodes) {
    //                renderNode(node, i, vkglTF::Material::ALPHAMODE_OPAQUE);
    //            }
    //        }
    //
    //        auto update_gui = std::bind(&Test_RayTracing::updateGUI, this);
    //        vkCmdEndRenderPass(currentCB);
    //        gui->render(update_gui);
    //        vkEndCommandBuffer(currentCB);
    //    }
    //}

    void renderNode(vkglTF::Node* node, uint32_t cbIndex, vkglTF::Material::AlphaMode alphaMode) {
        if (node->mesh) {
            // Render mesh primitives
            for (vkglTF::Primitive* primitive : node->mesh->primitives) {
                if (primitive->material.alphaMode == alphaMode) {

                    const std::vector<VkDescriptorSet> descriptorsets = {
                        descriptorSets[cbIndex].scene,
                        primitive->material.descriptorSet,
                        node->mesh->uniformBuffer.descriptorSet,
                    };
                    auto commandBuffers = m_device->getCommandBuffers();
                    vkCmdBindDescriptorSets(commandBuffers[cbIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, static_cast<uint32_t>(descriptorsets.size()), descriptorsets.data(), 0, NULL);

                    // Pass material parameters as push constants
                    PushConstBlockMaterial pushConstBlockMaterial{};
                    pushConstBlockMaterial.emissiveFactor = primitive->material.emissiveFactor;
                    // To save push constant space, availabilty and texture coordiante set are combined
                    // -1 = texture not used for this material, >= 0 texture used and index of texture coordinate set
                    pushConstBlockMaterial.colorTextureSet = primitive->material.baseColorTexture != nullptr ? primitive->material.texCoordSets.baseColor : -1;
                    pushConstBlockMaterial.normalTextureSet = primitive->material.normalTexture != nullptr ? primitive->material.texCoordSets.normal : -1;
                    pushConstBlockMaterial.occlusionTextureSet = primitive->material.occlusionTexture != nullptr ? primitive->material.texCoordSets.occlusion : -1;
                    pushConstBlockMaterial.emissiveTextureSet = primitive->material.emissiveTexture != nullptr ? primitive->material.texCoordSets.emissive : -1;
                    pushConstBlockMaterial.alphaMask = static_cast<float>(primitive->material.alphaMode == vkglTF::Material::ALPHAMODE_MASK);
                    pushConstBlockMaterial.alphaMaskCutoff = primitive->material.alphaCutoff;

                    if (primitive->material.pbrWorkflows.metallicRoughness) {
                        // Metallic roughness workflow
                        pushConstBlockMaterial.workflow = static_cast<float>(PBR_WORKFLOW_METALLIC_ROUGHNESS);
                        pushConstBlockMaterial.baseColorFactor = primitive->material.baseColorFactor;
                        pushConstBlockMaterial.metallicFactor = primitive->material.metallicFactor;
                        pushConstBlockMaterial.roughnessFactor = primitive->material.roughnessFactor;
                        pushConstBlockMaterial.PhysicalDescriptorTextureSet = primitive->material.metallicRoughnessTexture != nullptr ? primitive->material.texCoordSets.metallicRoughness : -1;
                        pushConstBlockMaterial.colorTextureSet = primitive->material.baseColorTexture != nullptr ? primitive->material.texCoordSets.baseColor : -1;
                    }

                    if (primitive->material.pbrWorkflows.specularGlossiness) {
                        // Specular glossiness workflow
                        pushConstBlockMaterial.workflow = static_cast<float>(PBR_WORKFLOW_SPECULAR_GLOSINESS);
                        pushConstBlockMaterial.PhysicalDescriptorTextureSet = primitive->material.extension.specularGlossinessTexture != nullptr ? primitive->material.texCoordSets.specularGlossiness : -1;
                        pushConstBlockMaterial.colorTextureSet = primitive->material.extension.diffuseTexture != nullptr ? primitive->material.texCoordSets.baseColor : -1;
                        pushConstBlockMaterial.diffuseFactor = primitive->material.extension.diffuseFactor;
                        pushConstBlockMaterial.specularFactor = glm::vec4(primitive->material.extension.specularFactor, 1.0f);
                    }

                    vkCmdPushConstants(commandBuffers[cbIndex], m_pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstBlockMaterial), &pushConstBlockMaterial);

                    if (primitive->hasIndices) {
                        vkCmdDrawIndexed(commandBuffers[cbIndex], primitive->indexCount, 1, primitive->firstIndex, 0, 0);
                    }
                    else {
                        vkCmdDraw(commandBuffers[cbIndex], primitive->vertexCount, 1, 0, 0);
                    }
                }
            }
        };
        for (auto child : node->children) {
            renderNode(child, cbIndex, alphaMode);
        }
    }

    void render() override {
        auto tStart = std::chrono::high_resolution_clock::now();

        buildCommandBuffers();
        drawFrame();
        frameCounter++;
        auto tEnd = std::chrono::high_resolution_clock::now();
        auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
        frameTimer = (float)tDiff / 1000.0f;
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
        vkWaitForFences(m_device->getDevice(), 1, &m_device->waitFences[m_device->getCurrentFrame()], VK_TRUE, UINT64_MAX);
        vkResetFences(m_device->getDevice(), 1, &m_device->waitFences[m_device->getCurrentFrame()]);

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
        m_device->m_imagesInFlight[imageIndex] = m_device->waitFences[m_device->getCurrentFrame()];

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

        // Model Mesh
        //updateUniformBuffer();
        //UniformBufferSet currentUB = uniformBuffers[imageIndex];
        //memcpy(currentUB.scene.mapped, &shaderValuesScene, sizeof(shaderValuesScene));
        //memcpy(currentUB.params.mapped, &shaderValuesParams, sizeof(shaderValuesParams));
        //m_skybox.updateUniformBuffer();
        m_device->submitCommandBuffer(m_device->getGraphicsQueue(), &submitInfo, m_device->waitFences[m_device->getCurrentFrame()]);

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

        // Update Animation
        //if ((enable_animate) && (meshModel.animations.size() > 0)) {
        //    animationTimer += frameTimer * animationSpeed;
        //    if (animationTimer > meshModel.animations[animationIndex].end) {
        //        animationTimer -= meshModel.animations[animationIndex].end;
        //    }
        //    meshModel.updateAnimation(animationIndex, animationTimer);
        //}
        //updateUniformBuffer();
    }

    void destroy() {
        gui->destroy();
        meshModel.destroy();
        emptyTexture.destroy(m_device->getDevice());
        vkDestroySampler(m_device->getDevice(), m_defaultSampler, nullptr);
        vkDestroyDescriptorSetLayout(m_device->getDevice(), descriptorSetLayouts.scene, nullptr);
        vkDestroyDescriptorSetLayout(m_device->getDevice(), descriptorSetLayouts.materials, nullptr);
        vkDestroyDescriptorSetLayout(m_device->getDevice(), descriptorSetLayouts.node, nullptr);

        for (auto ubo : uniformBuffers) {
            vkDestroyBuffer(m_device->getDevice(), ubo.scene.buffer, nullptr);
            vkDestroyBuffer(m_device->getDevice(), ubo.params.buffer, nullptr);
            vkDestroyBuffer(m_device->getDevice(), ubo.debug.buffer, nullptr);
        }
        vkDestroyPipeline(m_device->getDevice(), pipelines.solid, nullptr);
        vkDestroyPipeline(m_device->getDevice(), pipelines.enable_wireframe, nullptr);
        vkDestroyPipelineLayout(m_device->getDevice(), m_pipelineLayout, nullptr);
    }

    void updateGUI() {
        ImGui::Begin("Scene Settings");
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::Text("Frame Time %.2f", frameTimer);
        ImGui::Checkbox("Enable Animation Update", &enable_animate);
        if (enable_animate) {
            ImGui::Checkbox("Enable slerp", &enable_slerp);
            ImGui::SliderFloat("Animation Speed", &animationSpeed, 0.1f, 10.0f);
        }
        ImGui::Checkbox("Show Wireframe", &enable_wireframe);
        ImGui::End();
    }

    void updateHierarchy(vkglTF::Node* node, std::function<void()> updateFunc) {
        updateFunc();
        for (auto child : node->children) {
            updateHierarchy(child, updateFunc);
        }
    }

    /*
        Gets the device address from a buffer that's needed in many places during the ray tracing setup
    */
    uint64_t getBufferDeviceAddress(VkBuffer buffer)
    {
        VkBufferDeviceAddressInfoKHR buffer_device_address_info{};
        buffer_device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        buffer_device_address_info.buffer = buffer;
        return vkGetBufferDeviceAddressKHR(m_device->getDevice(), &buffer_device_address_info);
    }

    void createAccelerationStructureBuffer(AccelerationStructure& accelerationStructure, VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo)
    {
        VkBufferCreateInfo bufferCreateInfo{};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.size = buildSizeInfo.accelerationStructureSize;
        bufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        VK_CHECK_RESULT(vkCreateBuffer(m_device->getDevice(), &bufferCreateInfo, nullptr, &accelerationStructure.buffer->buffer));
        VkMemoryRequirements memoryRequirements{};
        vkGetBufferMemoryRequirements(m_device->getDevice(), accelerationStructure.buffer->buffer, &memoryRequirements);
        VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo{};
        memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
        VkMemoryAllocateInfo memoryAllocateInfo{};
        memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
        memoryAllocateInfo.allocationSize = memoryRequirements.size;
        memoryAllocateInfo.memoryTypeIndex = m_device->findMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK_RESULT(vkAllocateMemory(m_device->getDevice(), &memoryAllocateInfo, nullptr, &accelerationStructure.buffer->memory));
        VK_CHECK_RESULT(vkBindBufferMemory(m_device->getDevice(), accelerationStructure.buffer->buffer, accelerationStructure.buffer->memory, 0));
    }

    ScratchBuffer createScratchBuffer(VkDeviceSize size)
    {
        ScratchBuffer scratchBuffer{};

        VkBufferCreateInfo bufferCreateInfo{};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.size = size;
        bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        VK_CHECK_RESULT(vkCreateBuffer(m_device->getDevice(), &bufferCreateInfo, nullptr, &scratchBuffer.handle));

        VkMemoryRequirements memoryRequirements{};
        vkGetBufferMemoryRequirements(m_device->getDevice(), scratchBuffer.handle, &memoryRequirements);

        VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo{};
        memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

        VkMemoryAllocateInfo memoryAllocateInfo = {};
        memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
        memoryAllocateInfo.allocationSize = memoryRequirements.size;
        memoryAllocateInfo.memoryTypeIndex = m_device->findMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK_RESULT(vkAllocateMemory(m_device->getDevice(), &memoryAllocateInfo, nullptr, &scratchBuffer.memory));
        VK_CHECK_RESULT(vkBindBufferMemory(m_device->getDevice(), scratchBuffer.handle, scratchBuffer.memory, 0));

        VkBufferDeviceAddressInfoKHR bufferDeviceAddressInfo{};
        bufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        bufferDeviceAddressInfo.buffer = scratchBuffer.handle;
        scratchBuffer.device_address = vkGetBufferDeviceAddressKHR(m_device->getDevice(), &bufferDeviceAddressInfo);

        return scratchBuffer;
    }

    void deleteScratchBuffer(ScratchBuffer& scratch_buffer)
    {
        if (scratch_buffer.memory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device->getDevice(), scratch_buffer.memory, nullptr);
        }
        if (scratch_buffer.handle != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device->getDevice(), scratch_buffer.handle, nullptr);
        }
    }

    uint32_t alignedSize(uint32_t value, uint32_t alignment)
    {
        return (value + alignment - 1) & ~(alignment - 1);
    }
};

App* create_application()
{
    return new Test_RayTracing();
}