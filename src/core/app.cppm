module;
#include <flecs.h>
#include <functional>
#include <string>

export module firefly.core.app;

import firefly.core.types;
import firefly.ecs.engine_components;

export namespace firefly {

struct AppConfig {
    String title = "Firefly Engine";
    u32 width = 1280;
    u32 height = 720;
    bool fullscreen = false;
    bool vsync = true;
    bool resizable = true;
    bool enable_validation = true;
    bool enable_render_graph_diagnostics = false;
    u32 render_graph_diagnostics_interval_frames = 240;
};

class App {
public:
    App();
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    void configure(const AppConfig& config);
    void setup(std::function<void(flecs::world&)> fn);
    auto run() -> int;

    auto world() -> flecs::world& { return m_world; }

private:
    flecs::world m_world;
    AppConfig m_config;
    std::function<void(flecs::world&)> m_setup_fn;

    void init_singleton_components();
    void register_all_engine_systems();
};

} // namespace firefly
