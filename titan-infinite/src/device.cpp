/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#include "pch.h"
#include "device.h"
Device::Device()
{
}

Device::~Device()
{
    destroy();
}

void Device::create(Window* window, const std::unordered_map<const char*, bool>& instanceExtensions, const std::unordered_map<const char*, bool>& deviceExtensions, std::function<void()> func) {
    m_window = window;
    
    // Add extenisons
    {
        for (auto& extension : deviceExtensions) {
                m_enabledExtensions.emplace_back(extension.first);
        }

        for (auto& extension : instanceExtensions) {
            m_enabledInstanceExtensions.emplace_back(extension.first);
        }
    }
    createInstance();
    setupDebugMessenger(m_instance);
    createSurface(m_instance, m_window->getNativeWindow());
    pickPhysicalDevice();

    // Require enable features
    func();
    checkDeviceExtensionSupport(m_physicalDevice);
    
    VkPhysicalDeviceFeatures deviceFeatures{VK_FALSE};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.fillModeNonSolid = VK_TRUE;
    deviceFeatures.largePoints = VK_TRUE;

    createLogicalDevice(m_physicalDevice, m_surface, deviceFeatures);

    createSwapChain(m_physicalDevice, m_device, m_surface);
    m_commandPool = createCommandPool(m_device, vkHelper::findQueueFamilies(m_physicalDevice, m_surface).graphicsFamily.value());
    m_commandBuffers = createCommandBuffers(m_device, m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, (uint32_t)m_images.size());

    createDepthbuffer();
    createRenderPass();
    createFramebuffer();
    // Pipeline Cache
    {
        VkPipelineCacheCreateInfo pipelineCacheCreateInfo{};
        pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        vkCreatePipelineCache(m_device, &pipelineCacheCreateInfo, nullptr, &m_pipelineCache);
    }
}

void Device::destroy() {
    vkDestroyImageView(m_device, m_depthbuffer.imageView, nullptr);
    vkDestroyImage(m_device, m_depthbuffer.image, nullptr);
    vkFreeMemory(m_device, m_depthbuffer.imageMemory, nullptr);
    for (auto framebuffer : m_framebuffers)
        vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    vkFreeCommandBuffers(m_device, m_commandPool, static_cast<uint32_t>(m_commandBuffers.size()), m_commandBuffers.data());
    vkDestroyRenderPass(m_device, m_renderPass, nullptr);
    destroySwapChain();
    destroyDescriptorPool();
    for (int i = 0; i < waitFences.size(); i++)
        vkDestroyFence(m_device, waitFences[i], nullptr);
    for (int i = 0; i < m_imageAvailableSemaphores.size(); i++)
        vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
    for (int i = 0; i < m_renderFinishedSemaphores.size(); i++)
        vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
    destroyCommandPool();
    vkDestroyDevice(m_device, nullptr);
    if (enableValidationLayers) {
        vkHelper::DestroyDebugUtilsMessengerEXT(m_instance, debugMessenger, nullptr);
    }
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    vkDestroyInstance(m_instance, nullptr);
}

void Device::createSwapChain(const VkPhysicalDevice& physicalDevice, VkDevice device, const VkSurfaceKHR& surface) {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice, surface);
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities, WIDTH, HEIGHT);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = vkHelper::findQueueFamilies(physicalDevice, surface);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &m_swapChain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(device, m_swapChain, &imageCount, nullptr);
    m_images.resize(imageCount);
    vkGetSwapchainImagesKHR(device, m_swapChain, &imageCount, m_images.data());

    m_swapChainImageFormat = surfaceFormat.format;
    m_swapChainExtent = extent;

    // Create Swapchain Image Views
    {
        m_imageViews.resize(m_images.size());
        for (uint32_t i = 0; i < m_images.size(); i++) {
            m_imageViews[i] = createImageView(m_device,
                m_images[i],
                VK_IMAGE_VIEW_TYPE_2D,
                getSwapChainImageFormat(),
                { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
                { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
        }
    }

    // Sync object
    m_imageAvailableSemaphores.resize(renderAhead);
    m_renderFinishedSemaphores.resize(renderAhead);
    waitFences.resize(renderAhead);
    m_imagesInFlight.resize(m_images.size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (size_t i = 0; i < renderAhead; ++i) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &waitFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}

void Device::destroySwapChain() {
    for (auto imageView : m_imageViews) {
        vkDestroyImageView(m_device, imageView, nullptr);
    }  
    vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
}

VkCommandPool Device::createCommandPool(const VkDevice& device, uint32_t queueFamilyIndices) {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndices;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkCommandPool commandPool;
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }
    return commandPool;
}

void Device::destroyCommandPool() {    
    vkDestroyCommandPool(m_device, m_commandPool, NULL);
}

VkDescriptorPool Device::createDescriptorPool(const VkDevice& device, const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets) {
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = maxSets;

    VkDescriptorPool descriptorPool;
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
    return descriptorPool;
}

void Device::destroyDescriptorPool() {
    vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
}

void Device::createInstance() {
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("validation layers requested, but not available!");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Titan Infinite Renderer";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Titan Infinite";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    {
        auto extensions = getRequiredExtensions();
        for (int i = 0; i < extensions.size(); ++i) {
            m_enabledInstanceExtensions.push_back(extensions[i]);
        }
    }
    createInfo.enabledExtensionCount = static_cast<uint32_t>(m_enabledInstanceExtensions.size());
    createInfo.ppEnabledExtensionNames = m_enabledInstanceExtensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    }
    else {
        createInfo.enabledLayerCount = 0;

        createInfo.pNext = nullptr;
    }

    // Create the Vulkan instance.
    if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance!");
    }
}

