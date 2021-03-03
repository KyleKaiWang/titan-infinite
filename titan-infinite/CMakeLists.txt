cmake_minimum_required (VERSION 3.17 FATAL_ERROR)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_CXX_STANDARD 17)
set(NAME titan-infinite)

project(${NAME})

include_directories(lib)
include_directories(lib/assimp/include)
include_directories(lib/GLFW/include)
include_directories(lib/gli)
include_directories(lib/glm)
include_directories(lib/tinygltf)
include_directories(src)
include_directories(${Vulkan_INCLUDE_DIR})

set(GLFW_LIBRARY ${PROJECT_SOURCE_DIR}/lib/GLFW/x64/GLFW.lib)

if (NOT CMAKE_VERSION VERSION_LESS 3.7.0)
	message(STATUS "Using module to find Vulkan")
	find_package(Vulkan REQUIRED)
endif()

IF (NOT Vulkan_FOUND)
	message(FATAL_ERROR "Could not find Vulkan library!")
ELSE()
	message(STATUS ${Vulkan_LIBRARY})
ENDIF()

# Build
file(GLOB SOURCE src/*.cpp src/imgui/*.cpp)
add_executable(${NAME} src/main.cpp ${SOURCE})
if(WIN32)
	target_link_libraries(${NAME} ${Vulkan_LIBRARY} ${GLFW_LIBRARY} ${WINLIBS})
else(WIN32)
	target_link_libraries(${NAME} ${Vulkan_LIBRARY} ${GLFW_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})
endif(WIN32)