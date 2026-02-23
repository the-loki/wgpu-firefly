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
endif()
