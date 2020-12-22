/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#pragma once
#include <vulkan/vulkan.hpp>

class Skybox {

public:
	Skybox();
	~Skybox();
	void create(Device* device, const std::string& cubeFilename, const std::string& envTextureFilename);
	void destroy();
	void initUniformBuffer();
	void updateUniformBuffer();
	void initDescriptorSet();
	void bindUniformbuffer(uint32_t dstBinding);
	void bindSkyboxTexture(uint32_t dstBinding);
	void initPipelines(const VkRenderPass& renderPass);
	void draw(VkCommandBuffer commandBuffer);

	vkglTF::VulkanglTFModel skyboxModel;
	
	struct skyboxShaderData {
		glm::mat4 skyProjectionMatrix;
	}skyboxShaderData;
	Buffer skyboxUniformBuffer;

	VkDescriptorSetLayout uboDescriptorSetLayout;
	VkDescriptorSetLayout envDescriptorSetLayout;
	VkDescriptorSet uboDescriptorSet;
	VkDescriptorSet envDescriptorSet;

	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
	
private:
	Device* m_device;
	TextureObject m_envCube;
};