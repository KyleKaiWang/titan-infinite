/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

class CCDSolver 
{
protected:
	std::vector<Transform> m_IKchain;
	unsigned int m_numSteps;
	float m_threshold;
public:
	CCDSolver();
	inline unsigned int size() { return m_IKchain.size(); }
	inline void resize(unsigned int newSize) { m_IKchain.resize(newSize); }

	inline Transform& operator[](unsigned int index) { return m_IKchain[index]; }
	Transform getGlobalTransform(unsigned int index);

	inline unsigned int getNumSteps() { return m_numSteps; }
	inline void setNumSteps(unsigned int numSteps) { m_numSteps = numSteps; }

	inline float getThreshold() { return m_threshold; }
	inline void setThreshold(float value) { m_threshold = value; }
	bool solve(const glm::vec3& target);
};