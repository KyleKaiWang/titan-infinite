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
#include "gui.h"
#include "line_segment.h"
#include "spline.h"

class Application {
public:
    Application() {}
    ~Application() {
        delete m_camera;
        delete m_device;
        m_window->destroy();
        delete m_window;
    }

    void run() {
        initResource();
        mainLoop();
        destroy();
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
    vkglTF::VulkanglTFModel cubeModel;

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
        glm::mat4 projection          = glm::mat4(1.0f);
        glm::mat4 model               = glm::mat4(1.0f);
        glm::mat4 view                = glm::mat4(1.0f);
        glm::vec3 camPos              = glm::vec3(0.0f);
    }shaderValuesScene, shaderValuesDebug;

    std::vector<DescriptorSets> descriptorSets;
    std::vector<UniformBufferSet> uniformBuffers;

    struct shaderValuesParams {
        glm::vec4 lightDir                  = glm::vec4(10.0f, 10.0f, 10.0f, 1.0f);
        float exposure                      = 4.5f;
        float gamma                         = 2.2f;
        float prefilteredCubeMipLevels      = 1.0f;
        float scaleIBLAmbient               = 1.0f;
        float debugViewInputs               = 0;
        float debugViewEquation             = 0;
        float enableDebugBones              = 0.0f;
        glm::vec3 camPos                    = glm::vec3(10.0f, 10.0f, 10.0f);
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
    TextureObject checkerboardTexture;
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

    // Animation 
    int32_t animationIndex = 0;
    float animationTimer = 0.0f;
    float animationSpeed = 1.0f;

    // Inverse Kinematic
    struct IK
    {
        glm::vec3 target = glm::vec3(0.0f, 5.0f, 0.0f);
    }ccd_ik;

    // Values show on UI
    Gui* gui;
    bool enable_wireframe = false;
    bool enable_animate = false;
    bool enable_slerp = true;
    bool enable_debug_joints = false;
    bool enable_IK = false;
    bool enable_debug_spline = true;
    bool enable_debug_control_points = true;
    bool enable_moving_path = true;
    float velocity = 1.0f;
    
    // Spline
    Spline* spline;
    float _t1, _t2, _t3, pathTime;
    float distance = 0.0f;

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
        
        // Physical, Logical Device and Surface
        m_device = new Device();
        m_device->create(m_window);
        
        // Gui
        gui = new Gui();
        gui->init(m_device);

        uniformBuffers.resize(m_device->getSwapChainimages().size());
        descriptorSets.resize(m_device->getSwapChainimages().size());

        loadAssets();
        initUniformBuffers();
        initDescriptorPool();

        // Debug Line Segment
        meshModel.debug_line_segment = new LineSegment(m_device);
        meshModel.debug_line_segment->init();

        // Splines
        _t1 = _t2 = _t3 = pathTime = 0.0f;
        spline = new Spline(m_device);
        initSpline();
        spline->calculateAdaptiveTable(_t1, _t2, _t3);
        spline->init();

        initDescriptorSetLayout();
        initDescriptorSet();
        initPipelines();
        buildCommandBuffers();
    }

    void loadAssets() {
        meshModel.loadFromFile("data/models/glTF-Embedded/CesiumMan.gltf", m_device, m_device->getGraphicsQueue());
        cubeModel.loadFromFile("data/models/glTF-Embedded/Box.gltf", m_device, m_device->getGraphicsQueue());

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

        emptyTexture = texture::loadTexture("data/textures/empty.jpg", VK_FORMAT_R8G8B8A8_UNORM, m_device, 4);
        checkerboardTexture = texture::loadTexture("data/textures/checkerboard.png", VK_FORMAT_R8G8B8A8_UNORM, m_device, 4);
    }

