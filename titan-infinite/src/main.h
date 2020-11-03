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
#include "animation.h"

class Application {
public:
    Application() {}
    ~Application() {
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
    };

    VkPipelineLayout m_pipelineLayout;

    // glTF
    vkglTF::VulkanglTFModel meshModel;
    //Skybox skybox;

    struct Pipelines
    {
        VkPipeline solid;
        VkPipeline wireframe = VK_NULL_HANDLE;
    } pipelines;

    bool wireframe = false;

    struct DescriptorSetLayouts
    {
        VkDescriptorSetLayout scene;
        VkDescriptorSetLayout materials;
        VkDescriptorSetLayout node;
        //VkDescriptorSetLayout compute;
        //VkDescriptorSetLayout uniforms;
    } descriptorSetLayouts;

    struct UniformBufferSet {
        Buffer scene;
        Buffer params;
    };
    
    struct UBOMatrices {
        glm::mat4 projection;
        glm::mat4 model;
        glm::mat4 view;
        glm::vec3 camPos;
    }shaderValuesScene;

    std::vector<DescriptorSets> descriptorSets;
    std::vector<UniformBufferSet> uniformBuffers;

    struct shaderValuesParams {
        glm::vec4 lightDir = glm::vec4(10.0f, 10.0f, 10.0f, 1.0f);
        float exposure = 4.5f;
        float gamma = 2.2f;
        float prefilteredCubeMipLevels;
        float scaleIBLAmbient = 1.0f;
        float debugViewInputs = 0;
        float debugViewEquation = 0;
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

    //struct {
    //    TextureObject environmentCube;
    //    TextureObject irradianceCube;
    //    TextureObject lutBrdf;
    //}ibl_textures;
    
    TextureObject emptyTexture;
    VkSampler m_defaultSampler;

    struct SpecularFilterPushConstants
    {
        uint32_t level = 1;
        float roughness = 1.0f;
    };

    // GUI
    //Gui gui;

    // Mesuring frame time
    float frameTimer = 1.0f;
    uint32_t frameCounter = 0;
    uint32_t lastFPS = 0;
    std::chrono::time_point<std::chrono::high_resolution_clock> lastTimestamp;

    int32_t animationIndex = 0;
    float animationTimer = 0.0f;
    bool animate = true;

    void initResource() {

        // Camera
        m_camera = new Camera();
        m_camera->distance = 5.0f;
        m_camera->fov = 45.0f;
        m_camera->type = Camera::CameraType::lookat;
        m_camera->setPerspective(45.0f, (float)WIDTH / (float)HEIGHT, 0.1f, 1000.0f);
        m_camera->rotationSpeed = 0.25f;
        m_camera->movementSpeed = 0.1f;
        m_camera->setPosition({ 0.0f, 0.0f, m_camera->distance });
        m_camera->setRotation({ 0.0f, 0.0f, 0.0f });

        // glfw window
        m_window = new Window();
        m_window->setCamera(m_camera);
        m_window->create(WIDTH, HEIGHT);
        
        // Physical, Logical Device and Surface
        m_device = new Device();
        m_device->create(m_window);

        uniformBuffers.resize(m_device->getSwapChainimages().size());
        descriptorSets.resize(m_device->getSwapChainimages().size());

        loadAssets();

        // TODO : skip for skybox, put it back later 
        //loadEnvironment();

        initUniformBuffers();
        initDescriptorPool();
        initDescriptorSetLayout();
        initDescriptorSet();
        initPipelines();
        buildCommandBuffers();
        //initGUI();
    }

    //void initGUI() {
    //    gui.initResources(m_device);
    //    gui.initPipeline();
    //}

    void loadAssets() {
        meshModel.loadFromFile("data/models/glTF-Embedded/CesiumMan.gltf", m_device, m_device->getGraphicsQueue());
        //skybox.create(m_device, "data/models/glTF-Embedded/Box.gltf");

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

        // TODO : 
        //ibl_textures.environmentCube = texture::loadTextureCube("data/textures/papermill.ktx", VK_FORMAT_R16G16B16A16_SFLOAT, m_device, 4);
        //ibl_textures.irradianceCube = texture::loadTextureCube("data/textures/papermill.ktx", VK_FORMAT_R32G32B32A32_SFLOAT, m_device, 4);
        //ibl_textures.lutBrdf = texture::loadTexture("data/textures/empty.jpg", VK_FORMAT_R8G8B8A8_UNORM, m_device, 4);
    }
    
