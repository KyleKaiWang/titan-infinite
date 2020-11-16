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
	VkRenderPass imGuiRenderPass;
	VkDescriptorPool imguiDescriptorPool;
	ImGui_ImplVulkanH_Window g_MainWindowData;
	int g_MinImageCount = 2;
	float scale = 1.0f;
	bool showDemoWindow = false;

	void init(Device* device);
	void draw();
	void render();
	void resize(uint32_t width, uint32_t height);

	void destroy();
};