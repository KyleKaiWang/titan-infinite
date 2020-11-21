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

glm::mat4 CCDSolver::getGlobalTransform(unsigned int index)
{
	unsigned int size = (unsigned int)m_IKchain.size();
	glm::mat4 world = m_IKchain[index];
	for (int i = (int)index - 1; i >= 0; --i) {
		world = m_IKchain[i] * world;
	}
	return world;
}

glm::vec3 CCDSolver::getGlobalPosition(unsigned int index)
{
	glm::vec3 pos;
	glm::decompose(m_IKchain[index], glm::vec3(), glm::quat(), pos, glm::vec3(), glm::vec4());
	return pos;
}

glm::quat CCDSolver::getGlobalRotation(unsigned int index)
{
	glm::quat rot;
	glm::decompose(m_IKchain[index], glm::vec3(), rot, glm::vec3(), glm::vec3(), glm::vec4());
	return rot;
}

glm::quat fromTo(const glm::vec3& from, const glm::vec3& to) {
	glm::vec3 f = glm::normalize(from);
	glm::vec3 t = glm::normalize(to);

	if (f == t) {
		return glm::quat();
	}
	else if (f == t * -1.0f) {
		glm::vec3 ortho = glm::vec3(1, 0, 0);
		if (fabsf(f.y) < fabsf(f.x)) {
			ortho = glm::vec3(0, 1, 0);
		}
		if (fabsf(f.z) < fabs(f.y) && fabs(f.z) < fabsf(f.x)) {
			ortho = glm::vec3(0, 0, 1);
		}

		glm::vec3 axis = glm::normalize(glm::cross(f, ortho));
		return glm::quat(0, axis.x, axis.y, axis.z);
	}

	glm::vec3 half = glm::normalize(f + t);
	glm::vec3 axis = glm::cross(f, half);

	return glm::quat(
		dot(f, half),
		axis.x,
		axis.y,
		axis.z
	);
}

bool CCDSolver::solve(const glm::vec3& target)
{
	unsigned int size = this->size();
	if (size == 0) { return false; }
	unsigned int last = size - 1;
	float thresholdSq = m_threshold * m_threshold;
	glm::vec3 goal = target;
	for (unsigned int i = 0; i < m_numSteps; ++i) {
		glm::vec3 effector = getGlobalPosition(last);
		if (glm::length2(goal - effector) < thresholdSq) {
			return true;
		}
		for (int j = (int)size - 2; j >= 0; --j) {
			effector = getGlobalPosition(last);

			glm::vec3 position = getGlobalPosition(j);
			glm::quat rotation = getGlobalRotation(j);

			glm::vec3 toEffector = effector - position;
			glm::vec3 toGoal = goal - position;
			glm::quat effectorToGoal;
			if (glm::length2(toGoal) > 0.00001f) {
				effectorToGoal = fromTo(toEffector, toGoal);
			}

			glm::quat worldRotated = rotation * effectorToGoal;
			glm::quat localRotate = worldRotated * inverse(rotation);
			m_IKchain[j] *= glm::toMat4(localRotate);

			effector = getGlobalPosition(last);
			float dist = glm::length2(goal - effector);
			if (dist < thresholdSq) {
				return true;
			}
		}
	}
	return false;
}