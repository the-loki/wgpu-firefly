cmake_minimum_required(VERSION 3.28)
project(firefly LANGUAGES CXX)

# C++20 模块支持
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# 导出编译命令 (用于 IDE 集成)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============ 第三方依赖 (third_party) ============

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/glfw)

# Flecs
add_subdirectory(third_party/flecs)

# GLM (header-only)
add_subdirectory(third_party/glm)

# spdlog
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/spdlog)

# nlohmann/json (header-only)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/nlohmann_json)

# wgpu-native (预编译二进制)
set(WGPU_NATIVE_DIR "${CMAKE_SOURCE_DIR}/third_party/wgpu-native")

add_library(wgpu_native SHARED IMPORTED)
if(WIN32)
    set_target_properties(wgpu_native PROPERTIES
        IMPORTED_LOCATION "${WGPU_NATIVE_DIR}/lib/wgpu_native.dll"
        IMPORTED_IMPLIB "${WGPU_NATIVE_DIR}/lib/wgpu_native.dll.lib"
    )
elseif(APPLE)
    set_target_properties(wgpu_native PROPERTIES
        IMPORTED_LOCATION "${WGPU_NATIVE_DIR}/lib/libwgpu_native.dylib"
    )
else()
    set_target_properties(wgpu_native PROPERTIES
        IMPORTED_LOCATION "${WGPU_NATIVE_DIR}/lib/libwgpu_native.so"
    )
endif()
target_include_directories(wgpu_native INTERFACE "${WGPU_NATIVE_DIR}")

# doctest (header-only, 用于测试)
add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE third_party/doctest)

# ============ Firefly 引擎库 ============

add_library(firefly_core STATIC)
target_sources(firefly_core PUBLIC FILE_SET CXX_MODULES FILES
    src/core/core.cppm
    src/core/types.cppm
    src/core/logger.cppm
    src/core/time.cppm
    src/core/event.cppm
    src/core/engine.cppm
)
target_sources(firefly_core PRIVATE
    src/core/logger.cpp
    src/core/time.cpp
    src/core/engine.cpp
)
target_include_directories(firefly_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${WGPU_NATIVE_DIR}
)
target_link_libraries(firefly_core PUBLIC
    spdlog::spdlog
    glm::glm
    nlohmann_json::nlohmann_json
)
if(MSVC)
    target_compile_options(firefly_core PRIVATE /W4 /utf-8)
endif()

# Platform 模块
add_library(firefly_platform STATIC)
target_sources(firefly_platform PUBLIC FILE_SET CXX_MODULES FILES
    src/platform/platform.cppm
    src/platform/window.cppm
    src/platform/input.cppm
    src/platform/file_system.cppm
)
target_sources(firefly_platform PRIVATE
    src/platform/window.cpp
    src/platform/input.cpp
    src/platform/file_system.cpp
)
target_link_libraries(firefly_platform PUBLIC firefly_core glfw)
if(MSVC)
    target_compile_options(firefly_platform PRIVATE /W4 /utf-8)
endif()

# ECS 模块
add_library(firefly_ecs STATIC)
target_sources(firefly_ecs PUBLIC FILE_SET CXX_MODULES FILES
    src/ecs/ecs.cppm
    src/ecs/world.cppm
    src/ecs/components.cppm
    src/ecs/systems.cppm
    src/ecs/engine_components.cppm
    src/ecs/phases.cppm
)
target_sources(firefly_ecs PRIVATE
    src/ecs/world.cpp
    src/ecs/components.cpp
    src/ecs/systems.cpp
    src/ecs/phases.cpp
)
target_include_directories(firefly_ecs PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw/include
)
target_link_libraries(firefly_ecs PUBLIC firefly_core flecs_static glm::glm)
if(MSVC)
    target_compile_options(firefly_ecs PRIVATE /W4 /utf-8)
endif()

# Renderer 模块
add_library(firefly_renderer STATIC)
target_sources(firefly_renderer PUBLIC FILE_SET CXX_MODULES FILES
    src/renderer/render.cppm
    src/renderer/buffer.cppm
    src/renderer/texture.cppm
    src/renderer/sampler.cppm
    src/renderer/shader.cppm
    src/renderer/pipeline.cppm
    src/renderer/command.cppm
    src/renderer/material.cppm
    src/renderer/mesh.cppm
    src/renderer/camera.cppm
    src/renderer/device.cppm
    src/renderer/render_graph.cppm
    src/renderer/forward_renderer.cppm
    src/renderer/renderer.cppm
)
target_sources(firefly_renderer PRIVATE
    src/renderer/buffer.cpp
    src/renderer/texture.cpp
    src/renderer/sampler.cpp
    src/renderer/shader.cpp
    src/renderer/pipeline.cpp
    src/renderer/command.cpp
    src/renderer/material.cpp
    src/renderer/mesh.cpp
    src/renderer/camera.cpp
    src/renderer/device.cpp
    src/renderer/render_graph.cpp
    src/renderer/forward_renderer.cpp
    src/renderer/renderer.cpp
)
target_link_libraries(firefly_renderer PUBLIC firefly_core firefly_ecs wgpu_native glm::glm)
target_compile_definitions(firefly_renderer PRIVATE
    FIREFLY_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
if(MSVC)
    target_compile_options(firefly_renderer PRIVATE /W4 /utf-8)
endif()

# Resource 模块
add_library(firefly_resource STATIC)
target_sources(firefly_resource PUBLIC FILE_SET CXX_MODULES FILES
    src/resource/resource.cppm
    src/resource/manager.cppm
    src/resource/texture_importer.cppm
)
target_sources(firefly_resource PRIVATE
    src/resource/texture_importer.cpp
)
target_include_directories(firefly_resource PRIVATE third_party)
target_link_libraries(firefly_resource PUBLIC firefly_core)
if(MSVC)
    target_compile_options(firefly_resource PRIVATE /W4 /utf-8)
endif()

# Scene 模块
add_library(firefly_scene STATIC)
target_sources(firefly_scene PUBLIC FILE_SET CXX_MODULES FILES
    src/scene/scene.cppm
    src/scene/scene_node.cppm
    src/scene/scene_manager.cppm
)
target_sources(firefly_scene PRIVATE
    src/scene/scene_node.cpp
    src/scene/scene_manager.cpp
)
target_link_libraries(firefly_scene PUBLIC firefly_core firefly_ecs)
if(MSVC)
    target_compile_options(firefly_scene PRIVATE /W4 /utf-8)
endif()

# App 模块 (ECS-driven engine, depends on everything)
add_library(firefly_app STATIC)
target_sources(firefly_app PUBLIC FILE_SET CXX_MODULES FILES
    src/ecs/engine_systems.cppm
    src/core/app.cppm
)
target_sources(firefly_app PRIVATE
    src/ecs/engine_systems.cpp
    src/core/app.cpp
)
target_link_libraries(firefly_app PUBLIC
    firefly_core firefly_ecs firefly_renderer firefly_platform glfw
)
if(MSVC)
    target_compile_options(firefly_app PRIVATE /W4 /utf-8)
endif()

# ============ 测试 ============

option(FIREFLY_BUILD_TESTS "Build tests" ON)
if(FIREFLY_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============ 示例程序 ============

add_subdirectory(examples)
