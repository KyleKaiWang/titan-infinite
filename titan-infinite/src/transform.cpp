/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#include "pch.h"
#include "transform.h"
#include <cmath>
#include <iostream>

#define QUAT_EPSILON 0.000001f
#define VEC3_EPSILON 0.000001f

Transform combine(const Transform& a, const Transform& b) {
	Transform out;

	out.scale = a.scale * b.scale;
	out.rotation = b.rotation * a.rotation;

	out.position = a.rotation * (a.scale * b.position);
	out.position = a.position + out.position;

	return out;
}

Transform inverse(const Transform& t) {
	Transform inv;

	inv.rotation = inverse(t.rotation);

	inv.scale.x = fabs(t.scale.x) < VEC3_EPSILON ? 0.0f : 1.0f / t.scale.x;
	inv.scale.y = fabs(t.scale.y) < VEC3_EPSILON ? 0.0f : 1.0f / t.scale.y;
	inv.scale.z = fabs(t.scale.z) < VEC3_EPSILON ? 0.0f : 1.0f / t.scale.z;

	glm::vec3 invTranslation = t.position * -1.0f;
	inv.position = inv.rotation * (inv.scale * invTranslation);

	return inv;
}

Transform mix(const Transform& a, const Transform& b, float t) {
	glm::quat bRot = b.rotation;
	if (dot(a.rotation, bRot) < 0.0f) {
		bRot = -bRot;
	}
	return Transform(
		glm::lerp(a.position, b.position, t),
		nlerp(a.rotation, bRot, t),
		glm::lerp(a.scale, b.scale, t));
}

glm::quat nlerp(const glm::quat& from, const glm::quat& to, float t) {
	return glm::normalize(from + (to - from) * t);
}

bool operator==(const Transform& a, const Transform& b) {
	return a.position == b.position &&
		a.rotation == b.rotation &&
		a.scale == b.scale;
}

bool operator!=(const Transform& a, const Transform& b) {
	return !(a == b);
}

glm::mat4 transformToMat4(const Transform& t) {
	// First, extract the rotation basis of the transform
	glm::vec3 x = t.rotation * glm::vec3(1, 0, 0);
	glm::vec3 y = t.rotation * glm::vec3(0, 1, 0);
	glm::vec3 z = t.rotation * glm::vec3(0, 0, 1);

	// Next, scale the basis vectors
	x = x * t.scale.x;
	y = y * t.scale.y;
	z = z * t.scale.z;

	// Extract the position of the transform
	glm::vec3 p = t.position;

	// Create matrix
	return glm::mat4(
		x.x, x.y, x.z, 0, // X basis (& Scale)
		y.x, y.y, y.z, 0, // Y basis (& scale)
		z.x, z.y, z.z, 0, // Z basis (& scale)
		p.x, p.y, p.z, 1  // Position
	);
}

Transform mat4ToTransform(const glm::mat4& m) {
	Transform out;

	out.position = glm::vec3(m[12], m[13], m[14]);
	out.rotation = glm::toQuat(m);

	glm::mat4 rotScaleMat(
		m[0], m[1], m[2], 0,
		m[4], m[5], m[6], 0,
		m[8], m[9], m[10], 0,
		0, 0, 0, 1
	);
	glm::mat4 invRotMat = glm::toMat4(inverse(out.rotation));
	glm::mat4 scaleSkewMat = rotScaleMat * invRotMat;

	out.scale = glm::vec3(
		scaleSkewMat[0],
		scaleSkewMat[5],
		scaleSkewMat[10]
	);

	return out;
}

glm::vec3 transformPoint(const Transform& a, const glm::vec3& b) {
	glm::vec3 out;

	out = a.rotation * (a.scale * b);
	out = a.position + out;

	return out;
}

glm::vec3 transformVector(const Transform& a, const glm::vec3& b) {
	glm::vec3 out;

	out = a.rotation * (a.scale * b);

	return out;
}