    //void loadEnvironment() {
    //    uint32_t kEnvMapSize = 1024;
    //    uint32_t kIrradianceMapSize = 32;
    //    uint32_t kBRDF_LUT_Size = 256;
    //    uint32_t kEnvMapLevels = std::floor(std::log2((std::max)(kEnvMapSize, kEnvMapSize))) + 1;
    //    
    //    // Environment map (with pre-filtered mip chain)
    //    //ibl_textures.environmentCube = texture::createTexture(m_device, kEnvMapSize, kEnvMapSize, 6, VK_FORMAT_R16G16B16A16_SFLOAT, 0, VK_IMAGE_USAGE_STORAGE_BIT);
    //    ibl_textures.environmentCube = texture::loadTextureCube("data/textures/papermill.ktx", VK_FORMAT_R16G16B16A16_SFLOAT, m_device);
    //    // Irradiance map
    //    //ibl_textures.irradianceCube = texture::createTexture(m_device, kIrradianceMapSize, kIrradianceMapSize, 6, VK_FORMAT_R16G16B16A16_SFLOAT, 1, VK_IMAGE_USAGE_STORAGE_BIT);
    //    ibl_textures.irradianceCube = texture::loadTextureCube("data/textures/papermill.ktx", VK_FORMAT_R16G16B16A16_SFLOAT, m_device);
    //    // 2D LUT for split-sum approximation
    //    //ibl_textures.lutBrdf = texture::createTexture(m_device, kBRDF_LUT_Size, kBRDF_LUT_Size, 1, VK_FORMAT_R16G16_SFLOAT, 1, VK_IMAGE_USAGE_STORAGE_BIT);
    //    ibl_textures.lutBrdf = texture::loadTexture("data/textures/empty.jpg", VK_FORMAT_R16G16B16A16_SFLOAT, m_device, 4, VK_FILTER_LINEAR);
    //    // Create samplers.
    //    VkSampler computeSampler = texture::createSampler(
    //        m_device->getDevice(),
    //        VK_FILTER_LINEAR,
    //        VK_FILTER_LINEAR,
    //        VK_SAMPLER_MIPMAP_MODE_LINEAR,
    //        VK_SAMPLER_ADDRESS_MODE_REPEAT,
    //        VK_SAMPLER_ADDRESS_MODE_REPEAT,
    //        VK_SAMPLER_ADDRESS_MODE_REPEAT,
    //        0.0,
    //        VK_TRUE,
    //        16.0f,
    //        VK_FALSE,
    //        VK_COMPARE_OP_ALWAYS,
    //        0.0,
    //        0.0,
    //        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
    //        VK_FALSE);
    //    ibl_textures.lutBrdf.sampler = texture::createSampler(
    //        m_device->getDevice(),
    //        VK_FILTER_LINEAR,
    //        VK_FILTER_LINEAR,
    //        VK_SAMPLER_MIPMAP_MODE_LINEAR,
    //        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    //        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    //        VK_SAMPLER_ADDRESS_MODE_REPEAT,
    //        0.0,
    //        VK_FALSE,
    //        16.0f,
    //        VK_FALSE,
    //        VK_COMPARE_OP_ALWAYS,
    //        0.0,
    //        FLT_MAX,
    //        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
    //        VK_FALSE);
    //    ibl_textures.lutBrdf.updateDescriptor();
    //
    //    // Create temporary descriptor pool for pre-processing compute shaders.
    //    VkDescriptorPool computeDescriptorPool;
    //    {
    //        const std::array<VkDescriptorPoolSize, 2> poolSizes = { {
    //            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
    //            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, kEnvMapLevels },
    //        } };
    //
    //        VkDescriptorPoolCreateInfo createInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    //        createInfo.maxSets = 2;
    //        createInfo.poolSizeCount = (uint32_t)poolSizes.size();
    //        createInfo.pPoolSizes = poolSizes.data();
    //        if (vkCreateDescriptorPool(m_device->getDevice(), &createInfo, nullptr, &computeDescriptorPool) != VK_SUCCESS) {
    //            throw std::runtime_error("Failed to create setup descriptor pool");
    //        }
    //    }
    //
    //    // Friendly binding names for compute pipeline descriptor set
    //    enum ComputeDescriptorSetBindingNames : uint32_t {
    //        Binding_InputTexture = 0,
    //        Binding_OutputTexture = 1,
    //        Binding_OutputMipTail = 2,
    //    };
    //
    //    // Create common descriptor set & pipeline layout for pre-processing compute shaders.
    //    VkPipelineLayout computePipelineLayout;
    //    VkDescriptorSet computeDescriptorSet;
    //    {
    //        const std::vector<DescriptorSetLayoutBinding> descriptorSetLayoutBindings = {
    //            { Binding_InputTexture, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, &computeSampler },
    //            { Binding_OutputTexture, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
    //            { Binding_OutputMipTail, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, kEnvMapLevels - 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
    //        };
    //
    //        descriptorSetLayouts.compute = renderer::createDescriptorSetLayout(m_device->getDevice(), { descriptorSetLayoutBindings });
    //        computeDescriptorSet = renderer::createDescriptorSet(m_device->getDevice(), computeDescriptorPool, descriptorSetLayouts.compute);
    //
    //        const std::vector<VkDescriptorSetLayout> pipelineSetLayouts = {
    //            descriptorSetLayouts.compute,
    //        };
    //        const std::vector<VkPushConstantRange> pipelinePushConstantRanges = {
    //            { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SpecularFilterPushConstants) },
    //        };
    //        computePipelineLayout = renderer::createPipelineLayout(m_device->getDevice(), { pipelineSetLayouts }, { pipelinePushConstantRanges });
    //    }
    //
    //    // Load & pre-process environment map.
    //    {
    //        //TextureObject envTextureUnfiltered = texture::createTexture(m_device, kEnvMapSize, kEnvMapSize, 6, VK_FORMAT_R16G16B16A16_SFLOAT, 0, VK_IMAGE_USAGE_STORAGE_BIT);
    //        TextureObject envTextureUnfiltered = texture::loadTextureCube("data/textures/papermill.ktx", VK_FORMAT_R16G16B16A16_SFLOAT, m_device, VK_IMAGE_USAGE_STORAGE_BIT);
    //        // Load & convert equirectangular envuronment map to cubemap texture
    //        {
    //            VkPipeline pipeline = renderer::createComputePipeline(m_device->getDevice(), "data/shaders/equirect2cube.comp.spv", computePipelineLayout);
    //
    //            // TODO : hdr
    //            TextureObject envTextureEquirect = texture::loadTexture("data/textures/empty.jpg", VK_FORMAT_R16G16B16A16_SFLOAT, m_device, 4, VK_FILTER_LINEAR);
    //            //texture::generateMipmaps(m_device, envTextureEquirect, VK_FORMAT_R32G32B32A32_SFLOAT);
    //
    //            const VkDescriptorImageInfo inputTexture = { VK_NULL_HANDLE, envTextureEquirect.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    //            const VkDescriptorImageInfo outputTexture = { VK_NULL_HANDLE, envTextureUnfiltered.view, VK_IMAGE_LAYOUT_GENERAL };
    //            {
    //                VkWriteDescriptorSet writeDescriptorSet = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    //                writeDescriptorSet.dstSet = computeDescriptorSet;
    //                writeDescriptorSet.dstBinding = Binding_InputTexture;
    //                writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    //                writeDescriptorSet.descriptorCount = 1;
    //                writeDescriptorSet.pImageInfo = &inputTexture;
    //                vkUpdateDescriptorSets(m_device->getDevice(), 1, &writeDescriptorSet, 0, nullptr);
    //            }
    //            {
    //                VkWriteDescriptorSet writeDescriptorSet = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    //                writeDescriptorSet.dstSet = computeDescriptorSet;
    //                writeDescriptorSet.dstBinding = Binding_OutputTexture;
    //                writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    //                writeDescriptorSet.descriptorCount = 1;
    //                writeDescriptorSet.pImageInfo = &outputTexture;
    //                vkUpdateDescriptorSets(m_device->getDevice(), 1, &writeDescriptorSet, 0, nullptr);
    //            }
    //
    //            VkCommandBuffer commandBuffer = m_device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    //            {
    //                const auto preDispatchBarrier = ImageMemoryBarrier(envTextureUnfiltered, 0, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL).mipLevels(0, 1);
    //                vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, reinterpret_cast<const VkImageMemoryBarrier*>(&preDispatchBarrier));
    //
    //                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    //                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeDescriptorSet, 0, nullptr);
    //                vkCmdDispatch(commandBuffer, kEnvMapSize / 32, kEnvMapSize / 32, 6);
    //
    //                const auto postDispatchBarrier = ImageMemoryBarrier(envTextureUnfiltered, VK_ACCESS_SHADER_WRITE_BIT, 0, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL).mipLevels(0, 1);
    //                vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, reinterpret_cast<const VkImageMemoryBarrier*>(&postDispatchBarrier));
    //            }
    //            m_device->executeImmediateCommandBuffer(commandBuffer);
    //
    //            vkDestroyPipeline(m_device->getDevice(), pipeline, nullptr);
    //            envTextureEquirect.destroy(m_device->getDevice());
    //
    //            //texture::generateMipmaps(m_device, envTextureUnfiltered, VK_FORMAT_R16G16B16A16_SFLOAT);
    //        }
    //
    //        // Compute pre-filtered specular environment map.
    //        {
    //            const uint32_t numMipTailLevels = kEnvMapLevels - 1;
    //
    //            VkPipeline pipeline;
    //            {
    //                const VkSpecializationMapEntry specializationMap = { 0, 0, sizeof(uint32_t) };
    //                const uint32_t specializationData[] = { numMipTailLevels };
    //
    //                const VkSpecializationInfo specializationInfo = { 1, &specializationMap, sizeof(specializationData), specializationData };
    //                pipeline = renderer::createComputePipeline(m_device->getDevice(), "data/shaders/spmap.comp.spv", computePipelineLayout, &specializationInfo);
    //            }
    //
    //            VkCommandBuffer commandBuffer = m_device->beginImmediateCommandBuffer();
    //
    //            // Copy base mipmap level into destination environment map.
    //            {
    //                const std::vector<ImageMemoryBarrier> preCopyBarriers = {
    //                    ImageMemoryBarrier(envTextureUnfiltered, 0, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL).mipLevels(0, 1),
    //                    ImageMemoryBarrier(ibl_textures.environmentCube, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
    //                };
    //
    //                const std::vector<ImageMemoryBarrier> postCopyBarriers = {
    //                   ImageMemoryBarrier(envTextureUnfiltered, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL).mipLevels(0, 1),
    //                   ImageMemoryBarrier(ibl_textures.environmentCube, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL),
    //                };
    //                
    //                vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, reinterpret_cast<const VkImageMemoryBarrier*>(&preCopyBarriers));
    //
    //                VkImageCopy copyRegion{};
    //                copyRegion.extent = { ibl_textures.environmentCube.width, ibl_textures.environmentCube.height, 1 };
    //                copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    //                copyRegion.srcSubresource.layerCount = ibl_textures.environmentCube.layers;
    //                copyRegion.dstSubresource = copyRegion.srcSubresource;
    //                vkCmdCopyImage(commandBuffer,
    //                    envTextureUnfiltered.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    //                    ibl_textures.environmentCube.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    //                    1, &copyRegion);
    //               
    //                vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, reinterpret_cast<const VkImageMemoryBarrier*>(&postCopyBarriers));
    //            }
    //
    //            // Pre-filter rest of the mip-chain.
    //            std::vector<VkImageView> envTextureMipTailViews;
    //            {
    //                std::vector<VkDescriptorImageInfo> envTextureMipTailDescriptors;
    //                const VkDescriptorImageInfo inputTexture = { VK_NULL_HANDLE, envTextureUnfiltered.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    //                {
    //                    VkWriteDescriptorSet writeDescriptorSet = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    //                    writeDescriptorSet.dstSet = computeDescriptorSet;
    //                    writeDescriptorSet.dstBinding = Binding_InputTexture;
    //                    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    //                    writeDescriptorSet.descriptorCount = 1;
    //                    writeDescriptorSet.pImageInfo = &inputTexture;
    //                    vkUpdateDescriptorSets(m_device->getDevice(), 1, &writeDescriptorSet, 0, nullptr);
    //                }
    //
    //                for (uint32_t level = 1; level < kEnvMapLevels; ++level) {
    //                    envTextureMipTailViews.push_back(renderer::createImageView(
    //                        m_device->getDevice(),
    //                        ibl_textures.environmentCube.image,
    //                        (ibl_textures.environmentCube.layers == 6) ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D,
    //                        VK_FORMAT_R16G16B16A16_SFLOAT,
    //                        { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
    //                        { VK_IMAGE_ASPECT_COLOR_BIT, level, 1, 0, VK_REMAINING_ARRAY_LAYERS }
    //                    ));
    //                    envTextureMipTailDescriptors.push_back(VkDescriptorImageInfo{ VK_NULL_HANDLE, envTextureMipTailViews[level - 1], VK_IMAGE_LAYOUT_GENERAL });
    //                }
    //                {
    //                    VkWriteDescriptorSet writeDescriptorSet = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    //                    writeDescriptorSet.dstSet = computeDescriptorSet;
    //                    writeDescriptorSet.dstBinding = Binding_OutputMipTail;
    //                    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    //                    writeDescriptorSet.descriptorCount = (uint32_t)envTextureMipTailDescriptors.size();
    //                    writeDescriptorSet.pImageInfo = envTextureMipTailDescriptors.data();
    //                    vkUpdateDescriptorSets(m_device->getDevice(), 1, &writeDescriptorSet, 0, nullptr);
    //                }
    //
    //                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    //                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeDescriptorSet, 0, nullptr);
    //
    //                const float deltaRoughness = 1.0f / (std::max)(float(numMipTailLevels), 1.0f);
    //                for (uint32_t level = 1, size = kEnvMapSize / 2; level < kEnvMapLevels; ++level, size /= 2) {
    //                    const uint32_t numGroups = (std::max<uint32_t>)(1, size / 32);
    //
    //                    const SpecularFilterPushConstants pushConstants = { level - 1, level * deltaRoughness };
    //                    vkCmdPushConstants(commandBuffer, computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SpecularFilterPushConstants), &pushConstants);
    //                    vkCmdDispatch(commandBuffer, numGroups, numGroups, 6);
    //                }
    //
    //                const auto barrier = ImageMemoryBarrier(ibl_textures.environmentCube, VK_ACCESS_SHADER_WRITE_BIT, 0, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    //                vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, reinterpret_cast<const VkImageMemoryBarrier*>(&barrier));
    //            }
    //
    //            m_device->executeImmediateCommandBuffer(commandBuffer);
    //
    //            for (VkImageView mipTailView : envTextureMipTailViews) {
    //                vkDestroyImageView(m_device->getDevice(), mipTailView, nullptr);
    //            }
    //            vkDestroyPipeline(m_device->getDevice(), pipeline, nullptr);
    //            envTextureUnfiltered.destroy(m_device->getDevice());
    //        }
    //
    //        // Compute diffuse irradiance cubemap
    //        {
    //            VkPipeline pipeline = renderer::createComputePipeline(m_device->getDevice(), "data/shaders/irmap.comp.spv", computePipelineLayout);
    //
    //            const VkDescriptorImageInfo inputTexture = { VK_NULL_HANDLE, ibl_textures.environmentCube.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    //            const VkDescriptorImageInfo outputTexture = { VK_NULL_HANDLE, ibl_textures.irradianceCube.view, VK_IMAGE_LAYOUT_GENERAL };
    //            {
    //                VkWriteDescriptorSet writeDescriptorSet = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    //                writeDescriptorSet.dstSet = computeDescriptorSet;
    //                writeDescriptorSet.dstBinding = Binding_InputTexture;
    //                writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    //                writeDescriptorSet.descriptorCount = 1;
    //                writeDescriptorSet.pImageInfo = &inputTexture;
    //                vkUpdateDescriptorSets(m_device->getDevice(), 1, &writeDescriptorSet, 0, nullptr);
    //            }
    //
    //            {
    //                VkWriteDescriptorSet writeDescriptorSet = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    //                writeDescriptorSet.dstSet = computeDescriptorSet;
    //                writeDescriptorSet.dstBinding = Binding_OutputTexture;
    //                writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    //                writeDescriptorSet.descriptorCount = 1;
    //                writeDescriptorSet.pImageInfo = &outputTexture;
    //                vkUpdateDescriptorSets(m_device->getDevice(), 1, &writeDescriptorSet, 0, nullptr);
    //            }
    //
    //            VkCommandBuffer commandBuffer = m_device->beginImmediateCommandBuffer();
    //            {
    //                const auto preDispatchBarrier = ImageMemoryBarrier(ibl_textures.irradianceCube, 0, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    //                vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, reinterpret_cast<const VkImageMemoryBarrier*>(&preDispatchBarrier));
    //                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    //                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeDescriptorSet, 0, nullptr);
    //                vkCmdDispatch(commandBuffer, kIrradianceMapSize / 32, kIrradianceMapSize / 32, 6);
    //
    //                const auto postDispatchBarrier = ImageMemoryBarrier(ibl_textures.irradianceCube, VK_ACCESS_SHADER_WRITE_BIT, 0, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    //                vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, reinterpret_cast<const VkImageMemoryBarrier*>(&postDispatchBarrier));
    //            }
    //            m_device->executeImmediateCommandBuffer(commandBuffer);
    //            vkDestroyPipeline(m_device->getDevice(), pipeline, nullptr);
    //        }
    //
    //        // Compute Cook-Torrance BRDF 2D LUT for split-sum approximation.
    //        {
    //            VkPipeline pipeline = renderer::createComputePipeline(m_device->getDevice(), "data/shaders/spbrdf.comp.spv", computePipelineLayout);
    //
    //            const VkDescriptorImageInfo outputTexture = { ibl_textures.lutBrdf.sampler, ibl_textures.lutBrdf.view, VK_IMAGE_LAYOUT_GENERAL };
    //            {
    //                VkWriteDescriptorSet writeDescriptorSet = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    //                writeDescriptorSet.dstSet = computeDescriptorSet;
    //                writeDescriptorSet.dstBinding = Binding_OutputTexture;
    //                writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    //                writeDescriptorSet.descriptorCount = 1;
    //                writeDescriptorSet.pImageInfo = &outputTexture;
    //                vkUpdateDescriptorSets(m_device->getDevice(), 1, &writeDescriptorSet, 0, nullptr);
    //            }
    //
    //            VkCommandBuffer commandBuffer = m_device->beginImmediateCommandBuffer();
    //            {
    //                const auto preDispatchBarrier = ImageMemoryBarrier(ibl_textures.lutBrdf, 0, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    //                vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, reinterpret_cast<const VkImageMemoryBarrier*>(&preDispatchBarrier));
    //                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    //                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeDescriptorSet, 0, nullptr);
    //                vkCmdDispatch(commandBuffer, kBRDF_LUT_Size / 32, kBRDF_LUT_Size / 32, 6);
    //
    //                const auto postDispatchBarrier = ImageMemoryBarrier(ibl_textures.lutBrdf, VK_ACCESS_SHADER_WRITE_BIT, 0, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    //                vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, reinterpret_cast<const VkImageMemoryBarrier*>(&postDispatchBarrier));
    //            }
    //            m_device->executeImmediateCommandBuffer(commandBuffer);
    //            vkDestroyPipeline(m_device->getDevice(), pipeline, nullptr);
    //        }
    //    }
    //}

