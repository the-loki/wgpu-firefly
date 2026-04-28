module;
#include <flecs.h>

export module firefly.ecs.engine_systems;

import firefly.core.types;
import firefly.ecs.engine_components;
import firefly.ecs.phases;

export namespace firefly::ecs {

struct RenderGraphRuntimeStats {
    bool hasRenderState = false;
    bool hasActiveGraph = false;
    bool hasPendingGraph = false;
    u32 retiredGraphCount = 0;
    u32 retiredGraphKeepFrames = 0;
    u64 successfulExecuteFrames = 0;
    u64 failedExecuteFrames = 0;
    u64 swapCount = 0;
    u64 retiredGraphPeak = 0;
    u64 diagnosticsLogFrameCounter = 0;
};

[[nodiscard]] auto query_render_graph_runtime_stats(const flecs::world& world)
    -> RenderGraphRuntimeStats;
[[nodiscard]] auto render_graph_runtime_stats_text(const flecs::world& world) -> String;
[[nodiscard]] auto render_graph_full_debug_text(const flecs::world& world) -> String;
[[nodiscard]] auto request_render_screenshot(flecs::world& world, const String& path) -> Result<void>;
[[nodiscard]] auto consume_render_screenshot_error(flecs::world& world) -> String;

// Register all engine systems into the world.
// Requires engine phases and singleton components to be set up first.
void register_engine_systems(flecs::world& world);

// Individual system registration (for fine-grained control)
void register_window_systems(flecs::world& world);
void register_input_systems(flecs::world& world);
void register_time_systems(flecs::world& world);
void register_render_systems(flecs::world& world);

} // namespace firefly::ecs
