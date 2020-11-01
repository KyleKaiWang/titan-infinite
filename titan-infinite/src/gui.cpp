/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */
#include "pch.h"
#include "gui.h"

Gui::Gui()
{
	// Init ImGui
	ImGui::CreateContext();

	// Dimensions
	ImGuiIO& io = ImGui::GetIO();
	io.FontGlobalScale = scale;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;        // Enable Gamepad Controls
	//io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
	//io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
	//io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoTaskBarIcons;
	//io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoMerge;

	ImGui::StyleColorsDark();
	// Color scheme
	ImGuiStyle& style = ImGui::GetStyle();
	
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}
}

Gui::~Gui() { }

/** Prepare all vulkan resources required to render the UI overlay */
void Gui::initResources(Device* device)
{
	this->device = device;
	this->queue = device->getGraphicsQueue();
	auto glfwWindow = device->getWindow()->getNativeWindow();

	ImGui_ImplGlfw_InitForVulkan(glfwWindow, true);

	ImGuiIO& io = ImGui::GetIO();

	// Create font texture
	unsigned char* fontData;
	int texWidth, texHeight;
	
	io.Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);
	VkDeviceSize uploadSize = texWidth * texHeight * 4 * sizeof(char);

	// Create target image for copy
	fontImage = renderer::createImage(
		device->getDevice(),
		0,
		VK_IMAGE_TYPE_2D,
		VK_FORMAT_R8G8B8A8_UNORM,
		{ (uint32_t)texWidth, (uint32_t)texHeight, 1 },
		1,
		1,
		VK_SAMPLE_COUNT_1_BIT,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		VK_IMAGE_LAYOUT_UNDEFINED
	);

	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(device->getDevice(), fontImage, &memReqs);
	
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex = device->findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (vkAllocateMemory(device->getDevice(), &allocInfo, nullptr, &fontMemory) != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate buffer memory!");
	}

	if (vkBindImageMemory(device->getDevice(), fontImage, fontMemory, 0) != VK_SUCCESS) {
		throw std::runtime_error("Failed to bind image memory");
	}

	fontView = renderer::createImageView(
		device->getDevice(), 
		fontImage, 
		VK_IMAGE_VIEW_TYPE_2D, 
		VK_FORMAT_R8G8B8A8_UNORM,
		{ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A }, 
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
	);

	// Staging buffers for font data upload
	Buffer stagingBuffer;
	stagingBuffer = buffer::createBuffer(
		device, 
		uploadSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);

	stagingBuffer.map();
	memcpy(stagingBuffer.mapped, fontData, uploadSize);
	stagingBuffer.unmap();

	// Copy buffer data to font image
	VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	// Prepare for transfer
	texture::setImageLayout(
		copyCmd,
		fontImage,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_HOST_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT
	);

	// Copy
	VkBufferImageCopy bufferCopyRegion = {};
	bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	bufferCopyRegion.imageSubresource.layerCount = 1;
	bufferCopyRegion.imageExtent.width = texWidth;
	bufferCopyRegion.imageExtent.height = texHeight;
	bufferCopyRegion.imageExtent.depth = 1;

	vkCmdCopyBufferToImage(
		copyCmd,
		stagingBuffer.buffer,
		fontImage,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&bufferCopyRegion
	);

	// Prepare for shader read
	texture::setImageLayout(
		copyCmd,
		fontImage,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	device->flushCommandBuffer(copyCmd, queue, true);

	stagingBuffer.destroy();

	// Font texture Sampler
	sampler = texture::createSampler(device->getDevice(),
		VK_FILTER_LINEAR,
		VK_FILTER_LINEAR,
		VK_SAMPLER_MIPMAP_MODE_LINEAR,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		0.0,
		VK_TRUE,
		1.0f,
		VK_FALSE,
		VK_COMPARE_OP_NEVER,
		0.0,
		1,
		VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
		VK_FALSE);

	// Descriptor pool
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }
	};
	descriptorPool = device->createDescriptorPool(device->getDevice(), poolSizes, 2);

	// Descriptor set layout
	std::vector<DescriptorSetLayoutBinding> setLayoutBindings = {
		{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr }
	};
	descriptorSetLayout = renderer::createDescriptorSetLayout(device->getDevice(), { setLayoutBindings });

	// Descriptor set
	descriptorSet = renderer::createDescriptorSet(device->getDevice(), descriptorPool, descriptorSetLayout);


	VkDescriptorImageInfo fontDescriptor =
	{
		sampler,
		fontView,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};

	VkWriteDescriptorSet writeDescriptorSet = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	writeDescriptorSet.dstSet = descriptorSet;
	writeDescriptorSet.dstBinding = 0;
	writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.pImageInfo = &fontDescriptor;
	vkUpdateDescriptorSets(device->getDevice(), 1,&writeDescriptorSet, 0, nullptr);
}

