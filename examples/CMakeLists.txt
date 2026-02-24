# Firefly 示例程序

# Hello Triangle (ECS-driven)
add_executable(hello_triangle hello_triangle/main.cpp)
target_link_libraries(hello_triangle PRIVATE
    firefly_app
)
if(MSVC)
    target_compile_options(hello_triangle PRIVATE /W4 /utf-8)
endif()

# Forward Sphere (ECS-driven, renderer-only usage)
add_executable(forward_sphere forward_sphere/main.cpp)
target_link_libraries(forward_sphere PRIVATE
    firefly_app
)
if(MSVC)
    target_compile_options(forward_sphere PRIVATE /W4 /utf-8)
endif()
target_compile_definitions(forward_sphere PRIVATE
    FIREFLY_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# Model Viewer (Assimp + ForwardRenderer)
add_executable(model_viewer model_viewer/main.cpp)
target_link_libraries(model_viewer PRIVATE
    firefly_app
    firefly_resource
)
if(MSVC)
    target_compile_options(model_viewer PRIVATE /W4 /utf-8)
endif()
target_compile_definitions(model_viewer PRIVATE
    FIREFLY_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# Sponza Viewer (Assimp + ForwardRenderer)
add_executable(sponza_viewer sponza_viewer/main.cpp)
target_link_libraries(sponza_viewer PRIVATE
    firefly_app
    firefly_resource
)
if(MSVC)
    target_compile_options(sponza_viewer PRIVATE /W4 /utf-8)
endif()
target_compile_definitions(sponza_viewer PRIVATE
    FIREFLY_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# Copy wgpu DLL
if(WIN32)
    add_custom_command(TARGET hello_triangle POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WGPU_NATIVE_DIR}/lib/wgpu_native.dll"
            $<TARGET_FILE_DIR:hello_triangle>
    )
    add_custom_command(TARGET forward_sphere POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WGPU_NATIVE_DIR}/lib/wgpu_native.dll"
            $<TARGET_FILE_DIR:forward_sphere>
    )
    add_custom_command(TARGET model_viewer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WGPU_NATIVE_DIR}/lib/wgpu_native.dll"
            $<TARGET_FILE_DIR:model_viewer>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ASSIMP_NATIVE_DLL}"
            $<TARGET_FILE_DIR:model_viewer>
    )
    add_custom_command(TARGET sponza_viewer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WGPU_NATIVE_DIR}/lib/wgpu_native.dll"
            $<TARGET_FILE_DIR:sponza_viewer>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ASSIMP_NATIVE_DLL}"
            $<TARGET_FILE_DIR:sponza_viewer>
    )
endif()
