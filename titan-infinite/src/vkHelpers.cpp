/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#include "pch.h"
#include "vkHelpers.h"

namespace vkHelper {
	std::unordered_map<const char*, bool> device_extensions;
	std::unordered_map<const char*, bool> instance_extensions;
}