    void initUniformBuffers() {

        for (auto& uniformBuffer : uniformBuffers) {
            uniformBuffer.scene = buffer::createBuffer(
                m_device,
                sizeof(shaderValuesScene),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            );
            memory::map(m_device->getDevice(), uniformBuffer.scene.memory, 0, VK_WHOLE_SIZE, &uniformBuffer.scene.mapped);
            uniformBuffer.params = buffer::createBuffer(
                m_device,
                sizeof(shaderValuesParams),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            );
            memory::map(m_device->getDevice(), uniformBuffer.params.memory, 0, VK_WHOLE_SIZE, &uniformBuffer.params.mapped);
        }
        updateUniformBuffer();
    }

    void updateUniformBuffer() {

        // Scene
        shaderValuesScene.projection = m_camera->matrices.perspective;
        shaderValuesScene.view = m_camera->matrices.view;

        float scale = (1.0f / (std::max)(meshModel.aabb[0][0], (std::max)(meshModel.aabb[1][1], meshModel.aabb[2][2]))) * 0.5f;
        glm::vec3 translate = -glm::vec3(meshModel.aabb[3][0], meshModel.aabb[3][1], meshModel.aabb[3][2]);
        translate += -0.5f * glm::vec3(meshModel.aabb[0][0], meshModel.aabb[1][1], meshModel.aabb[2][2]);

        shaderValuesScene.model = glm::mat4(1.0f);
        shaderValuesScene.model[0][0] = scale;
        shaderValuesScene.model[1][1] = scale;
        shaderValuesScene.model[2][2] = scale;
        shaderValuesScene.model = glm::translate(shaderValuesScene.model, translate);

        shaderValuesScene.camPos = glm::vec3(
            -m_camera->position.z * sin(glm::radians(m_camera->rotation.y)) * cos(glm::radians(m_camera->rotation.x)),
            -m_camera->position.z * sin(glm::radians(m_camera->rotation.x)),
            m_camera->position.z * cos(glm::radians(m_camera->rotation.y)) * cos(glm::radians(m_camera->rotation.x))
        );

        // Skybox
        //skybox.updateUniformBuffer();
    }