    void initSpline() {
        spline->addControlPoint(glm::vec3(0.0f, 0.0f, -6.3f));
        spline->addControlPoint(glm::vec3(0.0f, 0.0f, 0.0f));
        spline->addControlPoint(glm::vec3(0.0f, 0.0f, 1.0f));
        spline->addControlPoint(glm::vec3(2.0f, 0.0f, 3.0f));

        spline->addControlPoint(glm::vec3(4.0f, 0.0f, 2.0f));
        spline->addControlPoint(glm::vec3(6.0f, 0.0f, -3.5f));        
        spline->addControlPoint(glm::vec3(4.0f, 0.0f, -5.5f));
        spline->addControlPoint(glm::vec3(2.0f, 0.0f, -5.8f));

        spline->addControlPoint(glm::vec3(0.0f, 0.0f, -6.0f));
        
        

        for (size_t i = 0; i < spline->m_controlPoints.size() - 3; ++i) {
            glm::mat4 matrix;
            matrix[0] = glm::vec4(spline->m_controlPoints[i], 1);
            matrix[1] = glm::vec4(spline->m_controlPoints[i + 1], 1);
            matrix[2] = glm::vec4(spline->m_controlPoints[i + 2], 1);
            matrix[3] = glm::vec4(spline->m_controlPoints[i + 3], 1);
            spline->addControlPointMatrix(glm::transpose(matrix));
        }

        for (size_t i = 0; i < spline->m_controlPointsMatrices.size(); ++i) {
            for (float j = 0.0f; j <= 1.0f; j += 0.0001f) {
                glm::vec3 point = spline->calculateBSpline(spline->m_controlPointsMatrices[i], j);
                spline->addInterpolationPoint(point);
            }
        }
        
    }

    void initUniformBuffers() {

        for (auto& uniformBuffer : uniformBuffers) {
            uniformBuffer.scene = buffer::createBuffer(
                m_device,
                sizeof(shaderValuesScene),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            );
            memory::map(m_device->getDevice(), uniformBuffer.scene.memory, 0, uniformBuffer.scene.bufferSize, &uniformBuffer.scene.mapped);
            uniformBuffer.debug = buffer::createBuffer(
                m_device,
                sizeof(shaderValuesDebug),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            );
            memory::map(m_device->getDevice(), uniformBuffer.debug.memory, 0, uniformBuffer.debug.bufferSize, &uniformBuffer.debug.mapped);
            uniformBuffer.params = buffer::createBuffer(
                m_device,
                sizeof(shaderValuesParams),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            );
            memory::map(m_device->getDevice(), uniformBuffer.params.memory, 0, uniformBuffer.params.bufferSize, &uniformBuffer.params.mapped);
        }
    }

    float ParametricBlend(float t)
    {
        float sqt = t * t;
        return sqt / (2.0f * (sqt - t) + 1.0f);
    }