/** Prepare a separate pipeline for the UI overlay rendering decoupled from the main application */
void Gui::initPipeline(const VkPipelineCache pipelineCache, const VkRenderPass renderPass)
{
	// Pipeline layout
	// Push constants for UI rendering parameters
	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushConstantRange.size = sizeof(PushConstBlock);
	pushConstantRange.offset = 0;
	pipelineLayout = renderer::createPipelineLayout(device->getDevice(), { descriptorSetLayout }, { pushConstantRange });

	InputAssemblyState inputAssembly{};
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	ViewportState viewport{};
	viewport.topX = 0;
	viewport.topY = 0;
	viewport.width = device->getSwapChainExtent().width;
	viewport.height = device->getSwapChainExtent().height;

	RasterizationState rasterizer{};
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_NONE;;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	// Enable blending
	VkPipelineColorBlendAttachmentState blendAttachmentState{};
	blendAttachmentState.blendEnable = VK_TRUE;
	blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

	ColorBlendState colorBlending{};
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachments = { blendAttachmentState };
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	DepthStencilState depthStencil{};
	depthStencil.depthTestEnable = VK_FALSE;
	depthStencil.depthWriteEnable = VK_FALSE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;
	depthStencil.back.compareOp = VK_COMPARE_OP_ALWAYS;
	depthStencil.front = depthStencil.back;

	MultisampleState multisampling{};
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = rasterizationSamples;

	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.pDynamicStates = dynamicStates.data();
	dynamicState.dynamicStateCount = 0;

	// Vertex bindings an attributes based on ImGui vertex definition
	VertexInputState vertexInputState = {
	   {
		   { 0, sizeof(ImDrawVert), VK_VERTEX_INPUT_RATE_VERTEX }
	   },
	   {
			{0, 0, VK_FORMAT_R32G32_SFLOAT,  offsetof(ImDrawVert, pos) },	// Location 0: Position
			{1, 0, VK_FORMAT_R32G32_SFLOAT,  offsetof(ImDrawVert, uv)  },	    // Location 1: UV
			{2, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(ImDrawVert, col) }	// Location 0: Color
	   }
	};
	std::vector<ShaderStage> shaderStages = renderer::createShader(
		device->getDevice(), 
		"data/shaders/ui.vert.spv", 
		"data/shaders/ui.frag.spv"
	);

	pipeline = renderer::createGraphicsPipeline(
		device->getDevice(), 
		pipelineCache,
		shaderStages,
		vertexInputState, 
		inputAssembly, 
		viewport, 
		rasterizer, 
		multisampling, 
		depthStencil, 
		colorBlending, 
		dynamicState, 
		pipelineLayout,
		renderPass);
}

/** Update vertex and index buffer containing the imGui elements when required */
bool Gui::update()
{
	ImDrawData* imDrawData = ImGui::GetDrawData();
	bool updateCmdBuffers = false;

	if (!imDrawData) { return false; };

	// Note: Alignment is done inside buffer creation
	VkDeviceSize vertexBufferSize = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
	VkDeviceSize indexBufferSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

	// Update buffers only if vertex or index count has been changed compared to current buffer size
	if ((vertexBufferSize == 0) || (indexBufferSize == 0)) {
		return false;
	}

	// Vertex buffer
	if ((vertexBuffer.buffer == VK_NULL_HANDLE) || (vertexCount != imDrawData->TotalVtxCount)) {
		vertexBuffer.unmap();
		vertexBuffer.destroy();
		vertexBuffer = buffer::createBuffer(device, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		vertexCount = imDrawData->TotalVtxCount;
		vertexBuffer.unmap();
		vertexBuffer.map();
		updateCmdBuffers = true;
	}

	// Index buffer
	VkDeviceSize indexSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);
	if ((indexBuffer.buffer == VK_NULL_HANDLE) || (indexCount < imDrawData->TotalIdxCount)) {
		indexBuffer.unmap();
		indexBuffer.destroy();
		indexBuffer = buffer::createBuffer(device, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		indexCount = imDrawData->TotalIdxCount;
		indexBuffer.map();
		updateCmdBuffers = true;
	}

	// Upload data
	ImDrawVert* vtxDst = (ImDrawVert*)vertexBuffer.mapped;
	ImDrawIdx* idxDst = (ImDrawIdx*)indexBuffer.mapped;

	for (int n = 0; n < imDrawData->CmdListsCount; n++) {
		const ImDrawList* cmd_list = imDrawData->CmdLists[n];
		memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
		memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
		vtxDst += cmd_list->VtxBuffer.Size;
		idxDst += cmd_list->IdxBuffer.Size;
	}

	// Flush to make writes visible to GPU
	vertexBuffer.flush();
	indexBuffer.flush();

	return updateCmdBuffers;
}

void Gui::draw(const VkCommandBuffer commandBuffer)
{
	ImDrawData* imDrawData = ImGui::GetDrawData();
	int32_t vertexOffset = 0;
	int32_t indexOffset = 0;

	if ((!imDrawData) || (imDrawData->CmdListsCount == 0)) {
		return;
	}

	ImGuiIO& io = ImGui::GetIO();

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);

	pushConstBlock.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
	pushConstBlock.translate = glm::vec2(-1.0f);
	vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstBlock), &pushConstBlock);

	VkDeviceSize offsets[1] = { 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer.buffer, offsets);
	vkCmdBindIndexBuffer(commandBuffer, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT16);

	for (int32_t i = 0; i < imDrawData->CmdListsCount; i++)
	{
		const ImDrawList* cmd_list = imDrawData->CmdLists[i];
		for (int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[j];
			VkRect2D scissorRect;
			scissorRect.offset.x = std::max((int32_t)(pcmd->ClipRect.x), 0);
			scissorRect.offset.y = std::max((int32_t)(pcmd->ClipRect.y), 0);
			scissorRect.extent.width = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
			scissorRect.extent.height = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y);
			vkCmdSetScissor(commandBuffer, 0, 1, &scissorRect);
			vkCmdDrawIndexed(commandBuffer, pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
			indexOffset += pcmd->ElemCount;
		}
		vertexOffset += cmd_list->VtxBuffer.Size;
	}
}

