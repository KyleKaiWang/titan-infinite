/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#pragma once

struct TableValue
{
	TableValue(float dist, float point, int index) 
		: distance(dist), pointOnCurve(point), curveIndex(index) {}
	
	float distance;
	float pointOnCurve;
	int curveIndex;
};

class Spline
{
public:
	Spline(Device* device);
	~Spline();

	void init();

	void addControlPoint(glm::vec3 pos);
	void addInterpolationPoint(glm::vec3 pos);
	void drawSpline(VkCommandBuffer commandBuffer);
	void drawControlPoints(VkCommandBuffer commandBuffer);
	glm::vec3 calculateBSpline(glm::mat4 matrix, float t);
	glm::vec3 calculateBSplineDerivative(glm::mat4 matrix, float t);
	TableValue findInTable(float distance);
	void calculateAdaptiveTable(float& t1, float& t2, float& t3);
	void updateUniformBuffer(Camera* camera, glm::mat4 model = glm::mat4(1.0));

private:
	Device* m_device;
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

	std::vector<TableValue> m_arcTable;
	float m_factor;
	glm::vec4 m_splineColor;

public:
	std::vector<glm::vec3> m_controlPoints;
	std::vector<glm::vec3> m_interpolatedPoints;
	std::vector<glm::mat4> m_controlPointsMatrices;

	struct {
		Buffer controlPoints;
		Buffer interpolatedPoints;
	} buffers;
};