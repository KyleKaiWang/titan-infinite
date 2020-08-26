/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#pragma once

struct Camera
{
    float pitch = 0.0f;
    float yaw = 0.0f;
    float distance = 5.0;
    float fov = 45.0;
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