void Gui::resize(uint32_t width, uint32_t height)
{
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2((float)(width), (float)(height));
}

void Gui::freeResources()
{
	ImGui::DestroyContext();
	vertexBuffer.destroy();
	indexBuffer.destroy();
	vkDestroyImageView(device->getDevice(), fontView, nullptr);
	vkDestroyImage(device->getDevice(), fontImage, nullptr);
	vkFreeMemory(device->getDevice(), fontMemory, nullptr);
	vkDestroySampler(device->getDevice(), sampler, nullptr);
	vkDestroyDescriptorSetLayout(device->getDevice(), descriptorSetLayout, nullptr);
	vkDestroyDescriptorPool(device->getDevice(), descriptorPool, nullptr);
	vkDestroyPipelineLayout(device->getDevice(), pipelineLayout, nullptr);
	vkDestroyPipeline(device->getDevice(), pipeline, nullptr);
}

bool Gui::header(const char* caption)
{
	return ImGui::CollapsingHeader(caption, ImGuiTreeNodeFlags_DefaultOpen);
}

bool Gui::checkBox(const char* caption, bool* value)
{
	bool res = ImGui::Checkbox(caption, value);
	if (res) { updated = true; };
	return res;
}

bool Gui::checkBox(const char* caption, int32_t* value)
{
	bool val = (*value == 1);
	bool res = ImGui::Checkbox(caption, &val);
	*value = val;
	if (res) { updated = true; };
	return res;
}

bool Gui::inputFloat(const char* caption, float* value, float step, uint32_t precision)
{
	bool res = ImGui::InputFloat(caption, value, step, step * 10.0f, precision);
	if (res) { updated = true; };
	return res;
}

bool Gui::sliderFloat(const char* caption, float* value, float min, float max)
{
	bool res = ImGui::SliderFloat(caption, value, min, max);
	if (res) { updated = true; };
	return res;
}

bool Gui::sliderInt(const char* caption, int32_t* value, int32_t min, int32_t max)
{
	bool res = ImGui::SliderInt(caption, value, min, max);
	if (res) { updated = true; };
	return res;
}

bool Gui::comboBox(const char* caption, int32_t* itemindex, std::vector<std::string> items)
{
	if (items.empty()) {
		return false;
	}
	std::vector<const char*> charitems;
	charitems.reserve(items.size());
	for (size_t i = 0; i < items.size(); i++) {
		charitems.push_back(items[i].c_str());
	}
	uint32_t itemCount = static_cast<uint32_t>(charitems.size());
	bool res = ImGui::Combo(caption, itemindex, &charitems[0], itemCount, itemCount);
	if (res) { updated = true; };
	return res;
}

bool Gui::button(const char* caption)
{
	bool res = ImGui::Button(caption);
	if (res) { updated = true; };
	return res;
}

void Gui::text(const char* formatstr, ...)
{
	va_list args;
	va_start(args, formatstr);
	ImGui::TextV(formatstr, args);
	va_end(args);
}