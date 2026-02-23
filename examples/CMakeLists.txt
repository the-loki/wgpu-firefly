# Firefly 示例程序

# Hello Triangle
add_executable(hello_triangle hello_triangle.cpp)
target_link_libraries(hello_triangle PRIVATE
    firefly_platform
    firefly_renderer
)
if(MSVC)
    target_compile_options(hello_triangle PRIVATE /W4 /utf-8)
endif()

# Copy wgpu DLL
if(WIN32)
    add_custom_command(TARGET hello_triangle POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WGPU_NATIVE_DIR}/lib/wgpu_native.dll"
            $<TARGET_FILE_DIR:hello_triangle>
    )
endif()