    void updateUniformBuffer() {

        // Scene
        shaderValuesScene.projection = m_camera->matrices.perspective;
        shaderValuesScene.view = m_camera->matrices.view;
        shaderValuesScene.camPos = m_camera->position;

        // Mesh
        float scale = (1.0f / (std::max)(meshModel.aabb[0][0], (std::max)(meshModel.aabb[1][1], meshModel.aabb[2][2]))) * 0.5f;
        //glm::vec3 translate = -glm::vec3(meshModel.aabb[3][0], meshModel.aabb[3][1], meshModel.aabb[3][2]);
        //translate += -0.5f * glm::vec3(meshModel.aabb[0][0], meshModel.aabb[1][1], meshModel.aabb[2][2]);

        shaderValuesScene.model = glm::mat4(1.0f);
        shaderValuesScene.model[0][0] = scale;
        shaderValuesScene.model[1][1] = scale;
        shaderValuesScene.model[2][2] = scale;
        //shaderValuesScene.model = glm::translate(shaderValuesScene.model, translate);
        
        float t1 = _t1 * 6 / velocity;
        float t2 = _t2 * 6 / velocity;
        float t3 = _t3 * 6 / velocity;
        float t = pathTime;
        float v = velocity;
        if (t <= t1) {
            animationSpeed = t * (v / t1);
            distance = (t * t * 0.5f) * (v / t1);
        }
        else if (pathTime > t1 && t <= t2) {
            animationSpeed = v;
            distance = (v * t1 * 0.5f) + v * (t - t1);
            //distance = velocity * (pathTime - t1) + (pathTime * pathTime * 0.5f) * (velocity / t1);
        }
        else if (t > t2 && t <= t3) {
            animationSpeed = (t3 - t) * (v / (t3 - t2));
            distance = (((v * t1) * 0.5f) + v * (t2 - t1)) + (v - (v * (t - t2) / (t3 - t2)) * 0.5f) * (t - t2);
            //float t = pathTime;
            //float v = (t3 - t) * velocity * (1 / (t3 - t2));
            //distance =
            //    (t * t1 * 0.5f) + t * (t - t1) +
            //    v * (2 * t3 * t - t * t - 2 * t3 * t2 + t2 * t2) / 2 * (t3 - t2);

            //distance = velocity * 0.5 * 1 / (t3 - t2) * (2 * t3 * pathTime - pathTime * pathTime - 2 * t3 * t2 + t2 * t2) 
            //    + velocity * (pathTime - t1);
        }
        else {
            distance = 0.0f;
            t = 0.0f;
            animationSpeed = 0.0f;
        }
        pathTime = t;
        velocity = v;
        
        TableValue tableValue = spline->findInTable(distance);
        glm::vec3 position = spline->calculateBSpline(spline->m_controlPointsMatrices[tableValue.curveIndex], tableValue.pointOnCurve);
        glm::mat4 pathModelMatrix = glm::translate(glm::mat4(1.0f), position);
        {
            glm::vec3 V = spline->calculateBSplineDerivative(spline->m_controlPointsMatrices[tableValue.curveIndex], tableValue.pointOnCurve);
            V = glm::normalize(V);
            glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
            glm::vec3 W = glm::normalize(glm::cross(up, V));
            glm::vec3 U = glm::normalize(glm::cross(V, W));
        
            glm::mat4 rotation;
            rotation[0] = glm::vec4(W, 0.0f);
            rotation[1] = glm::vec4(U, 0.0f);
            rotation[2] = glm::vec4(V, 0.0f);
            rotation[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        
            pathModelMatrix *= rotation;
        }
        animationSpeed /= velocity;
        glm::mat4 modelMatrix = shaderValuesScene.model;
        shaderValuesScene.model = pathModelMatrix * modelMatrix;
    }
    void updateDebugUniformBuffer(glm::mat4 model) {
        shaderValuesDebug.projection = m_camera->matrices.perspective;
        shaderValuesDebug.view = m_camera->matrices.view;
        shaderValuesDebug.camPos = m_camera->position;
        shaderValuesDebug.model = glm::scale(model, glm::vec3(0.01));
    }

    void initDescriptorPool()
    {
        uint32_t imageSamplerCount = 0;
        uint32_t materialCount = 0;
        uint32_t meshCount = 0;

        std::vector<vkglTF::VulkanglTFModel*> modellist = { &meshModel, &cubeModel };
        for (auto& model : modellist) {
            for (auto& material : model->materials) {
                imageSamplerCount += 5;
                materialCount++;
            }
            for (auto node : model->linearNodes) {
                if (node->mesh) {
                    meshCount++;
                }
            }
        }

        std::vector<VkDescriptorPoolSize> poolSizes = {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, (4 + meshCount) * (uint32_t)m_device->getSwapChainimages().size() },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageSamplerCount * (uint32_t)m_device->getSwapChainimages().size()}
        };

