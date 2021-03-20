/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#pragma once
#include <GLFW/glfw3.h>
#include "camera.h"
#include "timer.h"

//const int DisplaySamples = 16;
//const float ViewDistance = 5.0f;
//const float ViewFOV = 45.0f;
const float OrbitSpeed = 1.0f;
const float ZoomSpeed = 1.0f;

struct Window {
	GLFWwindow* m_window;

    Camera* m_camera;
    SceneSettings m_sceneSettings;
    InputMode m_mode = InputMode::None;

    uint32_t m_screen_width;
    uint32_t m_screen_height;

    double m_prevCursorX = 0.0;
    double m_prevCursorY = 0.0;
    double m_currCursorX = 0.0;
    double m_currCursorY = 0.0;
    float m_currMouseScrolOffsetX = 0;
    float m_currMouseScrolOffsetY = 0;
    float m_prevMouseScrolOffsetX = 0;
    float m_prevMouseScrolOffsetY = 0;

    bool m_framebufferResized = false;

    GLFWwindow* getNativeWindow() { return m_window; }
    uint32_t getWidth() { return m_screen_width; }
    uint32_t getHeight() { return m_screen_height; }
    bool getWindowShouldClose() { return glfwWindowShouldClose(m_window); }
    bool getFramebufferResized() { return m_framebufferResized; }
    void setFramebufferResized(bool resized) { m_framebufferResized = resized; }

    void setCamera(Camera* camera) { m_camera = camera; }
    Camera* getCamera() { return m_camera; }
    SceneSettings getSceneSettings() { return m_sceneSettings; }
    std::pair<double, double> getCurCursorPos() { return { m_currCursorX, m_currCursorY }; }

    void create(uint32_t width, uint32_t height) {
        
        // Should call SetCamera() from outside first
        assert(m_camera);

        // Create an GLFW window that supports Vulkan rendering.
        if (!glfwInit()) {
            std::cout << "Could not initialize GLFW." << std::endl;
            //return 1;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        m_screen_width = width;
        m_screen_height = height;

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
            self->m_currCursorX = xpos;
            self->m_currCursorY = ypos;
            const double dx = xpos - self->m_prevCursorX;
            const double dy = ypos - self->m_prevCursorY;

            switch (self->m_mode) {
            case InputMode::RotatingScene:
                self->m_sceneSettings.yaw += OrbitSpeed * float(dx);
                self->m_sceneSettings.pitch += OrbitSpeed * float(dy);

                self->m_camera->rotation.x += dy * self->m_camera->rotationSpeed;
                self->m_camera->rotation.y -= dx * self->m_camera->rotationSpeed;
                self->m_camera->rotation += glm::vec3(dy * self->m_camera->rotationSpeed, -dx * self->m_camera->rotationSpeed, 0.0f);
                self->m_camera->updateViewMatrix();
                break;
            case InputMode::RotatingCamera:
                self->m_camera->yaw += OrbitSpeed * float(dx);
                self->m_camera->pitch += OrbitSpeed * float(dy);
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
                self->m_mode = InputMode::RotatingScene;
                break;
            case GLFW_MOUSE_BUTTON_2:
                self->m_mode = InputMode::RotatingCamera;
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
        //self->m_camera->distance += ZoomSpeed * float(-yoffset);
        //if (self->m_camera->distance < 0.0f)
        //    self->m_camera->distance = 0.0f;

        self->m_currMouseScrolOffsetX = float(xoffset);
        self->m_currMouseScrolOffsetY = float(yoffset);

        self->m_camera->position += self->m_currMouseScrolOffsetY * self->m_camera->front * self->m_camera->movementSpeed;
        self->m_camera->updateViewMatrix();

        self->m_prevMouseScrolOffsetX = self->m_currMouseScrolOffsetX;
        self->m_prevMouseScrolOffsetY = self->m_currMouseScrolOffsetY;
    }

    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        Window* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            self->m_camera->keys.up = true;
        }
        else {
            self->m_camera->keys.up = false;
        }

        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            self->m_camera->keys.down = true;
        }
        else {
            self->m_camera->keys.down = false;
        }

        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            self->m_camera->keys.left = true;
        }
        else {
            self->m_camera->keys.left = false;
        }

        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            self->m_camera->keys.right = true;
        }
        else {
            self->m_camera->keys.right = false;
        }
    }
};