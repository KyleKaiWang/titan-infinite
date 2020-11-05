/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <sstream>
#include <iomanip>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

class Gui
{
public:
	Gui();
	~Gui();

	Device* m_device;
	VkQueue queue;

	VkSampleCountFlagBits rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	uint32_t subpass = 0;

	Buffer vertexBuffer;
	Buffer indexBuffer;
	int32_t vertexCount = 0;
	int32_t indexCount = 0;

	std::vector<VkPipelineShaderStageCreateInfo> shaders;

	VkRenderPass imGuiRenderPass;
	VkDescriptorPool descriptorPool;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSet descriptorSet;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;

	VkDeviceMemory fontMemory = VK_NULL_HANDLE;
	VkImage fontImage = VK_NULL_HANDLE;
	VkImageView fontView = VK_NULL_HANDLE;
	VkSampler sampler;

	ImGui_ImplVulkanH_Window g_MainWindowData;
	int g_MinImageCount = 2;

	struct PushConstBlock {
		glm::vec2 scale;
		glm::vec2 translate;
	} pushConstBlock;

	bool visible = true;
	bool updated = false;
	float scale = 1.0f;
	bool show_demo_window = true;

	void initResources(Device* device);
	void initPipeline();

	void beginUpdate();
	bool update();
	void draw(const VkCommandBuffer commandBuffer);
	void resize(uint32_t width, uint32_t height);

	void freeResources();

	bool header(const char* caption);
	bool checkBox(const char* caption, bool* value);
	bool checkBox(const char* caption, int32_t* value);
	bool inputFloat(const char* caption, float* value, float step, uint32_t precision);
	bool sliderFloat(const char* caption, float* value, float min, float max);
	bool sliderInt(const char* caption, int32_t* value, int32_t min, int32_t max);
	bool comboBox(const char* caption, int32_t* itemindex, std::vector<std::string> items);
	bool button(const char* caption);
	void text(const char* formatstr, ...);
};