/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#include "pch.h"
#include "inverse_kinematics.h"
#include <glm/gtx/matrix_decompose.hpp>

CCDSolver::CCDSolver()
{
	m_numSteps = 15;
	m_threshold = 0.00001f;
}

Transform CCDSolver::getGlobalTransform(unsigned int index)
{
	unsigned int size = (unsigned int)m_IKchain.size();
	Transform world = m_IKchain[index];
	for (int i = (int)index - 1; i >= 0; --i) {
		world = combine(m_IKchain[i], world);
	}
	return world;
}

bool CCDSolver::solve(const glm::vec3& target)
{
	unsigned int size = this->size();
	if (size == 0) { return false; }
	unsigned int last = size - 1;
	float thresholdSq = m_threshold * m_threshold;
	glm::vec3 goal = target;
	for (unsigned int i = 0; i < m_numSteps; ++i) {
		glm::vec3 effector = getGlobalTransform(last).position;
		if (glm::length(goal - effector) < thresholdSq) {
			return true;
		}
		for (int j = (int)size - 2; j >= 0; --j) {
			effector = getGlobalTransform(last).position;

			Transform world = getGlobalTransform(j);
			glm::vec3 position = world.position;
			glm::quat rotation = world.rotation;

			glm::vec3 toEffector = effector - position;
			glm::vec3 toGoal = goal - position;
			glm::quat effectorToGoal;
			if (glm::length(toGoal) > 0.00001f) {
				effectorToGoal = toEffector * toGoal;
			}

			glm::quat worldRotated = rotation * effectorToGoal;
			glm::quat localRotate = worldRotated * inverse(rotation);
			m_IKchain[j].rotation = localRotate * m_IKchain[j].rotation;

			effector = getGlobalTransform(last).position;
			if (glm::length(goal - effector) < thresholdSq) {
				return true;
			}
		}
	}
	return false;
}
