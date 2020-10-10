/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#pragma once
#include <vulkan/vulkan.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class VulkanglTFModel
{
public:
	Device* device;
	VkQueue copyQueue;

	/*
		Base glTF structures, see gltfscene sample for details
	*/

	struct Vertices
	{
		VkBuffer       buffer;
		VkDeviceMemory memory;
	} vertices;

	struct Indices
	{
		int            count;
		VkBuffer       buffer;
		VkDeviceMemory memory;
	} indices;

	struct Node;

	struct Material
	{
		glm::vec4 baseColorFactor = glm::vec4(1.0f);
		uint32_t  baseColorTextureIndex;
	};

	struct Image
	{
		TextureObject  texture;
		VkDescriptorSet descriptorSet;
	};

	struct Texture
	{
		int32_t imageIndex;
	};

	struct Primitive
	{
		uint32_t firstIndex;
		uint32_t indexCount;
		int32_t  materialIndex;
	};

	struct Mesh
	{
		std::vector<Primitive> primitives;
	};

	struct Node
	{
		Node* parent;
		uint32_t            index;
		std::vector<Node*> children;
		Mesh                mesh;
		glm::vec3           translation{};
		glm::vec3           scale{ 1.0f };
		glm::quat           rotation{};
		int32_t             skin = -1;
		glm::mat4           matrix;
		glm::mat4           getLocalMatrix();
	};

	struct Vertex
	{
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec2 uv;
		glm::vec3 color;
		glm::vec4 jointIndices;
		glm::vec4 jointWeights;
	};

	/*
		Skin structure
	*/

	struct Skin
	{
		std::string            name;
		Node* skeletonRoot = nullptr;
		std::vector<glm::mat4> inverseBindMatrices;
		std::vector<Node*>     joints;
		Resource<VkBuffer>     ssbo;
		VkDescriptorSet        descriptorSet;
	};

	/*
		Animation related structures
	*/

	struct AnimationSampler
	{
		std::string            interpolation;
		std::vector<float>     inputs;
		std::vector<glm::vec4> outputsVec4;
	};

	struct AnimationChannel
	{
		std::string path;
		Node* node;
		uint32_t    samplerIndex;
	};

	struct Animation
	{
		std::string                   name;
		std::vector<AnimationSampler> samplers;
		std::vector<AnimationChannel> channels;
		float                         start = std::numeric_limits<float>::max();
		float                         end = std::numeric_limits<float>::min();
		float                         currentTime = 0.0f;
	};

	std::vector<Image>     images;
	std::vector<Texture>   textures;
	std::vector<Material>  materials;
	std::vector<Node*>    nodes;
	std::vector<Skin>      skins;
	std::vector<Animation> animations;

	uint32_t activeAnimation = 0;

	~VulkanglTFModel();
	void      loadImages(tinygltf::Model& input);
	void      loadTextures(tinygltf::Model& input);
	void      loadMaterials(tinygltf::Model& input);
	Node* findNode(Node* parent, uint32_t index);
	Node* nodeFromIndex(uint32_t index);
	void      loadSkins(tinygltf::Model& input);
	void      loadAnimations(tinygltf::Model& input);
	void      loadNode(const tinygltf::Node& inputNode, const tinygltf::Model& input, VulkanglTFModel::Node* parent, uint32_t nodeIndex, std::vector<uint32_t>& indexBuffer, std::vector<VulkanglTFModel::Vertex>& vertexBuffer);
	glm::mat4 getNodeMatrix(VulkanglTFModel::Node* node);
	void      updateJoints(VulkanglTFModel::Node* node);
	void      updateAnimation(float deltaTime);
	void      drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, VulkanglTFModel::Node node);
	void      draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout);
};