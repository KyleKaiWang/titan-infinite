/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#pragma once
#include "device.h"
#include "buffer.h"
#include <vector>
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

class LineSegment
{
public:
	LineSegment(Device* device)
		:m_device(device), m_origin(glm::vec3(0.0)), m_destination(glm::vec3(0.0)) {
		m_lineWidth = 10.0f;
		initialized = false;

		ubo.mvp = glm::mat4();
		ubo.color = glm::vec4(1.0);
	}

	~LineSegment() {
		vkDestroyDescriptorSetLayout(m_device->getDevice(), m_descriptorSetLayout, nullptr);
		vkDestroyPipelineLayout(m_device->getDevice(), m_pipelineLayout, nullptr);
		vkDestroyPipeline(m_device->getDevice(), m_pipeline, nullptr);
		vkDestroyBuffer(m_device->getDevice(), vertices.buffer, nullptr);
		for (auto buffer : m_uniformBuffers)
			buffer.destroy();
	}


	struct Vertices {
		uint32_t count;
		VkBuffer buffer;
		VkDeviceMemory memory;
	} vertices;

private:
	Device* m_device;
	glm::vec3 m_origin;
	glm::vec3 m_destination;
	float m_lineWidth;
	bool initialized = false;

	struct UBO {
		glm::mat4 mvp;
		glm::vec4 color;
	}ubo;

	std::vector<Buffer> m_uniformBuffers;
	VkDescriptorSetLayout m_descriptorSetLayout;
	std::vector<VkDescriptorSet> m_descriptorsets;
	VkPipelineLayout m_pipelineLayout;
	VkPipeline m_pipeline;
public:
	void init() {
		if (initialized) return;
		assert(m_device->getDescriptorPool() != VK_NULL_HANDLE);

		std::vector<glm::vec3> vertice;
		vertice.push_back(m_origin);
		vertice.push_back(m_destination);
		vertices.count = static_cast<uint32_t>(vertice.size() * sizeof(glm::vec3));

		// Vertex Buffer
		struct StagingBuffer
		{
			VkBuffer buffer;
			VkDeviceMemory memory;
		} vertexStaging;

		buffer::createBuffer(
			m_device,
			vertice.size() * sizeof(glm::vec3),
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			&vertexStaging.buffer,
			&vertexStaging.memory,
			vertice.data());

		buffer::createBuffer(
			m_device,
			vertice.size() * sizeof(glm::vec3),
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			&vertices.buffer,
			&vertices.memory);

		// Copy from staging buffers
		VkCommandBuffer copyCmd = m_device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, m_device->getCommandPool(), true);
		VkBufferCopy copyRegion{};
		copyRegion.size = vertice.size() * sizeof(glm::vec3);
		vkCmdCopyBuffer(copyCmd, vertexStaging.buffer, vertices.buffer, 1, &copyRegion);
		m_device->flushCommandBuffer(copyCmd, m_device->getGraphicsQueue());

		vkDestroyBuffer(m_device->getDevice(), vertices.buffer, nullptr);
		vkFreeMemory(m_device->getDevice(), vertices.memory, nullptr);

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
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
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
		depthStencil.depthTestEnable = VK_FALSE;
		depthStencil.depthWriteEnable = VK_FALSE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
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

	void updateUniformBuffer(Camera* camera, glm::mat4 model = glm::mat4(1.0)) {
		ubo.mvp = camera->matrices.perspective * camera->matrices.view * model;
		ubo.color = glm::vec4(0.0, 1.0, 0.0, 1.0);

		auto imageIndex = m_device->getCurrentFrame();
		memcpy(m_uniformBuffers[imageIndex].mapped, &ubo, sizeof(ubo));
	}

	void updateVertexBuffer(VkCommandBuffer commandBuffer, glm::vec3 origin, glm::vec3 destination) {
		std::array<glm::vec3, 2> arr{ {origin, destination} };
		vkCmdUpdateBuffer(commandBuffer, vertices.buffer, 0, arr.size() * sizeof(glm::vec3), arr.data());
	}

	void updateLineWidth(VkCommandBuffer commandBuffer) {
		vkCmdSetLineWidth(commandBuffer, m_lineWidth);
	}

	void draw(VkCommandBuffer commandBuffer) {
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
		const VkDeviceSize offsets[1] = { 0 };
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, m_descriptorsets.data(), 0, NULL);
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertices.buffer, offsets);
		vkCmdDraw(commandBuffer, vertices.count, 1, 0, 0);
	}
};