void Device::createDepthbuffer() {
    auto depthFormat = vkHelper::findDepthFormat(m_physicalDevice);
    m_depthbuffer.image = createImage(m_device, 0, VK_IMAGE_TYPE_2D, depthFormat, { WIDTH, HEIGHT, 1 }, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_IMAGE_LAYOUT_UNDEFINED);

    VkMemoryRequirements memRequirements = memory::getMemoryRequirements(m_device, m_depthbuffer.image);
    m_depthbuffer.imageMemory = memory::allocate(m_device, memRequirements.size, findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    memory::bind(m_device, m_depthbuffer.imageMemory, 0, m_depthbuffer.image);
    m_depthbuffer.imageView = createImageView(
        m_device,
        m_depthbuffer.image,
        VK_IMAGE_VIEW_TYPE_2D,
        depthFormat,
        { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
        { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 });
}

void Device::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = getSwapChainImageFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = vkHelper::findDepthFormat(getPhysicalDevice());
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

    m_renderPass = createRenderPass(m_device, attachmentDesc, subpassDesc, dependencies);
}

void Device::createFramebuffer() {
    std::vector<VkImageView> imageViews = getSwapChainimageViews();
    m_framebuffers.resize(imageViews.size());
    for (std::size_t i = 0; i < getSwapChainimages().size(); ++i) {
        m_framebuffers[i] = createFramebuffer(m_device, m_renderPass, { imageViews[i], m_depthbuffer.imageView }, getSwapChainExtent(), 1);
    }
}

std::vector<VkCommandBuffer> Device::createCommandBuffers(const VkDevice& device, const VkCommandPool& commandPool, VkCommandBufferLevel level, uint32_t commandBufferCount) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = level;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = commandBufferCount;

    std::vector<VkCommandBuffer> commandBuffers(commandBufferCount);
    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
    return std::move(commandBuffers);
}

