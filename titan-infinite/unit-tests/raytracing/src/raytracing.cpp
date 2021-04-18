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

    // Mesuring frame time
    float frameTimer = 0.0f;
    uint32_t frameCounter = 0;
    uint32_t lastFPS = 0;

    std::chrono::time_point<std::chrono::high_resolution_clock> lastTimestamp;

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
        Buffer                     buffer;
    };

    AccelerationStructure bottom_level_acceleration_structure{};
    AccelerationStructure top_level_acceleration_structure{};

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
            initRayTracingPipeline();
            initShaderBindingTables();
            initDescriptorSets();
            buildCommandBuffers();
        }
    }

    void initStorageImage()
    {
        storage_image.width = m_window->getWidth();
        storage_image.height = m_window->getHeight();

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = m_device->getSwapChainImageFormat();
        imageInfo.extent.width = storage_image.width;
        imageInfo.extent.height = storage_image.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VK_CHECK(vkCreateImage(m_device->getDevice(), &imageInfo, nullptr, &storage_image.image));

        VkMemoryRequirements memory_requirements;
        vkGetImageMemoryRequirements(m_device->getDevice(), storage_image.image, &memory_requirements);
        VkMemoryAllocateInfo memory_allocate_info{};
        memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memory_allocate_info.allocationSize = memory_requirements.size;
        memory_allocate_info.memoryTypeIndex = m_device->findMemoryType(memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vkAllocateMemory(m_device->getDevice(), &memory_allocate_info, nullptr, &storage_image.memory));
        VK_CHECK(vkBindImageMemory(m_device->getDevice(), storage_image.image, storage_image.memory, 0));

        storage_image.view = m_device->createImageView(
            m_device->getDevice(),
            storage_image.image,
            VK_IMAGE_VIEW_TYPE_2D,
            m_device->getSwapChainImageFormat(),
            { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
            { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

        VkCommandBuffer command_buffer = m_device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        
        texture::setImageLayout(
            command_buffer,
            storage_image.image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL,
            { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        );
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
        acceleration_structure_create_info.buffer = bottom_level_acceleration_structure.buffer.buffer;
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
        acceleration_structure_create_info.buffer = top_level_acceleration_structure.buffer.buffer;;
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
        VK_CHECK(vkCreateDescriptorSetLayout(m_device->getDevice(), &layout_info, nullptr, &descriptor_set_layout));

        VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
        pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_create_info.setLayoutCount = 1;
        pipeline_layout_create_info.pSetLayouts = &descriptor_set_layout;

        VK_CHECK(vkCreatePipelineLayout(m_device->getDevice(), &pipeline_layout_create_info, nullptr, &pipeline_layout));

        /*
            Setup ray tracing shader groups
            Each shader group points at the corresponding shader in the pipeline
        */
        std::vector<ShaderStage> shader_stages;

        // Ray generation group
        {
            shader_stages.push_back(m_device->createRayTracingShader("../../data/shaders/raygen.rgen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR));
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
            shader_stages.push_back(m_device->createRayTracingShader("../../data/shaders/miss.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR));
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
            shader_stages.push_back(m_device->createRayTracingShader("../../data/shaders//closesthit.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR));
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
        VK_CHECK(vkCreateRayTracingPipelinesKHR(m_device->getDevice(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &raytracing_pipeline_create_info, nullptr, &pipeline));
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
        VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(m_device->getDevice(), pipeline, 0, group_count, sbt_size, shader_handle_storage.data()));

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
        descriptor_set = m_device->createDescriptorSet(m_device->getDevice(), m_device->getDescriptorPool(), { descriptor_set_layout });

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
        result_image_write.descriptorCount = 1;
        result_image_write.dstBinding = 1;
        result_image_write.pImageInfo = &image_descriptor;
        
        
        VkWriteDescriptorSet uniform_buffer_write{};
        uniform_buffer_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        uniform_buffer_write.dstSet = descriptor_set;
        uniform_buffer_write.descriptorCount = 1;
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
            VK_CHECK(vkBeginCommandBuffer(currentCB, &command_buffer_begin_info));

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
                subresource_range
            );

            // Prepare ray tracing output image as transfer source
            texture::setImageLayout(
                currentCB,
                storage_image.image,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
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
                subresource_range
            );

            // Transition ray tracing output image back to general layout
            texture::setImageLayout(
                currentCB,
                storage_image.image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_IMAGE_LAYOUT_GENERAL,
                subresource_range
            );
            VK_CHECK(vkEndCommandBuffer(currentCB));
        }
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
    }

    void updateUniformBuffer() {
        uniform_data.proj_inverse = glm::inverse(m_camera->matrices.perspective);
        uniform_data.view_inverse = glm::inverse(m_camera->matrices.view);
        memcpy(ubo.mapped, &uniform_data, sizeof(uniform_data));
    }

    void render() override {
        auto tStart = std::chrono::high_resolution_clock::now();

        //buildCommandBuffers();
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
    }

    void updateGUI() {
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
        VK_CHECK(vkCreateBuffer(m_device->getDevice(), &bufferCreateInfo, nullptr, &accelerationStructure.buffer.buffer));
        VkMemoryRequirements memoryRequirements{};
        vkGetBufferMemoryRequirements(m_device->getDevice(), accelerationStructure.buffer.buffer, &memoryRequirements);
        VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo{};
        memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
        VkMemoryAllocateInfo memoryAllocateInfo{};
        memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
        memoryAllocateInfo.allocationSize = memoryRequirements.size;
        memoryAllocateInfo.memoryTypeIndex = m_device->findMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vkAllocateMemory(m_device->getDevice(), &memoryAllocateInfo, nullptr, &accelerationStructure.buffer.memory));
        VK_CHECK(vkBindBufferMemory(m_device->getDevice(), accelerationStructure.buffer.buffer, accelerationStructure.buffer.memory, 0));
    }

    ScratchBuffer createScratchBuffer(VkDeviceSize size)
    {
        ScratchBuffer scratchBuffer{};

        VkBufferCreateInfo bufferCreateInfo{};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.size = size;
        bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        VK_CHECK(vkCreateBuffer(m_device->getDevice(), &bufferCreateInfo, nullptr, &scratchBuffer.handle));

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
        VK_CHECK(vkAllocateMemory(m_device->getDevice(), &memoryAllocateInfo, nullptr, &scratchBuffer.memory));
        VK_CHECK(vkBindBufferMemory(m_device->getDevice(), scratchBuffer.handle, scratchBuffer.memory, 0));

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