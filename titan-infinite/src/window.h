/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#pragma once
#include <GLFW/glfw3.h>
#include "camera.h"

const int DisplaySamples = 16;
const float ViewDistance = 5.0f;
const float ViewFOV = 45.0f;
const float OrbitSpeed = 1.0f;
const float ZoomSpeed = 1.0f;

struct Window {
	GLFWwindow* m_window;

    Camera m_camera;
    SceneSettings m_sceneSettings;
    InputMode m_mode = InputMode::None;

    double m_prevCursorX = 0.0;
    double m_prevCursorY = 0.0;

    bool m_framebufferResized = false;

    GLFWwindow* getNativeWindow() { return m_window; }
    bool getWindowShouldClose() { return glfwWindowShouldClose(m_window); }
    bool getFramebufferResized() { return m_framebufferResized; }
    void setFramebufferResized(bool resized) { m_framebufferResized = resized; }

    const Camera& getCamera() { return m_camera; }
    SceneSettings getSceneSettings() { return m_sceneSettings; }

    void create(uint32_t width, uint32_t height) {
        // Create an GLFW window that supports Vulkan rendering.
        if (!glfwInit()) {
            std::cout << "Could not initialize GLFW." << std::endl;
            //return 1;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        m_window = glfwCreateWindow(width, height, "Vulkan Window", nullptr, nullptr);
        if (m_window == NULL) {
            std::cout << "Could not create GLFW window." << std::endl;
        }
        glfwSetWindowUserPointer(m_window, this);
        glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
        glfwSetCursorPosCallback(m_window, mousePositionCallback);
        glfwSetMouseButtonCallback(m_window, mouseButtonCallback);
        glfwSetScrollCallback(m_window, mouseScrollCallback);
        glfwSetKeyCallback(m_window, keyCallback);

        m_camera.distance = ViewDistance;
        m_camera.fov = ViewFOV;
    }

    void destroy() {
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
        app->m_framebufferResized = true;
    }

    static void mousePositionCallback(GLFWwindow* window, double xpos, double ypos) {
        Window* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
        if (self->m_mode != InputMode::None) {
            const double dx = xpos - self->m_prevCursorX;
            const double dy = ypos - self->m_prevCursorY;

            switch (self->m_mode) {
            case InputMode::RotatingScene:
                self->m_sceneSettings.yaw += OrbitSpeed * float(dx);
                self->m_sceneSettings.pitch += OrbitSpeed * float(dy);
                break;
            case InputMode::RotatingCamera:
                self->m_camera.yaw += OrbitSpeed * float(dx);
                self->m_camera.pitch += OrbitSpeed * float(dy);
                break;
            }

            self->m_prevCursorX = xpos;
            self->m_prevCursorY = ypos;
        }
    }
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
        Window* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));

        const InputMode oldMode = self->m_mode;
        if (action == GLFW_PRESS && self->m_mode == InputMode::None) {
            switch (button) {
            case GLFW_MOUSE_BUTTON_1:
                self->m_mode = InputMode::RotatingCamera;
                break;
            case GLFW_MOUSE_BUTTON_2:
                self->m_mode = InputMode::RotatingScene;
                break;
            }
        }
        if (action == GLFW_RELEASE && (button == GLFW_MOUSE_BUTTON_1 || button == GLFW_MOUSE_BUTTON_2)) {
            self->m_mode = InputMode::None;
        }

        if (oldMode != self->m_mode) {
            if (self->m_mode == InputMode::None) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
            else {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                glfwGetCursorPos(window, &self->m_prevCursorX, &self->m_prevCursorY);
            }
        }
    }

    static void mouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
        Window* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
        self->m_camera.distance += ZoomSpeed * float(-yoffset);
        if (self->m_camera.distance < 0.0f)
            self->m_camera.distance = 0.0f;
    }

    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        Window* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    }
};