/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#pragma once
#include "window.h"

#ifdef _DEBUG
const bool enableValidationLayers = true;
#else
const bool enableValidationLayers = false;
#endif

// Use validation layers if this is a debug build
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const uint32_t WIDTH = 1280;
const uint32_t HEIGHT = 720;

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct Device {
    Device();
    ~Device();

    // Swapchain
    VkSwapchainKHR m_swapChain;
    VkFormat m_swapChainImageFormat;
    VkExtent2D m_swapChainExtent;
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;

    VkSwapchainKHR getSwapChain() const { return m_swapChain; }
    std::vector<VkImage> getSwapChainimages() const { return m_images; }
    std::vector<VkImageView> getSwapChainimageViews() const { return m_imageViews; }
    VkExtent2D getSwapChainExtent() const { return m_swapChainExtent; }
    VkFormat getSwapChainImageFormat() const { return m_swapChainImageFormat; }

    size_t m_currentFrame = 0;
    size_t getCurrentFrame() { return m_currentFrame; }

    // Synchronization
    std::vector<VkFence>       m_imagesInFlight;
    std::vector<VkFence>       waitFences;
    std::vector<VkSemaphore>   m_imageAvailableSemaphores;
    std::vector<VkSemaphore>   m_renderFinishedSemaphores;

    // Command Buffer
    VkCommandPool m_commandPool;
    std::vector<VkCommandBuffer> m_commandBuffers;
    const VkCommandPool& getCommandPool() const { return m_commandPool; }
    const std::vector<VkCommandBuffer>& getCommandBuffers() { return m_commandBuffers; }
    VkCommandBuffer getCurrentCommandBuffer() { return m_commandBuffers[m_currentFrame]; }

    // Descriptor
    VkDescriptorPool m_descriptorPool;
    std::vector<VkDescriptorSet> m_descriptorSets;
    const VkDescriptorPool& getDescriptorPool() const { return m_descriptorPool; }
    const std::vector<VkDescriptorSet>& getDescriptorSets() const { return m_descriptorSets; }

    Window* m_window;
    VkInstance m_instance;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device;
    VkSurfaceKHR m_surface;

    struct Depthbuffer {
        VkImage image;
        VkDeviceMemory imageMemory;
        VkImageView imageView;
    }m_depthbuffer;

    VkRenderPass m_renderPass;
    std::vector<VkFramebuffer> m_framebuffers;

    VkQueue m_graphicsQueue;
    VkQueue m_presentQueue;
    VkPipelineCache m_pipelineCache;
    const uint32_t renderAhead = 2;

    VkDebugUtilsMessengerEXT debugMessenger;
    VkPhysicalDeviceMemoryProperties m_memoryProperties;

    std::vector<const char*> m_enabledExtensions{};

    void* m_deviceCreatepNextChain = nullptr;

    VkDevice getDevice() const { return m_device; }
    VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
    VkInstance getInstance() const { return m_instance; }
    VkSurfaceKHR getSurface() const { return m_surface; }
    VkQueue getGraphicsQueue() const { return m_graphicsQueue; }
    VkQueue getPresentQueue() const { return m_presentQueue; }
    Window* getWindow() { return m_window; }
    VkRenderPass getRenderPass() { return m_renderPass; }
    const Depthbuffer& getDepthbuffer() const { return m_depthbuffer; }
    const std::vector<VkFramebuffer>& getFramebuffers() const { return m_framebuffers; }
    VkPipelineCache getPipelineCache() const { return m_pipelineCache; }
    void create(Window* window, std::unordered_map<const char*, bool> deviceExtensions = {}, std::function<void()> func = nullptr);
    void destroy();

    void createSurface(VkInstance instance, GLFWwindow* window);
    void createInstance();
    void createLogicalDevice(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkPhysicalDeviceFeatures enabledFeatures = {});
    
    void createSwapChain(const VkPhysicalDevice& physicalDevice, VkDevice device, const VkSurfaceKHR& surface);
    void destroySwapChain();
    
    VkCommandPool createCommandPool(const VkDevice& device, uint32_t queueFamilyIndices);
    void destroyCommandPool();
    
    VkDescriptorPool createDescriptorPool(const VkDevice& device, const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets);
    void destroyDescriptorPool();

    std::vector<VkCommandBuffer> createCommandBuffers(const VkDevice& device, const VkCommandPool& commandPool, VkCommandBufferLevel level, uint32_t commandBufferCount);
    VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, bool begin);
    void submitCommandBuffer(const VkQueue& queue, const VkSubmitInfo* submitInfo, const VkFence& fence);
    void flushCommandBuffer(const VkCommandBuffer& commandBuffer, const VkQueue& queue, bool free = true);
    VkCommandBuffer beginImmediateCommandBuffer();
    void executeImmediateCommandBuffer(VkCommandBuffer commandBuffer);

    void createDepthbuffer();
    void createRenderPass();
    void createFramebuffer();

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
        VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED);

    VkImageView createImageView(const VkDevice& device,
        VkImage image,
        VkImageViewType viewType,
        VkFormat format,
        const VkComponentMapping& components,
        const VkImageSubresourceRange& subresourceRange,
        VkImageViewCreateFlags flags = 0);

    VkRenderPass createRenderPass(const VkDevice& device,
        const std::vector<VkAttachmentDescription>& attachmentDescriptions,
        const std::vector<SubpassDescription>& subpassDescriptions,
        const std::vector<VkSubpassDependency>& subpassDependency);

    VkFramebuffer createFramebuffer(const VkDevice& device,
        const VkRenderPass& renderPass,
        const std::vector<VkImageView>& imageViews,
        VkExtent2D extent,
        uint32_t layers);

    VkShaderModule createShaderModule(const VkDevice& device, const std::vector<char>& code);
    std::vector<ShaderStage> createShader(const VkDevice& device, const std::string& vertexShaderFile, const std::string& pixelShaderFile);
    ShaderStage createShader(const VkDevice& device, const std::string& computeShaderFile);
    ShaderStage createRayTracingShader(const std::string& shaderFile, VkShaderStageFlagBits stage);
    VkDescriptorSetLayout createDescriptorSetLayout(const VkDevice& device, const std::vector<DescriptorSetLayoutBinding>& descriptorSetLayoutBindings);
    VkPipelineLayout createPipelineLayout(const VkDevice& device, const std::vector<VkDescriptorSetLayout>& descriptorSetLayout, const std::vector<VkPushConstantRange>& pushConstantRanges);

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
        const VkRenderPass& renderPass);

    VkPipeline createComputePipeline(const VkDevice& device, const std::string& computeShaderFile, VkPipelineLayout layout, const VkSpecializationInfo* specializationInfo = nullptr);
    std::vector<VkDescriptorSet> createDescriptorSets(const VkDevice& device, const VkDescriptorPool& descriptorPool, const std::vector<VkDescriptorSetLayout>& descriptorSetLayout);
    VkDescriptorSet createDescriptorSet(const VkDevice& device, const VkDescriptorPool& descriptorPool, const VkDescriptorSetLayout& descriptorSetLayout);

    std::vector<const char*> getRequiredExtensions();
    void pickPhysicalDevice();
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    bool checkValidationLayerSupport();
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    bool memoryTypeNeedsStaging(uint32_t memoryTypeIndex) const;
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height);
    
    void setupDebugMessenger(VkInstance instance);
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
};