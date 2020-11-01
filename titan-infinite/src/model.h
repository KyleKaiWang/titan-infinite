/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#pragma once

namespace vkglTF {

	extern VkDescriptorSetLayout descriptorSetLayoutImage;
	extern VkDescriptorSetLayout descriptorSetLayoutUbo;
	extern VkMemoryPropertyFlags memoryPropertyFlags;

	struct Node;

	struct BoundingBox {
		glm::vec3 min;
		glm::vec3 max;
		bool valid = false;
		BoundingBox() {};
		BoundingBox(glm::vec3 min, glm::vec3 max) : min(min), max(max) {}
		BoundingBox getAABB(glm::mat4 m);
	};

	/*
		glTF texture sampler
	*/
	struct TextureSampler {
		VkFilter magFilter;
		VkFilter minFilter;
		VkSamplerAddressMode addressModeU;
		VkSamplerAddressMode addressModeV;
		VkSamplerAddressMode addressModeW;
	};

	/*
		glTF material class
	*/
	struct Material {
		Material(Device* device) : device(device) {};
		Device* device;
		enum AlphaMode { ALPHAMODE_OPAQUE, ALPHAMODE_MASK, ALPHAMODE_BLEND };
		AlphaMode alphaMode = ALPHAMODE_OPAQUE;
		float alphaCutoff = 1.0f;
		float metallicFactor = 1.0f;
		float roughnessFactor = 1.0f;
		glm::vec4 baseColorFactor = glm::vec4(1.0f);
		glm::vec4 emissiveFactor = glm::vec4(1.0f);
		TextureObject* baseColorTexture = nullptr;
		TextureObject* metallicRoughnessTexture = nullptr;
		TextureObject* normalTexture = nullptr;
		TextureObject* occlusionTexture = nullptr;
		TextureObject* emissiveTexture = nullptr;
		
		struct TexCoordSets {
			uint8_t baseColor = 0;
			uint8_t metallicRoughness = 0;
			uint8_t specularGlossiness = 0;
			uint8_t normal = 0;
			uint8_t occlusion = 0;
			uint8_t emissive = 0;
		} texCoordSets;

		struct Extension {
			TextureObject* specularGlossinessTexture;
			TextureObject* diffuseTexture;
			glm::vec4 diffuseFactor = glm::vec4(1.0f);
			glm::vec3 specularFactor = glm::vec3(0.0f);
		} extension;

		struct PbrWorkflows {
			bool metallicRoughness = true;
			bool specularGlossiness = false;
		} pbrWorkflows;

		VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	};

	/*
		glTF primitive
	*/
	struct Primitive {
		Primitive(uint32_t firstIndex, uint32_t indexCount, uint32_t vertexCount, Material& material) : firstIndex(firstIndex), indexCount(indexCount), vertexCount(vertexCount), material(material) {
			hasIndices = indexCount > 0;
		};
		uint32_t firstIndex;
		uint32_t indexCount;
		uint32_t firstVertex;
		uint32_t vertexCount;
		Material& material;
		bool hasIndices;

		BoundingBox bb;
		void setBoundingBox(glm::vec3 min, glm::vec3 max);
	};

	/*
		glTF mesh
	*/
	struct Mesh {
		Mesh(Device* device, glm::mat4 matrix);
		~Mesh();
		Device* device;

		std::vector<Primitive*> primitives;
		std::string name;

		BoundingBox bb;
		BoundingBox aabb;

		struct UniformBuffer {
			VkBuffer buffer;
			VkDeviceMemory memory;
			VkDescriptorBufferInfo descriptor;
			VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
			void* mapped;
		} uniformBuffer;

		struct UniformBlock {
			glm::mat4 matrix;
			glm::mat4 jointMatrix[64]{};
			float jointcount{ 0 };
		} uniformBlock;

		void setBoundingBox(glm::vec3 min, glm::vec3 max);
	};

	/*
		glTF skin
	*/
	struct Skin {
		std::string name;
		Node* skeletonRoot = nullptr;
		std::vector<glm::mat4> inverseBindMatrices;
		std::vector<Node*> joints;
	};

	/*
		glTF node
	*/
	struct Node {
		~Node();
		Node* parent;
		uint32_t index;
		std::vector<Node*> children;
		glm::mat4 matrix;
		std::string name;
		Mesh* mesh;
		Skin* skin;
		int32_t skinIndex = -1;
		glm::vec3 translation{};
		glm::vec3 scale{ 1.0f };
		glm::quat rotation{};
		BoundingBox bvh;
		BoundingBox aabb;