        m_device->m_descriptorPool = m_device->createDescriptorPool(
            m_device->getDevice(),
            poolSizes,
            (2 + materialCount + meshCount) * (uint32_t)m_device->getSwapChainimages().size()
        );
    }

    void initDescriptorSetLayout() {
        // Scene
        {
            std::vector<DescriptorSetLayoutBinding> sceneLayoutBindings = {
                { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
                { 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
            };
            descriptorSetLayouts.scene = m_device->createDescriptorSetLayout(m_device->getDevice(), { sceneLayoutBindings });
        }
        // Material
        {
            std::vector<DescriptorSetLayoutBinding> materialLayoutBindings = {
                { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
                { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
                { 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
                { 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
                { 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
            };
            descriptorSetLayouts.materials = m_device->createDescriptorSetLayout(m_device->getDevice(), { materialLayoutBindings });
        }
        // Model node (matrices)
        {
            std::vector<DescriptorSetLayoutBinding> nodeSetLayoutBindings = {
                { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
            };
            descriptorSetLayouts.node = m_device->createDescriptorSetLayout(m_device->getDevice(), { nodeSetLayoutBindings });

            // Per-Node descriptor set
            for (auto& node : meshModel.nodes) {
                meshModel.initNodeDescriptor(node, descriptorSetLayouts.node);
            }

            for (auto& node : cubeModel.nodes) {
                cubeModel.initNodeDescriptor(node, descriptorSetLayouts.node);
            }
        }

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.size = sizeof(PushConstBlockMaterial);
        pushConstantRange.offset = 0;

        m_pipelineLayout = m_device->createPipelineLayout(m_device->getDevice(), { descriptorSetLayouts.scene, descriptorSetLayouts.materials, descriptorSetLayouts.node }, { pushConstantRange });
    }

    void initDescriptorSet()
    {   
        // Scene
        for (auto i = 0; i < descriptorSets.size(); i++) {
            descriptorSets[i].scene = m_device->createDescriptorSet(m_device->getDevice(), m_device->getDescriptorPool(), descriptorSetLayouts.scene);
            std::array<VkWriteDescriptorSet, 2> writeDescriptorSets{};

            writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writeDescriptorSets[0].descriptorCount = 1;
            writeDescriptorSets[0].dstSet = descriptorSets[i].scene;
            writeDescriptorSets[0].dstBinding = 0;
            writeDescriptorSets[0].pBufferInfo = &uniformBuffers[i].scene.descriptor;

            writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writeDescriptorSets[1].descriptorCount = 1;
            writeDescriptorSets[1].dstSet = descriptorSets[i].scene;
            writeDescriptorSets[1].dstBinding = 1;
            writeDescriptorSets[1].pBufferInfo = &uniformBuffers[i].params.descriptor;

            vkUpdateDescriptorSets(m_device->getDevice(), static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
        }
        // Debug Bone
        for (auto i = 0; i < descriptorSets.size(); i++) {
            descriptorSets[i].debug = m_device->createDescriptorSet(m_device->getDevice(), m_device->getDescriptorPool(), descriptorSetLayouts.scene);
            std::array<VkWriteDescriptorSet, 2> writeDescriptorSets{};

            writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writeDescriptorSets[0].descriptorCount = 1;
            writeDescriptorSets[0].dstSet = descriptorSets[i].debug;
            writeDescriptorSets[0].dstBinding = 0;
            writeDescriptorSets[0].pBufferInfo = &uniformBuffers[i].debug.descriptor;

            writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writeDescriptorSets[1].descriptorCount = 1;
            writeDescriptorSets[1].dstSet = descriptorSets[i].debug;
            writeDescriptorSets[1].dstBinding = 1;
            writeDescriptorSets[1].pBufferInfo = &uniformBuffers[i].params.descriptor;

            vkUpdateDescriptorSets(m_device->getDevice(), static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
        }
        // Material
        {
            // Per-Material descriptor sets
            for (auto& material : meshModel.materials) {
                material.descriptorSet = m_device->createDescriptorSet(m_device->getDevice(), m_device->getDescriptorPool(), descriptorSetLayouts.materials);
                VkDescriptorImageInfo emptyDescriptorImageInfo{ m_defaultSampler, emptyTexture.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

                std::vector<VkDescriptorImageInfo> imageDescriptors = {
                    emptyDescriptorImageInfo,
                    emptyDescriptorImageInfo,
                    material.normalTexture ? material.normalTexture->descriptor : emptyDescriptorImageInfo,
                    material.occlusionTexture ? material.occlusionTexture->descriptor : emptyDescriptorImageInfo,
                    material.emissiveTexture ? material.emissiveTexture->descriptor : emptyDescriptorImageInfo
                };

                if (material.pbrWorkflows.metallicRoughness) {
                    if (material.baseColorTexture) {
                        imageDescriptors[0] = material.baseColorTexture->descriptor;
                    }
                    if (material.metallicRoughnessTexture) {
                        imageDescriptors[1] = material.metallicRoughnessTexture->descriptor;
                    }
                } 
                else if (material.pbrWorkflows.specularGlossiness) {
                    if (material.extension.diffuseTexture) {
                        imageDescriptors[0] = material.extension.diffuseTexture->descriptor;
                    }
                    if (material.extension.specularGlossinessTexture) {
                        imageDescriptors[1] = material.extension.specularGlossinessTexture->descriptor;
                    }
                }

                std::array<VkWriteDescriptorSet, 5> writeDescriptorSets{};
                for (size_t i = 0; i < imageDescriptors.size(); i++) {
                    writeDescriptorSets[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    writeDescriptorSets[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    writeDescriptorSets[i].descriptorCount = 1;
                    writeDescriptorSets[i].dstSet = material.descriptorSet;
                    writeDescriptorSets[i].dstBinding = static_cast<uint32_t>(i);
                    writeDescriptorSets[i].pImageInfo = &imageDescriptors[i];
                }
                vkUpdateDescriptorSets(m_device->getDevice(), static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
            }

            for (auto& material : cubeModel.materials) {
                material.descriptorSet = m_device->createDescriptorSet(m_device->getDevice(), m_device->getDescriptorPool(), descriptorSetLayouts.materials);
                VkDescriptorImageInfo emptyDescriptorImageInfo{ m_defaultSampler, emptyTexture.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

                std::vector<VkDescriptorImageInfo> imageDescriptors = {
                    emptyDescriptorImageInfo,
                    emptyDescriptorImageInfo,
                    material.normalTexture ? material.normalTexture->descriptor : emptyDescriptorImageInfo,
                    material.occlusionTexture ? material.occlusionTexture->descriptor : emptyDescriptorImageInfo,
                    material.emissiveTexture ? material.emissiveTexture->descriptor : emptyDescriptorImageInfo
                };

                if (material.pbrWorkflows.metallicRoughness) {
                    if (material.baseColorTexture) {
                        imageDescriptors[0] = material.baseColorTexture->descriptor;
                    }
                    if (material.metallicRoughnessTexture) {
                        imageDescriptors[1] = material.metallicRoughnessTexture->descriptor;
                    }
                }
                else if (material.pbrWorkflows.specularGlossiness) {
                    if (material.extension.diffuseTexture) {
                        imageDescriptors[0] = material.extension.diffuseTexture->descriptor;
                    }
                    if (material.extension.specularGlossinessTexture) {
                        imageDescriptors[1] = material.extension.specularGlossinessTexture->descriptor;
                    }
                }

                std::array<VkWriteDescriptorSet, 5> writeDescriptorSets{};
                for (size_t i = 0; i < imageDescriptors.size(); i++) {
                    writeDescriptorSets[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    writeDescriptorSets[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    writeDescriptorSets[i].descriptorCount = 1;
                    writeDescriptorSets[i].dstSet = material.descriptorSet;
                    writeDescriptorSets[i].dstBinding = static_cast<uint32_t>(i);
                    writeDescriptorSets[i].pImageInfo = &imageDescriptors[i];
                }
                vkUpdateDescriptorSets(m_device->getDevice(), static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
            }
        }
    }

    void initPipelines()
    {
        VertexInputState vertexInputState = { 
            { 
                { 0, sizeof(vkglTF::Vertex), VK_VERTEX_INPUT_RATE_VERTEX } 
            },
            { 
                {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vkglTF::Vertex, pos)},
                {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vkglTF::Vertex, normal)},
                {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(vkglTF::Vertex, uv0)},
                {3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(vkglTF::Vertex, uv1)},
                {4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vkglTF::Vertex, joint0)},
                {5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vkglTF::Vertex, weight0)}
            }
        };

        InputAssemblyState inputAssembly{};
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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
        depthStencil.front = depthStencil.back;
        depthStencil.back.compareOp = VK_COMPARE_OP_ALWAYS;

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
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());;

        // Solid rendering pipeline
        std::vector<ShaderStage> shaderStages_mesh = m_device->createShader(m_device->getDevice(), "data/shaders/pbr.vert.spv", "data/shaders/pbr.frag.spv");
        pipelines.solid = m_device->createGraphicsPipeline(m_device->getDevice(), m_device->getPipelineCache(), shaderStages_mesh, vertexInputState, inputAssembly, viewport, rasterizer, multisampling, depthStencil, colorBlending, dynamicState, m_pipelineLayout, m_device->getRenderPass());

        // Wire frame rendering pipeline
        //if (enable_wireframe) {
            rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
            rasterizer.lineWidth = 1.0f;
            pipelines.enable_wireframe = m_device->createGraphicsPipeline(m_device->getDevice(), m_device->getPipelineCache(), shaderStages_mesh, vertexInputState, inputAssembly, viewport, rasterizer, multisampling, depthStencil, colorBlending, dynamicState, m_pipelineLayout, m_device->getRenderPass());
        //}
        for (auto shaderStage : shaderStages_mesh)
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

            // Model
            vkCmdBindPipeline(currentCB, VK_PIPELINE_BIND_POINT_GRAPHICS, enable_wireframe ? pipelines.enable_wireframe : pipelines.solid);
            vkCmdBindVertexBuffers(currentCB, 0, 1, &meshModel.vertices.buffer, offsets);
            if (meshModel.indices.count > 0) {
                vkCmdBindIndexBuffer(currentCB, meshModel.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
            }

            // Opaque primitives first
            for (auto node : meshModel.nodes) {
                renderNode(node, i, vkglTF::Material::ALPHAMODE_OPAQUE);
            }

            // Draw Spline
            spline->updateUniformBuffer(m_camera, glm::mat4(1.0f), false);
            if (enable_debug_spline)
                spline->drawSpline(currentCB);

            spline->updateUniformBuffer(m_camera, glm::mat4(1.0f), true);
            if (enable_debug_control_points)
                spline->drawControlPoints(currentCB);

            // Draw Joints
            if(enable_debug_joints)
                meshModel.drawJoint(currentCB);

            auto update_gui = std::bind(&Application::updateGUI, this);
            vkCmdEndRenderPass(currentCB);
            gui->render(update_gui);
            vkEndCommandBuffer(currentCB);
        }
    }

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

    void renderCube(vkglTF::Node* node, uint32_t cbIndex, vkglTF::Material::AlphaMode alphaMode) {
        if (node->mesh) {
            for (vkglTF::Primitive* primitive : node->mesh->primitives) {
                if (primitive->material.alphaMode == alphaMode) {
                    const std::vector<VkDescriptorSet> descriptorsets = {
                                descriptorSets[cbIndex].debug,
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

                    for (auto node : meshModel.nodes)
                        drawDebugBone(node, cbIndex, primitive);
                }
            }
        };
        for (auto child : node->children) {
            renderCube(child, cbIndex, alphaMode);
        }
    }

    void drawDebugBone(vkglTF::Node* node, uint32_t cbIndex, vkglTF::Primitive* primitive) {

        if (node->skin) {
            auto commandBuffers = m_device->getCommandBuffers();
            
            for (size_t i = 0; i < node->mesh->uniformBlock.jointcount; ++i) {
                updateDebugUniformBuffer(node->getGlobalMatrix() * node->mesh->uniformBlock.jointMatrix[i]);
                memcpy(uniformBuffers[m_device->getCurrentFrame()].debug.mapped, &shaderValuesDebug, sizeof(shaderValuesDebug));
        
                if (primitive->hasIndices) {
                    vkCmdDrawIndexed(commandBuffers[cbIndex], primitive->indexCount, 1, primitive->firstIndex, 0, 0);
                }
                else {
                    vkCmdDraw(commandBuffers[cbIndex], cubeModel.vertices.count, 1, 0, 0);
                }
            }
        }
        for (auto child : node->children) {
            drawDebugBone(child, cbIndex, primitive);
        }
    }

    void mainLoop() {
        while (!m_window->getWindowShouldClose()) {
            // Poll for user input.
            glfwPollEvents();
            render();
        }
        vkDeviceWaitIdle(m_device->getDevice());
    }

    void render() {
        auto tStart = std::chrono::high_resolution_clock::now();

        buildCommandBuffers();
        drawFrame();
        frameCounter++;
        auto tEnd = std::chrono::high_resolution_clock::now();
        auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
        frameTimer = (float)tDiff / 1000.0f;
        m_camera->update(frameTimer);

        if(enable_animate)
            pathTime += frameTimer;
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
            //recreateSwapChain();
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
        updateUniformBuffer();
        UniformBufferSet currentUB = uniformBuffers[imageIndex];
        memcpy(currentUB.scene.mapped, &shaderValuesScene, sizeof(shaderValuesScene));
        memcpy(currentUB.params.mapped, &shaderValuesParams, sizeof(shaderValuesParams));
        
        // Debug Line Segment
        meshModel.debug_line_segment->updateUniformBuffer(m_camera, shaderValuesScene.model);
        
        // Spline
        //spline->updateUniformBuffer(m_camera, glm::mat4(1.0f));
        
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
        if ((enable_animate) && (meshModel.animations.size() > 0)) {
            animationTimer += frameTimer * animationSpeed;
            if (animationTimer > meshModel.animations[animationIndex].end) {
                animationTimer -= meshModel.animations[animationIndex].end;
            }
            meshModel.updateAnimation(animationIndex, animationTimer);
        }
        else if (enable_IK) {
            // Update IK
            for (auto& node : meshModel.nodes) {
                updateIK(node);
            }
        }
        updateUniformBuffer();
    }

    void updateIK(vkglTF::Node* node) {
        if (node->skin) {
            if(!node->skin->ccd_solver->solve(ccd_ik.target))
                node->update();
        }
        for (auto child : node->children) {
            updateIK(child);
        }
    }

    void destroy() {
        gui->destroy();
        meshModel.destroy();
        cubeModel.destroy();
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
        if(enable_animate) {
            ImGui::Checkbox("Enable slerp", &enable_slerp);
            ImGui::SliderFloat("Animation Speed", &velocity, 0.1f, 10.0f);
            ImGui::Checkbox("Enable IK", &enable_IK);
            if (enable_IK) {
                ImGui::SliderFloat3("IK Target", glm::value_ptr(ccd_ik.target), -100.0f, 100.0f);
            }
        }
        ImGui::Checkbox("Show Wireframe", &enable_wireframe);
        ImGui::Checkbox("Enable Debug Joints", &enable_debug_joints);
        ImGui::Checkbox("Enable Debug Spline", &enable_debug_spline);
        ImGui::Checkbox("Enable Debug Control Points", &enable_debug_control_points);
        
        ImGui::End();
    }

    void updateHierarchy(vkglTF::Node* node, std::function<void()> updateFunc) {
        updateFunc();
        for (auto child : node->children) {
            updateHierarchy(child, updateFunc);
        }
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
