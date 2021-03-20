/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */


#include "pch.h"
#include "spline.h"
#include <stack>
#include <iostream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/compatibility.hpp>
#include <glm/gtc/type_ptr.hpp>

Spline::Spline(Device* device)
	: m_device(device), m_lineWidth(10.0f), m_splineColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)), m_controlPointSize(10.0f), m_descriptorSetLayout(VK_NULL_HANDLE), m_pipelineLayout(VK_NULL_HANDLE), m_pipeline(VK_NULL_HANDLE)
{
	ubo.mvp = glm::mat4(1.0f);
	ubo.color = m_splineColor;
}

Spline::~Spline()
{
	vkDestroyDescriptorSetLayout(m_device->getDevice(), m_descriptorSetLayout, nullptr);
	vkDestroyPipelineLayout(m_device->getDevice(), m_pipelineLayout, nullptr);
	vkDestroyPipeline(m_device->getDevice(), m_pipeline, nullptr);
	vkDestroyBuffer(m_device->getDevice(), buffers.controlPoints.buffer, nullptr);
	vkDestroyBuffer(m_device->getDevice(), buffers.interpolatedPoints.buffer, nullptr);
	for (auto buffer : m_uniformBuffers)
		buffer.destroy();
	buffers.controlPoints.destroy();
	buffers.interpolatedPoints.destroy();
}

void Spline::init()
{
	if (initialized) return;
	assert(m_device->getDescriptorPool() != VK_NULL_HANDLE);

	struct StagingBuffer
	{
		VkBuffer buffer;
		VkDeviceMemory memory;
	} vertexStaging_control, vertexStaging_interpolated;

	// Vertex Buffer - Control Points
	buffer::createBuffer(
		m_device,
		m_controlPoints.size() * sizeof(glm::vec3),
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		&vertexStaging_control.buffer,
		&vertexStaging_control.memory,
		m_controlPoints.data());
	
	buffer::createBuffer(
		m_device,
		m_controlPoints.size() * sizeof(glm::vec3),
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		&buffers.controlPoints.buffer,
		&buffers.controlPoints.memory);

	// Vertex Buffer - Interpolated Points
	buffer::createBuffer(
		m_device,
		m_interpolatedPoints.size() * sizeof(glm::vec3),
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		&vertexStaging_interpolated.buffer,
		&vertexStaging_interpolated.memory,
		m_interpolatedPoints.data());

	buffer::createBuffer(
		m_device,
		m_interpolatedPoints.size() * sizeof(glm::vec3),
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		&buffers.interpolatedPoints.buffer,
		&buffers.interpolatedPoints.memory);

	// Copy from staging buffers
	VkCommandBuffer copyCmd_contorl = m_device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	VkBufferCopy copyRegion_contorl{};
	copyRegion_contorl.size = m_controlPoints.size() * sizeof(glm::vec3);
	vkCmdCopyBuffer(copyCmd_contorl, vertexStaging_control.buffer, buffers.controlPoints.buffer, 1, &copyRegion_contorl);
	m_device->flushCommandBuffer(copyCmd_contorl, m_device->getGraphicsQueue());

	// Copy from staging buffers
	VkCommandBuffer copyCmd_interpolated = m_device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	VkBufferCopy copyRegion_interpolated{};
	copyRegion_interpolated.size = m_interpolatedPoints.size() * sizeof(glm::vec3);
	vkCmdCopyBuffer(copyCmd_interpolated, vertexStaging_interpolated.buffer, buffers.interpolatedPoints.buffer, 1, &copyRegion_interpolated);
	m_device->flushCommandBuffer(copyCmd_interpolated, m_device->getGraphicsQueue());

	vkDestroyBuffer(m_device->getDevice(), vertexStaging_control.buffer, nullptr);
	vkFreeMemory(m_device->getDevice(), vertexStaging_control.memory, nullptr);
	vkDestroyBuffer(m_device->getDevice(), vertexStaging_interpolated.buffer, nullptr);
	vkFreeMemory(m_device->getDevice(), vertexStaging_interpolated.memory, nullptr);
	// Uniform Buffer
	m_uniformBuffers.resize(m_device->getSwapChainimages().size());
	for (auto& uniformBuffer : m_uniformBuffers) {
		uniformBuffer = buffer::createBuffer(
			m_device,
			sizeof(ubo),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		);
		memory::map(m_device->getDevice(), uniformBuffer.memory, 0, uniformBuffer.bufferSize, &uniformBuffer.mapped);
	}

	// Descriptor Set 
	{
		std::vector<DescriptorSetLayoutBinding> layoutBindings = {
			{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr }
		};
		m_descriptorSetLayout = m_device->createDescriptorSetLayout(m_device->getDevice(), { layoutBindings });
	}

	m_descriptorsets.resize(m_device->getSwapChainimages().size());
	for (auto i = 0; i < m_descriptorsets.size(); ++i) {
		m_descriptorsets[i] = m_device->createDescriptorSet(m_device->getDevice(), m_device->getDescriptorPool(), m_descriptorSetLayout);
		std::array<VkWriteDescriptorSet, 1> writeDescriptorSets{};

		writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescriptorSets[0].descriptorCount = 1;
		writeDescriptorSets[0].dstSet = m_descriptorsets[i];
		writeDescriptorSets[0].dstBinding = 0;
		writeDescriptorSets[0].pBufferInfo = &m_uniformBuffers[i].descriptor;
		vkUpdateDescriptorSets(m_device->getDevice(), static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
	}

	// Pipeline Layout
	VertexInputState vertexInputState = {
		{
			{ 0, sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_VERTEX }
		},
		{
			{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}
		}
	};

	// Graphics Pipeline
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
	rasterizer.lineWidth = m_lineWidth;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	MultisampleState multisampling{};
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	DepthStencilState depthStencil{};
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;
	depthStencil.front = depthStencil.back;
	depthStencil.back.compareOp = VK_COMPARE_OP_ALWAYS;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_SUBTRACT;
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	

	std::vector<VkPipelineColorBlendAttachmentState> attachments{ colorBlendAttachment };

	ColorBlendState colorBlending{};
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachments = attachments;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_LINE_WIDTH };
	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.pDynamicStates = dynamicStates.data();
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());;

	m_pipelineLayout = m_device->createPipelineLayout(m_device->getDevice(), { m_descriptorSetLayout }, {});
	std::vector<ShaderStage> shaderStages_mesh = m_device->createShader(m_device->getDevice(), "../../data/shaders/debug_draw.vert.spv", "../../data/shaders/debug_draw.frag.spv");
	m_pipeline = m_device->createGraphicsPipeline(m_device->getDevice(), m_device->getPipelineCache(), shaderStages_mesh, vertexInputState, inputAssembly, viewport, rasterizer, multisampling, depthStencil, colorBlending, dynamicState, m_pipelineLayout, m_device->getRenderPass());

	initialized = true;
}

