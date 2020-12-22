/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#include "pch.h"
#include "skybox.h"

Skybox::Skybox()
{
}

Skybox::~Skybox()
{
}

void Skybox::create(Device* device, const std::string& cubeFilename, const std::string& envTextureFilename)
{
	m_device = device;
	skyboxModel.loadFromFile(cubeFilename, device, device->getGraphicsQueue());
	m_envCube = texture::loadTextureCube(envTextureFilename, VK_FORMAT_R8G8B8A8_UNORM, m_device, 4);
	initUniformBuffer();
}

void Skybox::destroy()
{
	vkDestroyPipelineLayout(m_device->getDevice(), pipelineLayout, nullptr);
	vkDestroyPipeline(m_device->getDevice(), pipeline, nullptr);
	delete m_device;
}

void Skybox::draw(VkCommandBuffer commandBuffer)
{
	const std::array<VkDescriptorSet, 2> descriptorSets = {
			uboDescriptorSet,
			envDescriptorSet,
	};
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	skyboxModel.draw(commandBuffer);
}

void Skybox::initUniformBuffer()
{
	skyboxUniformBuffer = buffer::createBuffer(
		m_device,
		sizeof(skyboxShaderData),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);
	memory::map(m_device->getDevice(), skyboxUniformBuffer.memory, 0, VK_WHOLE_SIZE, &skyboxUniformBuffer.mapped);
	updateUniformBuffer();
}

void Skybox::updateUniformBuffer()
{
	glm::mat4 projectionMatrix = glm::perspectiveFov(m_device->getWindow()->getCamera()->fov, (float)m_device->getSwapChainExtent().width, (float)m_device->getSwapChainExtent().height, 1.0f, 1000.0f);
	projectionMatrix[1][1] *= -1;
	//const glm::mat4 viewRotationMatrix = glm::eulerAngleXY(glm::radians(m_device->getWindow()->getCamera()->pitch), glm::radians(m_device->getWindow()->getCamera()->yaw));
	const glm::mat4 viewRotationMatrix = m_device->getWindow()->getCamera()->matrices.view;
	skyboxShaderData.skyProjectionMatrix = projectionMatrix * viewRotationMatrix;
	memcpy(skyboxUniformBuffer.mapped, &skyboxShaderData, sizeof(skyboxShaderData));
}

void Skybox::initDescriptorSet()
{
	{
		const std::vector<DescriptorSetLayoutBinding> descriptorSetLayoutBindings = {
				{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr }
		};
		uboDescriptorSetLayout = m_device->createDescriptorSetLayout(m_device->getDevice(), { descriptorSetLayoutBindings });
		uboDescriptorSet = m_device->createDescriptorSet(m_device->getDevice(), m_device->getDescriptorPool(), uboDescriptorSetLayout);
	}
	{
		const std::vector<DescriptorSetLayoutBinding> descriptorSetLayoutBindings = {
			{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &m_envCube.sampler }
		};
		envDescriptorSetLayout = m_device->createDescriptorSetLayout(m_device->getDevice(), { descriptorSetLayoutBindings });
		envDescriptorSet = m_device->createDescriptorSet(m_device->getDevice(), m_device->getDescriptorPool(), envDescriptorSetLayout);
	}
	const std::vector<VkDescriptorSetLayout> pipelineDescriptorSetLayouts = {
			uboDescriptorSetLayout,
			envDescriptorSetLayout
	};
	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushConstantRange.size = sizeof(glm::mat4);
	pushConstantRange.offset = 0;

	pipelineLayout = m_device->createPipelineLayout(m_device->getDevice(), { pipelineDescriptorSetLayouts }, { pushConstantRange });
	bindUniformbuffer(0);
	bindSkyboxTexture(0);
}

void Skybox::bindUniformbuffer(uint32_t dstBinding)
{
	VkWriteDescriptorSet writeDescriptorSet{};
	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.dstSet = uboDescriptorSet;
	writeDescriptorSet.dstBinding = dstBinding;
	writeDescriptorSet.pBufferInfo = &skyboxUniformBuffer.descriptor;
	vkUpdateDescriptorSets(m_device->getDevice(), 1, &writeDescriptorSet, 0, nullptr);
}

void Skybox::bindSkyboxTexture(uint32_t dstBinding)
{
	const VkDescriptorImageInfo skyboxTexture = { VK_NULL_HANDLE, m_envCube.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
	VkWriteDescriptorSet writeDescriptorSet{};
	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.dstSet = envDescriptorSet;
	writeDescriptorSet.dstBinding = dstBinding;
	writeDescriptorSet.pImageInfo = &skyboxTexture;
	vkUpdateDescriptorSets(m_device->getDevice(), 1, &writeDescriptorSet, 0, nullptr);
}

void Skybox::initPipelines(const VkRenderPass& renderPass)
{
	std::vector<ShaderStage> shaderStages_skybox = m_device->createShader(m_device->getDevice(), "data/shaders/skybox.vert.spv", "data/shaders/skybox.frag.spv");

	VertexInputState vertexInputState = {
		
		// VkVertexInputBindingDescription
		{ 
			{ 0, sizeof(vkglTF::Vertex), VK_VERTEX_INPUT_RATE_VERTEX }
		},
		// VkVertexInputAttributeDescription
		{
			{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 } 
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
	rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	MultisampleState multisampling{};
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	DepthStencilState depthStencil{};
	depthStencil.depthTestEnable = VK_FALSE;
	depthStencil.depthWriteEnable = VK_FALSE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;
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
	dynamicState.dynamicStateCount = 0;

	pipeline = m_device->createGraphicsPipeline(m_device->getDevice(), nullptr, shaderStages_skybox, vertexInputState, inputAssembly, viewport, rasterizer, multisampling, depthStencil, colorBlending, dynamicState, pipelineLayout, renderPass);
}