VkCommandBuffer Device::createCommandBuffer(VkCommandBufferLevel level, bool begin)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = level;
    allocInfo.commandPool = getCommandPool();
    allocInfo.commandBufferCount = 1;
    VkCommandBuffer cmdBuffer;
    if (vkAllocateCommandBuffers(getDevice(), &allocInfo, &cmdBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
    // If requested, also start recording for the new command buffer
    if (begin)
    {
        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        vkBeginCommandBuffer(cmdBuffer, &beginInfo);
    }
    return cmdBuffer;
}

void Device::submitCommandBuffer(const VkQueue& queue, const VkSubmitInfo* submitInfo, const VkFence& fence) {
    vkQueueSubmit(queue, 1, submitInfo, fence ? fence : VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
}

void Device::flushCommandBuffer(const VkCommandBuffer& commandBuffer, const VkQueue& queue, bool free)
{
    if (commandBuffer == VK_NULL_HANDLE) return;

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    // Create fence to ensure that the command buffer has finished executing
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    VkFence fence;
    if (vkCreateFence(getDevice(), &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        throw std::runtime_error("failed to create synchronization objects for a frame!");
    }
    vkQueueSubmit(queue, 1, &submitInfo, fence);

    // Wait for the fence to signal that command buffer has finished executing
    vkWaitForFences(getDevice(), 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(getDevice(), fence, nullptr);
    if (free)
    {
        vkFreeCommandBuffers(getDevice(), getCommandPool(), 1, &commandBuffer);
    }
}

VkCommandBuffer Device::beginImmediateCommandBuffer()
{
    VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(m_commandBuffers[m_currentFrame], &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin immediate command buffer (still in recording state?)");
    }
    return m_commandBuffers[m_currentFrame];
}

void Device::executeImmediateCommandBuffer(VkCommandBuffer commandBuffer) 
{
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to end immediate command buffer");
    }
    auto queue = getGraphicsQueue();
    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    if (vkResetCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT) != VK_SUCCESS) {
        throw std::runtime_error("Failed to reset immediate command buffer");
    }
}

VkImage Device::createImage(
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

VkImageView Device::createImageView(
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

VkRenderPass Device::createRenderPass(
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

VkFramebuffer Device::createFramebuffer(
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

VkShaderModule Device::createShaderModule(const VkDevice& device, const std::vector<char>& code) {
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

std::vector<ShaderStage> Device::createShader(const VkDevice& device, const std::string& vertexShaderFile, const std::string& pixelShaderFile) {
    ShaderStage vertShaderStage{};
    vertShaderStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStage.module = createShaderModule(device, vkHelper::readFile(vertexShaderFile));
    vertShaderStage.pName = "main";

    ShaderStage fragShaderStage{};
    fragShaderStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStage.module = createShaderModule(device, vkHelper::readFile(pixelShaderFile));
    fragShaderStage.pName = "main";

    std::vector<ShaderStage> shaderStages{ vertShaderStage, fragShaderStage };
    return shaderStages;
}

ShaderStage Device::createShader(const VkDevice& device, const std::string& computeShaderFile) {
    ShaderStage computeShaderStage{};
    computeShaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeShaderStage.module = createShaderModule(device, vkHelper::readFile(computeShaderFile));
    computeShaderStage.pName = "main";

    return computeShaderStage;
}

ShaderStage Device::createRayTracingShader(const std::string& shaderFile, VkShaderStageFlagBits stage) {
    ShaderStage shaderStage{};
    shaderStage.stage = stage;
    shaderStage.module = createShaderModule(m_device, vkHelper::readFile(shaderFile));
    shaderStage.pName = "main";

    return shaderStage;
}

VkDescriptorSetLayout Device::createDescriptorSetLayout(const VkDevice& device, const std::vector<DescriptorSetLayoutBinding>& descriptorSetLayoutBindings) {
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

VkPipelineLayout Device::createPipelineLayout(const VkDevice& device, const std::vector<VkDescriptorSetLayout>& descriptorSetLayout, const std::vector<VkPushConstantRange>& pushConstantRanges) {
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

VkPipeline Device::createGraphicsPipeline(
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

VkPipeline Device::createComputePipeline(const VkDevice& device, const std::string& computeShaderFile, VkPipelineLayout layout, const VkSpecializationInfo* specializationInfo)
{
    ShaderStage computeShader = createShader(device, computeShaderFile);

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


std::vector<VkDescriptorSet> Device::createDescriptorSets(const VkDevice& device, const VkDescriptorPool& descriptorPool, const std::vector<VkDescriptorSetLayout>& descriptorSetLayout) {
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

VkDescriptorSet Device::createDescriptorSet(const VkDevice& device, const VkDescriptorPool& descriptorPool, const VkDescriptorSetLayout& descriptorSetLayout) {

    std::vector<VkDescriptorSet> descriptorSet{ createDescriptorSets(device, descriptorPool, { descriptorSetLayout }) };
    return descriptorSet[0];
}

void Device::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

void Device::setupDebugMessenger(VkInstance instance) {
    if (!enableValidationLayers) return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);

    if (vkHelper::CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("failed to set up debug messenger!");
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL Device::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

std::vector<const char*> Device::getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

void Device::createSurface(VkInstance instance, GLFWwindow* window) {
    if (glfwCreateWindowSurface(instance, window, nullptr, &m_surface) != VK_SUCCESS) {
        std::cout << "Could not create a Vulkan surface." << std::endl;
    }
}

void Device::pickPhysicalDevice() {
    uint32_t deviceCount = 1;
    const uint32_t reqCount = deviceCount;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    VkResult res = vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());
    assert(!res && deviceCount >= reqCount);
    m_physicalDevice = devices[0];


    if (m_physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find a suitable GPU!");
    }

    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &m_memoryProperties);
}

bool Device::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<const char*> requiredExtensions(m_enabledExtensions.begin(), m_enabledExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

bool Device::checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

void Device::createLogicalDevice(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkPhysicalDeviceFeatures enabledFeatures) {
    QueueFamilyIndices indices = vkHelper::findQueueFamilies(physicalDevice, surface);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkDeviceCreateInfo createInfo{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &enabledFeatures;

    // If a pNext(Chain) has been passed, we need to add it to the device creation info
    VkPhysicalDeviceVulkan12Features features12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;
    
    VkPhysicalDeviceFeatures2 deviceFeatures2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    deviceFeatures2.pNext = &features12;

    createInfo.pEnabledFeatures = nullptr;
    createInfo.pNext = &deviceFeatures2;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(m_enabledExtensions.size());
    createInfo.ppEnabledExtensionNames = m_enabledExtensions.data();

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }

    vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0, &m_presentQueue);
}

uint32_t Device::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    for (uint32_t i = 0; i < m_memoryProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) && (m_memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

bool Device::memoryTypeNeedsStaging(uint32_t memoryTypeIndex) const
{
    assert(memoryTypeIndex < m_memoryProperties.memoryTypeCount);
    const VkMemoryPropertyFlags flags = m_memoryProperties.memoryTypes[memoryTypeIndex].propertyFlags;
    return (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0;
}

SwapChainSupportDetails Device::querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkPresentModeKHR Device::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkSurfaceFormatKHR Device::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}


VkExtent2D Device::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }
    else {
        VkExtent2D actualExtent = { width, height };

        actualExtent.width = (std::max)(capabilities.minImageExtent.width, (std::min)(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = (std::max)(capabilities.minImageExtent.height, (std::min)(capabilities.maxImageExtent.height, actualExtent.height));

        return actualExtent;
    }
}