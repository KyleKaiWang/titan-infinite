/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */
#pragma once

class IKSolver
{
public:
	IKSolver() {}
	virtual ~IKSolver() {}
};

class CCDSolver : public IKSolver
{
protected:
	std::vector<glm::mat4> m_IKchain;
	unsigned int m_numSteps;
	float m_threshold;
public:
	CCDSolver();
	inline unsigned int size() { return m_IKchain.size(); }
	inline void resize(unsigned int newSize) { m_IKchain.resize(newSize); }
	inline void setIKchain(glm::mat4 matrix, unsigned int index) { m_IKchain[index] = matrix; }
	inline glm::mat4& operator[](unsigned int index) { return m_IKchain[index]; }
	glm::mat4 getGlobalTransform(unsigned int index);
	glm::vec3 getGlobalPosition(unsigned int index);
	glm::quat getGlobalRotation(unsigned int index);

	inline unsigned int getNumSteps() { return m_numSteps; }
	inline void setNumSteps(unsigned int numSteps) { m_numSteps = numSteps; }

	inline float getThreshold() { return m_threshold; }
	inline void setThreshold(float value) { m_threshold = value; }
	bool solve(const glm::vec3& target);
};