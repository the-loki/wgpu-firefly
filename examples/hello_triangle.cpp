// Firefly Engine - Hello Triangle Example
// Creates a window and renders a colored triangle using WGPU.

import firefly.platform.window;
import firefly.renderer.device;
import firefly.renderer.shader;
import firefly.renderer.pipeline;
import firefly.renderer.command;
import firefly.core.types;
import firefly.core.event;
import firefly.core.logger;

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

static const char* TRIANGLE_SHADER = R"(
@vertex
fn vs_main(@builtin(vertex_index) idx: u32) -> @builtin(position) vec4f {
    var pos = array<vec2f, 3>(
        vec2f( 0.0,  0.5),
        vec2f(-0.5, -0.5),
        vec2f( 0.5, -0.5),
    );
    return vec4f(pos[idx], 0.0, 1.0);
}

@fragment
fn fs_main() -> @location(0) vec4f {
    return vec4f(1.0, 0.5, 0.2, 1.0);
}
)";

int main() {
    firefly::Logger::init();

    firefly::WindowConfig winConfig;
    winConfig.title = "Firefly - Hello Triangle";
    winConfig.width = 800;
    winConfig.height = 600;
    firefly::Window window(winConfig);

    firefly::RenderDevice device;
    auto* hwnd = glfwGetWin32Window(window.native_handle());
    auto result = device.initialize(hwnd);
    if (result.is_error()) {
        return 1;
    }

    device.configure_surface(800, 600);

    // Handle resize
    window.events().subscribe<firefly::WindowResizeEvent>(
        [&device](firefly::WindowResizeEvent& e) {
            if (e.width > 0 && e.height > 0) {
                device.configure_surface(e.width, e.height);
            }
        });

    // Create shader and pipeline
    firefly::ShaderDesc shaderDesc;
    shaderDesc.label = "TriangleShader";
    shaderDesc.wgslCode = TRIANGLE_SHADER;
    auto shader = device.create_shader(shaderDesc);

    firefly::PipelineDesc pipeDesc;
    pipeDesc.label = "TrianglePipeline";
    pipeDesc.vertexShader = shader.get();
    pipeDesc.depthTest = false;
    pipeDesc.cullMode = firefly::CullMode::None;
    auto pipeline = device.create_pipeline(pipeDesc);

    // Main loop
    while (!window.should_close()) {
        window.poll_events();

        auto* cmd = device.begin_frame();
        if (cmd) {
            cmd->begin_render_pass(device.current_texture_view());
            cmd->set_pipeline(*pipeline);
            cmd->draw(3);
            cmd->end_render_pass();
            device.end_frame();
            device.present();
        }
    }

    device.shutdown();
    firefly::Logger::shutdown();
    return 0;
}