    void initDescriptorPool()
    {
        uint32_t imageSamplerCount = 0;
        uint32_t materialCount = 0;
        uint32_t meshCount = 0;

        // Environment samplers (radiance, irradiance, brdf lut)
        imageSamplerCount += 3;

        std::vector<vkglTF::VulkanglTFModel*> modellist = { &meshModel };
        //std::vector<vkglTF::VulkanglTFModel*> modellist = { &skybox.skyboxModel, &meshModel };
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
                { 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
                //{ 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
                //{ 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
                //{ 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
            };
            descriptorSetLayouts.scene = renderer::createDescriptorSetLayout(m_device->getDevice(), { sceneLayoutBindings });
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
            descriptorSetLayouts.materials = renderer::createDescriptorSetLayout(m_device->getDevice(), { materialLayoutBindings });
        }
        {
            // Model node (matrices)
            {
                std::vector<DescriptorSetLayoutBinding> nodeSetLayoutBindings = {
                    { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
                };
                descriptorSetLayouts.node = renderer::createDescriptorSetLayout(m_device->getDevice(), { nodeSetLayoutBindings });

                // Per-Node descriptor set
                for (auto& node : meshModel.nodes) {
                    meshModel.initNodeDescriptor(node, descriptorSetLayouts.node);
                }
            }
        }

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.size = sizeof(PushConstBlockMaterial);
        pushConstantRange.offset = 0;

        m_pipelineLayout = renderer::createPipelineLayout(m_device->getDevice(), { descriptorSetLayouts.scene, descriptorSetLayouts.materials, descriptorSetLayouts.node }, { pushConstantRange });
    }

