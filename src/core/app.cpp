module;
#include <flecs.h>
#include <GLFW/glfw3.h>

module firefly.core.app;

import firefly.core.logger;
import firefly.ecs.engine_components;
import firefly.ecs.phases;
import firefly.ecs.engine_systems;
import firefly.renderer.device;
import firefly.renderer.render_graph;

namespace firefly {

App::App() = default;

App::~App() {
    auto* render = m_world.get_mut<ecs::RenderState>();
    if (render && render->graph) {
        static_cast<RenderGraph*>(render->graph.get())->clear();
        render->graph.reset();
    }
    if (render && render->pending_graph) {
        static_cast<RenderGraph*>(render->pending_graph.get())->clear();
        render->pending_graph.reset();
    }
    if (render) {
        for (auto& retired : render->retired_graphs) {
            if (retired.graph) {
                static_cast<RenderGraph*>(retired.graph.get())->clear();
            }
        }
        render->retired_graphs.clear();
    }

    // Cleanup render device before world destruction
    if (render && render->device) {
        static_cast<RenderDevice*>(render->device.get())->shutdown();
        render->device.reset();
    }

    // Cleanup GLFW window
    auto* win = m_world.get_mut<ecs::WindowState>();
    if (win && win->handle) {
        glfwDestroyWindow(win->handle);
        win->handle = nullptr;
    }
    glfwTerminate();

    Logger::shutdown();
}

void App::configure(const AppConfig& config) {
    m_config = config;
}

void App::setup(std::function<void(flecs::world&)> fn) {
    m_setup_fn = std::move(fn);
}

void App::init_singleton_components() {
    m_world.set<ecs::WindowConfig>({
        .title = m_config.title,
        .width = m_config.width,
        .height = m_config.height,
        .fullscreen = m_config.fullscreen,
        .vsync = m_config.vsync,
        .resizable = m_config.resizable,
    });
    m_world.set<ecs::RenderConfig>({
        .enable_validation = m_config.enable_validation,
        .enable_vsync = m_config.vsync,
        .enable_render_graph_diagnostics = m_config.enable_render_graph_diagnostics,
        .render_graph_diagnostics_interval_frames = m_config.render_graph_diagnostics_interval_frames,
        .enable_screenshot_hotkey = m_config.enable_screenshot_hotkey,
        .screenshot_hotkey = m_config.screenshot_hotkey,
        .screenshot_output_dir = m_config.screenshot_output_dir,
    });
    m_world.set<ecs::WindowState>({});
    m_world.set<ecs::InputState>({});
    m_world.set<ecs::TimeState>({});
    m_world.set<ecs::RenderState>({});
}

void App::register_all_engine_systems() {
    ecs::register_engine_phases(m_world);
    ecs::register_engine_systems(m_world);
}

auto App::run() -> int {
    Logger::init();

    init_singleton_components();
    register_all_engine_systems();

    if (m_setup_fn) {
        m_setup_fn(m_world);
    }

    // Main loop driven by Flecs
    while (true) {
        m_world.progress();
        const auto* win = m_world.get<ecs::WindowState>();
        if (!win) break;
        if (!win->handle) {
            Logger::error("Window handle unavailable, app will exit.");
            return 1;
        }
        if (win->should_close) break;
    }

    return 0;
}

} // namespace firefly