		glm::mat4 localMatrix();
		glm::mat4 getMatrix();
		void update();
	};

	/*
		glTF animation channel
	*/
	struct AnimationChannel {
		enum PathType { TRANSLATION, ROTATION, SCALE };
		PathType path;
		Node* node;
		uint32_t samplerIndex;
	};

	/*
		glTF animation sampler
	*/
	struct AnimationSampler {
		enum InterpolationType { LINEAR, STEP, CUBICSPLINE };
		InterpolationType interpolation;
		std::vector<float> inputs;
		std::vector<glm::vec4> outputsVec4;
	};

	/*
		glTF animation
	*/
	struct Animation {
		std::string name;
		std::vector<AnimationSampler> samplers;
		std::vector<AnimationChannel> channels;
		float start = std::numeric_limits<float>::max();
		float end = std::numeric_limits<float>::min();
	};

	/*
		glTF default vertex layout with easy Vulkan mapping functions
	*/
	//enum class VertexComponent { Position, Normal, UV, Color, Tangent, Joint0, Weight0 };

	struct Vertex {
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec2 uv0;
		glm::vec2 uv1;
		glm::vec4 joint0;
		glm::vec4 weight0;
		glm::vec4 tangent;
	};

	enum FileLoadingFlags {
		None = 0x00000000,
		PreTransformVertices = 0x00000001,
		PreMultiplyVertexColors = 0x00000002,
		FlipY = 0x00000004,
		DontLoadImages = 0x00000008
	};

	enum RenderFlags {
		BindImages = 0x00000001
	};

	class VulkanglTFModel {
	private:
		TextureObject* getTexture(uint32_t index);
		TextureObject fromglTfImage(tinygltf::Image& gltfimage, Device* device, VkQueue copyQueue, TextureSampler sampler);
	public:
		VulkanglTFModel();
		~VulkanglTFModel();
		Device* device;
		VkQueue copyQueue;

		struct Vertices {
			int count;
			VkBuffer buffer;
			VkDeviceMemory memory;
		} vertices;
		struct Indices {
			int count;
			VkBuffer buffer;
			VkDeviceMemory memory;
		} indices;

		glm::mat4 aabb;
		std::vector<Node*> nodes;
		std::vector<Node*> linearNodes;
		std::vector<Skin*> skins;
		std::vector<TextureObject> textures;
		std::vector<TextureSampler> textureSamplers;
		std::vector<Material> materials;
		std::vector<Animation> animations;
		std::vector<std::string> extensions;

		struct Dimensions {
			glm::vec3 min = glm::vec3(FLT_MAX);
			glm::vec3 max = glm::vec3(-FLT_MAX);
		} dimensions;

		bool buffersBound = false;
		std::string path;

		void loadFromFile(const std::string& filename, Device* device, VkQueue transferQueue, uint32_t fileLoadingFlags = vkglTF::FileLoadingFlags::None, float scale = 1.0f);
		void loadNode(vkglTF::Node* parent, const tinygltf::Node& node, uint32_t nodeIndex, const tinygltf::Model& model, std::vector<uint32_t>& indexBuffer, std::vector<Vertex>& vertexBuffer, float globalscale);
		void loadSkins(tinygltf::Model& gltfModel);
		void loadTextures(tinygltf::Model& gltfModel, Device* device, VkQueue transferQueue);
		void loadTextureSamplers(tinygltf::Model& gltfModel);
		void loadMaterials(tinygltf::Model& gltfModel);
		void loadAnimations(tinygltf::Model& gltfModel);
		void bindBuffers(VkCommandBuffer commandBuffer);
		void drawNode(Node* node, VkCommandBuffer commandBuffer, uint32_t renderFlags = 0, VkPipelineLayout pipelineLayout = VK_NULL_HANDLE, uint32_t bindImageSet = 1);
		void draw(VkCommandBuffer commandBuffer, uint32_t renderFlags = 0, VkPipelineLayout pipelineLayout = VK_NULL_HANDLE, uint32_t bindImageSet = 1);
		void calculateBoundingBox(Node* node, Node* parent);
		void getSceneDimensions();
		void updateAnimation(uint32_t index, float time);
		Node* findNode(Node* parent, uint32_t index);
		Node* nodeFromIndex(uint32_t index);
		void initNodeDescriptor(vkglTF::Node* node, VkDescriptorSetLayout descriptorSetLayout);
		VkFilter getVkFilterMode(int32_t filterMode);
		VkSamplerAddressMode getVkWrapMode(int32_t wrapMode);
	};
}