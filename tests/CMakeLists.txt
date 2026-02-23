# Firefly 测试配置

# Phase 1: 构建系统验证测试
add_executable(test_build_verification
    build/build_verification_test.cpp
)
target_link_libraries(test_build_verification PRIVATE
    doctest
    glm::glm
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)
if(MSVC)
    target_compile_options(test_build_verification PRIVATE /W4 /utf-8)
endif()
add_test(NAME build_verification COMMAND test_build_verification)

# Phase 2: Core 模块测试
add_executable(test_core core/test_core.cpp)
target_link_libraries(test_core PRIVATE doctest firefly_core)
if(MSVC)
    target_compile_options(test_core PRIVATE /W4 /utf-8)
endif()
add_test(NAME core COMMAND test_core)

# Phase 3: Platform 模块测试 (Input + FileSystem, 不需要窗口)
add_executable(test_platform platform/test_platform.cpp)
target_link_libraries(test_platform PRIVATE doctest firefly_platform)
if(MSVC)
    target_compile_options(test_platform PRIVATE /W4 /utf-8)
endif()
add_test(NAME platform COMMAND test_platform)

# Phase 4: ECS 模块测试
add_executable(test_ecs ecs/test_ecs.cpp)
target_link_libraries(test_ecs PRIVATE doctest firefly_ecs)
if(MSVC)
    target_compile_options(test_ecs PRIVATE /W4 /utf-8)
endif()
add_test(NAME ecs COMMAND test_ecs)

# ECS Engine Components & Phases 测试
add_executable(test_ecs_engine ecs/test_ecs_engine.cpp)
target_link_libraries(test_ecs_engine PRIVATE doctest firefly_ecs firefly_app)
if(MSVC)
    target_compile_options(test_ecs_engine PRIVATE /W4 /utf-8)
endif()
add_test(NAME ecs_engine COMMAND test_ecs_engine)

# Phase 5: Renderer 模块测试 (不需要 GPU 的部分)
add_executable(test_renderer renderer/test_renderer.cpp)
target_link_libraries(test_renderer PRIVATE doctest firefly_renderer)
if(MSVC)
    target_compile_options(test_renderer PRIVATE /W4 /utf-8)
endif()
add_test(NAME renderer COMMAND test_renderer)

# Copy wgpu DLL for renderer tests
if(WIN32)
    add_custom_command(TARGET test_renderer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WGPU_NATIVE_DIR}/lib/wgpu_native.dll"
            $<TARGET_FILE_DIR:test_renderer>
    )
endif()

# Phase 6: Resource 模块测试
add_executable(test_resource resource/test_resource.cpp)
target_link_libraries(test_resource PRIVATE doctest firefly_resource)
if(MSVC)
    target_compile_options(test_resource PRIVATE /W4 /utf-8)
endif()
add_test(NAME resource COMMAND test_resource)

# Phase 7: Scene 模块测试
add_executable(test_scene scene/test_scene.cpp)
target_link_libraries(test_scene PRIVATE doctest firefly_scene)
if(MSVC)
    target_compile_options(test_scene PRIVATE /W4 /utf-8)
endif()
add_test(NAME scene COMMAND test_scene)

# Phase 8: 集成测试
add_executable(test_integration integration/test_integration.cpp)
target_link_libraries(test_integration PRIVATE
    doctest
    firefly_core
    firefly_platform
    firefly_ecs
    firefly_renderer
    firefly_resource
    firefly_scene
)
if(MSVC)
    target_compile_options(test_integration PRIVATE /W4 /utf-8)
endif()
add_test(NAME integration COMMAND test_integration)

if(WIN32)
    add_custom_command(TARGET test_integration POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WGPU_NATIVE_DIR}/lib/wgpu_native.dll"
            $<TARGET_FILE_DIR:test_integration>
    )
endif()