    void initDescriptorSet()
    {   
        // Scene
        for (auto i = 0; i < descriptorSets.size(); i++) {
            descriptorSets[i].scene = renderer::createDescriptorSet(m_device->getDevice(), m_device->getDescriptorPool(), descriptorSetLayouts.scene);
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

            //writeDescriptorSets[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            //writeDescriptorSets[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            //writeDescriptorSets[2].descriptorCount = 1;
            //writeDescriptorSets[2].dstSet = descriptorSet.scene;
            //writeDescriptorSets[2].dstBinding = 2;
            //writeDescriptorSets[2].pImageInfo = &ibl_textures.irradianceCube.descriptor;
            //
            //writeDescriptorSets[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            //writeDescriptorSets[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            //writeDescriptorSets[3].descriptorCount = 1;
            //writeDescriptorSets[3].dstSet = descriptorSet.scene;
            //writeDescriptorSets[3].dstBinding = 3;
            //writeDescriptorSets[3].pImageInfo = &ibl_textures.environmentCube.descriptor;
            //
            //writeDescriptorSets[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            //writeDescriptorSets[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            //writeDescriptorSets[4].descriptorCount = 1;
            //writeDescriptorSets[4].dstSet = descriptorSet.scene;
            //writeDescriptorSets[4].dstBinding = 4;
            //writeDescriptorSets[4].pImageInfo = &ibl_textures.lutBrdf.descriptor;

            vkUpdateDescriptorSets(m_device->getDevice(), static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
        }
        // Material
        {
            // Per-Material descriptor sets
            for (auto& material : meshModel.materials) {
                material.descriptorSet = renderer::createDescriptorSet(m_device->getDevice(), m_device->getDescriptorPool(), descriptorSetLayouts.materials);
                VkDescriptorImageInfo emptyDescriptorImageInfo{ m_defaultSampler, emptyTexture.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

                std::vector<VkDescriptorImageInfo> imageDescriptors = {
                    emptyDescriptorImageInfo,
                    emptyDescriptorImageInfo,
                    material.normalTexture ? material.normalTexture->descriptor : emptyDescriptorImageInfo,
                    material.occlusionTexture ? material.occlusionTexture->descriptor : emptyDescriptorImageInfo,
                    material.emissiveTexture ? material.emissiveTexture->descriptor : emptyDescriptorImageInfo
                };

                // TODO: glTF specs states that metallic roughness should be preferred, even if specular glosiness is present
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

        // Skybox descriptorsets
        //skybox.initDescriptorSet(ibl_textures.environmentCube, m_defaultSampler);
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
        std::vector<ShaderStage> shaderStages_mesh = renderer::createShader(m_device->getDevice(), "data/shaders/pbr.vert.spv", "data/shaders/pbr.frag.spv");
        pipelines.solid = renderer::createGraphicsPipeline(m_device->getDevice(), m_device->getPipelineCache(), shaderStages_mesh, vertexInputState, inputAssembly, viewport, rasterizer, multisampling, depthStencil, colorBlending, dynamicState, m_pipelineLayout, m_device->getRenderPass());

        // Skybox initPipelines
        //skybox.initPipelines(m_renderPass);

        // Wire frame rendering pipeline
        if (wireframe) {
            rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
            rasterizer.lineWidth = 1.0f;
            pipelines.wireframe = renderer::createGraphicsPipeline(m_device->getDevice(), m_device->getPipelineCache(), shaderStages_mesh, vertexInputState, inputAssembly, viewport, rasterizer, multisampling, depthStencil, colorBlending, dynamicState, m_pipelineLayout, m_device->getRenderPass());
        }
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
        clearValues[0].color = { 1.0f, 0.0f, 0.0f, 1.0f };
        clearValues[1].depthStencil = { 1.0f, 0 };

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        for (size_t i = 0; i < m_device->getCommandBuffers().size(); ++i)
        {
            renderPassInfo.framebuffer = m_device->getFramebuffers()[i];
            VkDeviceSize offsets[1] = { 0 };

            VkCommandBuffer currentCB = m_device->m_commandBuffers[i];

            //vkResetCommandBuffer(m_device->m_commandBuffers[i], 0);
            if (vkBeginCommandBuffer(currentCB, &beginInfo) != VK_SUCCESS) {
                throw std::runtime_error("failed to begin recording command buffer!");
            }
            vkCmdBeginRenderPass(currentCB, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            VkViewport viewport{};
            viewport.width = (float)WIDTH;
            viewport.height = -(float)HEIGHT;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            viewport.x = 0;
            viewport.y = viewport.height;
            vkCmdSetViewport(currentCB, 0, 1, &viewport);
            
            VkRect2D scissor{};
            scissor.extent = { WIDTH, HEIGHT };
            vkCmdSetScissor(currentCB, 0, 1, &scissor);

            // Skybox
            //skybox.render(m_device->m_commandBuffers[i]);

            // Model
            vkCmdBindPipeline(currentCB, VK_PIPELINE_BIND_POINT_GRAPHICS, wireframe ? pipelines.wireframe : pipelines.solid);
            vkCmdBindVertexBuffers(currentCB, 0, 1, &meshModel.vertices.buffer, offsets);
            if (meshModel.indices.buffer != VK_NULL_HANDLE) {
                vkCmdBindIndexBuffer(currentCB, meshModel.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
            }

            // Opaque primitives first
            for (auto node : meshModel.nodes) {
                renderNode(node, i, vkglTF::Material::ALPHAMODE_OPAQUE);
            }

            // User interface
            //gui.draw(currentCB);

            vkCmdEndRenderPass(currentCB);
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

                    // TODO: glTF specs states that metallic roughness should be preferred, even if specular glosiness is present

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

       //updateGUI();
    }

    void drawFrame() {
        vkWaitForFences(m_device->getDevice(), 1, &m_device->waitFences[m_device->getCurrentFrame()], VK_TRUE, UINT64_MAX);

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

        vkResetFences(m_device->getDevice(), 1, &m_device->waitFences[m_device->getCurrentFrame()]);

        // Update Animation
        meshModel.updateAnimation(animationIndex, frameTimer);
        updateUniformBuffer();
        UniformBufferSet currentUB = uniformBuffers[imageIndex];
        memcpy(currentUB.scene.mapped, &shaderValuesScene, sizeof(shaderValuesScene));
        memcpy(currentUB.scene.mapped, &shaderValuesParams, sizeof(shaderValuesParams));
        //skybox.updateUniformBuffer();

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
            //recreateSwapChain();
        }
        else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }
        m_device->m_currentFrame = (m_device->getCurrentFrame() + 1) % 2;
    }

    void destroy() {
        //gui.freeResources();
        meshModel.destroy();
        emptyTexture.destroy(m_device->getDevice());
        vkDestroyDescriptorSetLayout(m_device->getDevice(), descriptorSetLayouts.scene, nullptr);
        vkDestroyDescriptorSetLayout(m_device->getDevice(), descriptorSetLayouts.materials, nullptr);
        vkDestroyDescriptorSetLayout(m_device->getDevice(), descriptorSetLayouts.node, nullptr);
        //vkDestroyDescriptorSetLayout(m_device->getDevice(), descriptorSetLayouts.uniforms, nullptr);
        //vkDestroyDescriptorSetLayout(m_device->getDevice(), descriptorSetLayouts.compute, nullptr);
        
        for (auto ubo : uniformBuffers) {
            vkDestroyBuffer(m_device->getDevice(), ubo.scene.buffer, nullptr);
            vkDestroyBuffer(m_device->getDevice(), ubo.params.buffer, nullptr);
        }
        vkDestroySampler(m_device->getDevice(), m_defaultSampler, nullptr);
        vkDestroyPipeline(m_device->getDevice(), pipelines.solid, nullptr);
        vkDestroyPipeline(m_device->getDevice(), pipelines.wireframe, nullptr);
        vkDestroyPipelineLayout(m_device->getDevice(), m_pipelineLayout, nullptr);
    }

    void destroyBuffer(const VkDevice& device, Buffer buffer) const
    {
        if (buffer.buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, buffer.buffer, nullptr);
        }
        if (buffer.memory != VK_NULL_HANDLE) {
            vkFreeMemory(device, buffer.memory, nullptr);
        }
        buffer = {};
    }

    //void updateGUI()
    //{
    //    gui.beginUpdate();
    //    ImGuiIO& io = ImGui::GetIO();
    //    //io.DisplaySize = ImVec2((float)WIDTH, (float)HEIGHT);
    //    //io.DeltaTime = frameTimer;
    //    //io.MousePos = ImVec2(m_window->getCurCursorPos().first, m_window->getCurCursorPos().second);
    //    //io.MouseDown[0] = glfwGetMouseButton(m_window->getNativeWindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    //    //io.MouseDown[1] = glfwGetMouseButton(m_window->getNativeWindow(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    //    //ImGui::NewFrame();
    //
    //    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    //    ImGui::SetNextWindowPos(ImVec2(10, 10));
    //    ImGui::SetNextWindowSize(ImVec2(200, 200), 4);
    //    ImGui::SetWindowFocus();
    //    ImGui::Begin("Vulkan Example", nullptr);
    //    ImGui::TextUnformatted("Titan Infinite");
    //    //ImGui::TextUnformatted(m_device->getPhysicalDevice().deviceName);
    //    ImGui::Text("%.2f ms/frame (%.1d fps)", (1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate));
    //    ImGui::End();
    //    ImGui::PopStyleVar();
    //
    //    ImGui::Render();
    //    //if (gui.update() || gui.updated) {
    //    //    buildCommandBuffers();
    //    //    gui.updated = false;
    //    //}
    //
    //    // Update and Render additional Platform Windows
    //    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    //    {
    //        ImGui::UpdatePlatformWindows();
    //        ImGui::RenderPlatformWindowsDefault();
    //    }
    //}
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
