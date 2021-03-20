/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Camera
{
	float pitch = 0.0f;
	float yaw = 0.0f;
	//float distance = 5.0;
	float fov = 45.0;
	float znear, zfar;

	enum CameraType { lookat, firstperson };
	CameraType type = CameraType::lookat;

	glm::vec3 rotation = glm::vec3();
	glm::vec3 position = glm::vec3();

	glm::vec3 worldUp = { 0.0f, 1.0f, 0.0f };
	glm::vec3 up = { 0.0f, 1.0f, 0.0f };
	glm::vec3 front = { 0.0f, 0.0f, 1.0f };
	glm::vec3 right = { 1.0f, 0.0f, 0.0f };

	float rotationSpeed = 1.0f;
	float movementSpeed = 100.0f;

	bool updated = false;

	struct
	{
		glm::mat4 perspective;
		glm::mat4 view;
	} matrices;

	struct
	{
		bool left = false;
		bool right = false;
		bool up = false;
		bool down = false;
	} keys;

	bool moving()
	{
		return keys.left || keys.right || keys.up || keys.down;
	}

	float getNearClip() {
		return znear;
	}

	float getFarClip() {
		return zfar;
	}

	void setPerspective(float fov, float aspect, float znear, float zfar)
	{
		this->fov = fov;
		this->znear = znear;
		this->zfar = zfar;
		matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
	};

	void updateAspectRatio(float aspect)
	{
		matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
	}

	void setPosition(glm::vec3 position)
	{
		this->position = position;
		updateViewMatrix();
	}

	void setRotation(glm::vec3 rotation)
	{
		this->rotation = rotation;
		updateViewMatrix();
	};

	void setFront(glm::vec3& _front) {
		front = _front;
		right = glm::normalize(glm::cross(front, worldUp));
		up = glm::normalize(glm::cross(right, front));
	}

	void rotate(glm::vec3 delta)
	{
		this->rotation += delta;
		updateViewMatrix();
	}

	void setTranslation(glm::vec3 translation)
	{
		this->position = translation;
		updateViewMatrix();
	};

	void translate(glm::vec3 delta)
	{
		this->position += delta;
		updateViewMatrix();
	}

	void update(float deltaTime)
	{
		updated = false;
		if (type == CameraType::lookat) {
			if (moving()) {
				glm::vec3 front;
				front.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
				front.y =  sin(glm::radians(rotation.x));
				front.z =  cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
				front = glm::normalize(front);
				setFront(front);

				float moveSpeed = deltaTime * movementSpeed;

				if (keys.up)
					position -= front * moveSpeed;
				if (keys.down)
					position += front * moveSpeed;
				if (keys.left)
					position -= right * moveSpeed;
				if (keys.right)
					position += right * moveSpeed;

				updateViewMatrix();
			}
		}
	};

	void updateViewMatrix()
	{
		glm::mat4 rotM = glm::mat4(1.0f);
		glm::mat4 transM;

		rotM = glm::rotate(rotM, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		rotM = glm::rotate(rotM, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		rotM = glm::rotate(rotM, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		transM = glm::translate(glm::mat4(1.0f), position * glm::vec3(1.0f, 1.0f, -1.0f));

		if (type == CameraType::firstperson)
		{
			matrices.view = rotM * transM;
		}
		else
		{
			matrices.view = transM * rotM;
		}

		updated = true;
	};
};

struct SceneSettings
{
    float pitch = 0.0f;
    float yaw = 0.0f;
};

enum class InputMode
{
    None,
    RotatingCamera,
    RotatingScene,
};