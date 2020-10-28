/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#pragma once

#include <vulkan/vulkan.hpp>
#include <optional>
#include <iostream>
#include <fstream>
#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <set>
#include <chrono>
#include <assert.h>

#include <stb_image.h>
#include <stb_image_write.h>

#include <tiny_gltf.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "vkHelpers.h"
#include "device.h"
#include "camera.h"
#include "memory.h"
#include "buffer.h"
#include "renderer.h"
#include "texture.h"
#include "model.h"
#include "skybox.h"