void Spline::addControlPoint(glm::vec3 pos)
{
	m_controlPoints.push_back(pos);
}

void Spline::addControlPointMatrix(glm::mat4 mat)
{
	m_controlPointsMatrices.push_back(mat);
}

void Spline::addInterpolationPoint(glm::vec3 pos)
{
	m_interpolatedPoints.push_back(pos);
}

void Spline::drawSpline(VkCommandBuffer commandBuffer)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, m_descriptorsets.data(), 0, NULL);
	const VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &buffers.interpolatedPoints.buffer, offsets);
	vkCmdDraw(commandBuffer, m_interpolatedPoints.size(), 1, 0, 0);
}

void Spline::drawControlPoints(VkCommandBuffer commandBuffer)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, m_descriptorsets.data(), 0, NULL);
	const VkDeviceSize controlPointsOffsets[] = { 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &buffers.controlPoints.buffer, controlPointsOffsets);
	vkCmdDraw(commandBuffer, m_controlPoints.size(), 1, 0, 0);
}

glm::vec3 Spline::calculateBSpline(glm::mat4 matrix, float t)
{
	// Uniform B-Spline
	glm::mat4 bsplineMatrix;
	bsplineMatrix[0] = glm::vec4(-1.0f, 3.0f, -3.0f, 1.0f);
	bsplineMatrix[1] = glm::vec4(3.0f, -6.0f, 3.0f, 0.0f);
	bsplineMatrix[2] = glm::vec4(-3.0f, 0.0f, 3.0f, 0.0f);
	bsplineMatrix[3] = glm::vec4(1.0f, 4.0f, 1.0f, 0.0f);

	bsplineMatrix /= 6;
	bsplineMatrix = glm::transpose(bsplineMatrix);

	glm::vec4 pos = glm::vec4(t * t * t, t * t, t, 1.0f) * bsplineMatrix * matrix;
	return glm::vec3(pos.x, pos.y, pos.z);
}

glm::vec3 Spline::calculateBSplineDerivative(glm::mat4 matrix, float t)
{
	glm::mat4 bsplineMatrix;
	bsplineMatrix[0] = glm::vec4(-1.0f, 3.0f, -3.0f, 1.0f);
	bsplineMatrix[1] = glm::vec4(3.0f, -6.0f, 3.0f, 0.0f);
	bsplineMatrix[2] = glm::vec4(-3.0f, 0.0f, 3.0f, 0.0f);
	bsplineMatrix[3] = glm::vec4(1.0f, 4.0f, 1.0f, 0.0f);

	bsplineMatrix /= 6;
	bsplineMatrix = glm::transpose(bsplineMatrix);

	glm::vec4 pos = glm::vec4(3 * t * t, 2 * t, 1.0, 0.0f) * bsplineMatrix * matrix;
	return glm::vec3(pos.x, pos.y, pos.z);
}

TableValue Spline::findInTable(float distance)
{
	for (unsigned int i = 0; i < m_arcTable.size() - 1; ++i) {
		float point, alpha;
		float dist1, dist2;
		int curveIndex;
		if ((m_arcTable[i].distance < distance) && (m_arcTable[i + 1].distance > distance)) {
			alpha = (distance - m_arcTable[i].distance) / (m_arcTable[i + 1].distance - m_arcTable[i].distance);
			if (m_arcTable[i].curveIndex != m_arcTable[i + 1].curveIndex) {
				dist1 = 0.0f;
				dist2 = m_arcTable[i + 1].pointOnCurve;
				point = glm::lerp(dist1, dist2, alpha);
				curveIndex = m_arcTable[i + 1].curveIndex;
			}
			else {
				dist1 = m_arcTable[i + 1].pointOnCurve;
				dist2 = m_arcTable[i].pointOnCurve;
				point = glm::lerp(dist2, dist1, alpha);
				curveIndex = m_arcTable[i].curveIndex;
			}
			return TableValue(m_arcTable[i].distance, point, curveIndex);
		}
	}
	return TableValue(0.0f, 0.0f, 0);
}

void Spline::calculateAdaptiveTable(float& t1, float& t2, float& t3)
{
	float tolerance = 0.1;
	float alpha = 0.001f;
	m_arcTable.emplace_back(TableValue(0.0f, 0.0f, 0));

	for (unsigned int i = 0; i < m_controlPointsMatrices.size(); ++i) {
		std::stack<TableValue> segmentStack;
		segmentStack.push(TableValue(0.0f, 1.0f, i));

		while (segmentStack.size() > 0) {
			TableValue stackTop = segmentStack.top();
			segmentStack.pop();

			int curveIndex = stackTop.curveIndex;
			float s_a = stackTop.distance;
			float s_b = stackTop.pointOnCurve;
			float s_mid = (s_a + s_b) / 2.0f;

			glm::vec3 P_sa = calculateBSpline(m_controlPointsMatrices[curveIndex], s_a);
			glm::vec3 P_sb = calculateBSpline(m_controlPointsMatrices[curveIndex], s_b);
			glm::vec3 P_sm = calculateBSpline(m_controlPointsMatrices[curveIndex], s_mid);

			float A = glm::length(P_sm - P_sa);
			float B = glm::length(P_sb - P_sm);
			float C = glm::length(P_sb - P_sa);

			float d = A + B - C;

			if (d < tolerance && s_b - s_a > alpha) {
				int previousLength = m_arcTable[m_arcTable.size() - 1].distance;
				m_arcTable.emplace_back(TableValue(previousLength + A, s_mid, curveIndex));
				m_arcTable.emplace_back(TableValue(previousLength + A + B, s_b, curveIndex));
			}
			else {
				segmentStack.push(TableValue(s_mid, s_b, curveIndex));
				segmentStack.push(TableValue(s_a, s_mid, curveIndex));
			}
		}
	}

	t3 = m_arcTable[m_arcTable.size() - 1].distance / 6;
	t1 = 0.3f * t3; // ramp up
	t2 = 0.9f * t3; // ramp down
	t3 += (t1 + (t3 - t2));
}

void Spline::updateUniformBuffer(Camera* camera, glm::mat4 model, bool controlPoint) {
	ubo.mvp = camera->matrices.perspective * camera->matrices.view * model;
	ubo.color = m_splineColor;
	ubo.controlPoint = controlPoint;

	auto imageIndex = m_device->getCurrentFrame();
	memcpy(m_uniformBuffers[imageIndex].mapped, &ubo, sizeof(ubo));
}
