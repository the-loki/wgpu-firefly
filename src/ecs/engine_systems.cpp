module;
#include <flecs.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <memory>
#include <sstream>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

module firefly.ecs.engine_systems;

import firefly.core.logger;
import firefly.renderer.device;
import firefly.renderer.render_graph;

namespace firefly::ecs {

// ============ GLFW Callbacks (free functions) ============

static void glfw_framebuffer_size_cb(GLFWwindow* win, int w, int h) {
    auto* raw = static_cast<ecs_world_t*>(glfwGetWindowUserPointer(win));
    flecs::world world(raw);
    auto* state = world.get_mut<WindowState>();
    if (state) {
        state->width = static_cast<u32>(w);
        state->height = static_cast<u32>(h);
        state->resized = true;
    }
}

static void glfw_key_cb(GLFWwindow* win, int key, int, int action, int) {
    auto* raw = static_cast<ecs_world_t*>(glfwGetWindowUserPointer(win));
    flecs::world world(raw);
    auto* input = world.get_mut<InputState>();
    if (input && key >= 0 && key < InputState::MAX_KEYS) {
        input->current_keys[key] = (action != GLFW_RELEASE);
    }
}

static void glfw_cursor_pos_cb(GLFWwindow* win, double x, double y) {
    auto* raw = static_cast<ecs_world_t*>(glfwGetWindowUserPointer(win));
    flecs::world world(raw);
    auto* input = world.get_mut<InputState>();
    if (input) {
        input->mouse_delta.first += x - input->mouse_pos.first;
        input->mouse_delta.second += y - input->mouse_pos.second;
        input->mouse_pos = {x, y};
    }
}

static void glfw_mouse_button_cb(GLFWwindow* win, int button, int action, int) {
    auto* raw = static_cast<ecs_world_t*>(glfwGetWindowUserPointer(win));
    flecs::world world(raw);
    auto* input = world.get_mut<InputState>();
    if (input && button >= 0 && button < InputState::MAX_BUTTONS) {
        input->current_buttons[button] = (action != GLFW_RELEASE);
    }
}

static void glfw_scroll_cb(GLFWwindow* win, double, double yoff) {
    auto* raw = static_cast<ecs_world_t*>(glfwGetWindowUserPointer(win));
    flecs::world world(raw);
    auto* input = world.get_mut<InputState>();
    if (input) {
        input->scroll += yoff;
    }
}

static void glfw_window_close_cb(GLFWwindow* win) {
    auto* raw = static_cast<ecs_world_t*>(glfwGetWindowUserPointer(win));
    flecs::world world(raw);
    auto* state = world.get_mut<WindowState>();
    if (state) {
        state->should_close = true;
    }
}

// ============ Window Systems ============

static bool s_glfw_initialized = false;

static void window_init(flecs::iter& it) {
    auto world = it.world();
    const auto* config = world.get<WindowConfig>();
    if (!config) return;

    if (!s_glfw_initialized) {
        if (!glfwInit()) {
            Logger::fatal("Failed to initialize GLFW");
            if (auto* state = world.get_mut<WindowState>()) {
                state->should_close = true;
            }
            return;
        }
        s_glfw_initialized = true;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, config->resizable ? GLFW_TRUE : GLFW_FALSE);

    auto* glfw_win = glfwCreateWindow(
        static_cast<int>(config->width),
        static_cast<int>(config->height),
        config->title.c_str(),
        config->fullscreen ? glfwGetPrimaryMonitor() : nullptr,
        nullptr
    );

    if (!glfw_win) {
        Logger::fatal("Failed to create GLFW window");
        if (auto* state = world.get_mut<WindowState>()) {
            state->should_close = true;
        }
        return;
    }

    // Store world pointer for GLFW callbacks
    glfwSetWindowUserPointer(glfw_win, world.c_ptr());

    // Setup callbacks
    glfwSetFramebufferSizeCallback(glfw_win, glfw_framebuffer_size_cb);
    glfwSetKeyCallback(glfw_win, glfw_key_cb);
    glfwSetCursorPosCallback(glfw_win, glfw_cursor_pos_cb);
    glfwSetMouseButtonCallback(glfw_win, glfw_mouse_button_cb);
    glfwSetScrollCallback(glfw_win, glfw_scroll_cb);
    glfwSetWindowCloseCallback(glfw_win, glfw_window_close_cb);

    // Update existing singleton in-place (avoid deferred set)
    auto* state = world.get_mut<WindowState>();
    state->handle = glfw_win;
    state->width = config->width;
    state->height = config->height;
    state->should_close = false;
    state->resized = false;

    Logger::info("Window created: {}x{}", config->width, config->height);
}

static void window_poll(flecs::iter& it) {
    auto* state = it.world().get_mut<WindowState>();
    if (!state || !state->handle) return;
    state->resized = false;
    glfwPollEvents();
    state->should_close = glfwWindowShouldClose(state->handle);
}

// ============ Input Systems ============

static void input_process(flecs::iter& it) {
    auto* input = it.world().get_mut<InputState>();
    if (!input) return;
    input->previous_keys = input->current_keys;
    input->previous_buttons = input->current_buttons;
    input->mouse_delta = {0.0, 0.0};
    input->scroll = 0.0;
}

// ============ Time Systems ============

static void time_update(flecs::iter& it) {
    auto* time = it.world().get_mut<TimeState>();
    if (!time) return;
    f32 dt = it.delta_time();
    time->total_frame_count++;
    time->total_time += dt;
    time->fps_accumulator += dt;
    time->fps_frame_count++;
    if (time->fps_accumulator >= 1.0f) {
        time->fps = static_cast<f32>(time->fps_frame_count) / time->fps_accumulator;
        time->fps_accumulator = 0.0f;
        time->fps_frame_count = 0;
    }
    time->fixed_accumulator += dt;
}

// ============ Render Systems ============

// Helper to get RenderDevice* from type-erased SharedPtr<void>
static auto get_device(RenderState* state) -> RenderDevice* {
    return static_cast<RenderDevice*>(state->device.get());
}

static auto get_graph(RenderState* state) -> RenderGraph* {
    return static_cast<RenderGraph*>(state->graph.get());
}

static void render_init(flecs::iter& it) {
    auto world = it.world();
    const auto* win = world.get<WindowState>();
    const auto* config = world.get<RenderConfig>();
    if (!win || !win->handle) return;

    auto* state = world.get_mut<RenderState>();
    if (!state) return;

    auto device = SharedPtr<RenderDevice>(new RenderDevice());
#ifdef _WIN32
    auto* hwnd = glfwGetWin32Window(win->handle);
    auto result = device->initialize(hwnd, DeviceConfig{
        .enableValidation = config ? config->enable_validation : true,
        .enableVSync = config ? config->enable_vsync : true,
    });
#else
    auto result = device->initialize(win->handle, DeviceConfig{});
#endif
    if (result.is_error()) {
        Logger::fatal("Failed to initialize render device: {}", result.error());
        return;
    }
    device->configure_surface(win->width, win->height);
    state->device = SharedPtr<void>(device);
    const u32 configuredFrames =
        (config && config->max_frames_in_flight > 0) ? config->max_frames_in_flight : 2;
    state->retired_graph_keep_frames = configuredFrames + 1;
    state->initialized = true;
    Logger::info("Render device initialized");
}

static void render_begin(flecs::iter& it) {
    auto world = it.world();
    auto* state = world.get_mut<RenderState>();
    if (!state || !state->initialized) return;

    const auto* win = world.get<WindowState>();
    if (win && win->resized && win->width > 0 && win->height > 0) {
        get_device(state)->configure_surface(win->width, win->height);
    }
}

static void render_end(flecs::iter& it) {
    auto world = it.world();
    auto* state = world.get_mut<RenderState>();
    if (!state || !state->initialized || !state->auto_execute_graph) return;

    auto* device = get_device(state);
    if (!device) return;

    auto* graph = get_graph(state);
    bool executeSucceeded = false;
    bool executeFailed = false;
    bool graphSwapped = false;
    if (graph) {
        auto executeResult = graph->execute(*device);
        if (executeResult.is_error()) {
            ++state->failed_graph_execute_frames;
            executeFailed = true;
            Logger::error("RenderGraph execute failed: {}", executeResult.error());
        } else {
            executeSucceeded = true;
        }
    }
    if (executeSucceeded) {
        ++state->successful_graph_execute_frames;
    }

    if (state->pending_graph) {
        const u64 keepFrames = state->retired_graph_keep_frames > 0
            ? static_cast<u64>(state->retired_graph_keep_frames)
            : 1u;
        if (state->graph) {
            RenderState::RetiredGraphEntry retired{};
            retired.graph = state->graph;
            retired.retire_after_success_frame =
                state->successful_graph_execute_frames + keepFrames;
            state->retired_graphs.push_back(std::move(retired));
        }
        state->graph = std::move(state->pending_graph);
        ++state->graph_swap_count;
        graphSwapped = true;
        state->retired_graph_peak = std::max<u64>(
            state->retired_graph_peak,
            static_cast<u64>(state->retired_graphs.size()));
        Logger::debug(
            "RenderGraph swapped: swapCount={}, retiredSize={}, retireAfterSuccessFrame={}",
            state->graph_swap_count,
            state->retired_graphs.size(),
            state->successful_graph_execute_frames + keepFrames);
    }
    while (!state->retired_graphs.empty() &&
        state->retired_graphs.front().retire_after_success_frame <=
        state->successful_graph_execute_frames) {
        state->retired_graphs.pop_front();
    }

    const auto* config = world.get<RenderConfig>();
    if (config && config->enable_render_graph_diagnostics) {
        ++state->diagnostics_log_frame_counter;
        const u32 interval = config->render_graph_diagnostics_interval_frames > 0
            ? config->render_graph_diagnostics_interval_frames
            : 1u;
        const bool intervalHit = (state->diagnostics_log_frame_counter % interval) == 0u;
        if (executeFailed || graphSwapped || intervalHit) {
            Logger::debug("{}", render_graph_full_debug_text(world));
        }
    }
}

auto query_render_graph_runtime_stats(const flecs::world& world) -> RenderGraphRuntimeStats {
    RenderGraphRuntimeStats stats{};
    const auto* state = world.get<RenderState>();
    if (!state) {
        return stats;
    }

    stats.hasRenderState = true;
    stats.hasActiveGraph = (state->graph != nullptr);
    stats.hasPendingGraph = (state->pending_graph != nullptr);
    stats.retiredGraphCount = static_cast<u32>(state->retired_graphs.size());
    stats.retiredGraphKeepFrames = state->retired_graph_keep_frames;
    stats.successfulExecuteFrames = state->successful_graph_execute_frames;
    stats.failedExecuteFrames = state->failed_graph_execute_frames;
    stats.swapCount = state->graph_swap_count;
    stats.retiredGraphPeak = state->retired_graph_peak;
    stats.diagnosticsLogFrameCounter = state->diagnostics_log_frame_counter;
    return stats;
}

auto render_graph_runtime_stats_text(const flecs::world& world) -> String {
    const auto stats = query_render_graph_runtime_stats(world);
    std::ostringstream ss;
    ss << "RenderGraphRuntimeStats"
       << " hasRenderState=" << (stats.hasRenderState ? "true" : "false")
       << " active=" << (stats.hasActiveGraph ? "true" : "false")
       << " pending=" << (stats.hasPendingGraph ? "true" : "false")
       << " retiredCount=" << stats.retiredGraphCount
       << " keepFrames=" << stats.retiredGraphKeepFrames
       << " successFrames=" << stats.successfulExecuteFrames
       << " failedFrames=" << stats.failedExecuteFrames
       << " swaps=" << stats.swapCount
       << " retiredPeak=" << stats.retiredGraphPeak
       << " diagFrames=" << stats.diagnosticsLogFrameCounter;
    return ss.str();
}

auto render_graph_full_debug_text(const flecs::world& world) -> String {
    std::ostringstream ss;
    ss << render_graph_runtime_stats_text(world);

    const auto* state = world.get<RenderState>();
    if (!state) {
        ss << "\nRenderGraph Debug Report unavailable: no RenderState";
        return ss.str();
    }

    ss << "\nRenderGraphQueueState"
       << " active=" << (state->graph ? "true" : "false")
       << " pending=" << (state->pending_graph ? "true" : "false")
       << " retiredCount=" << state->retired_graphs.size();

    if (!state->retired_graphs.empty()) {
        const u64 currentSuccessFrame = state->successful_graph_execute_frames;
        u32 retiredIndex = 0;
        for (const auto& retired : state->retired_graphs) {
            const u64 remainingFrames =
                (retired.retire_after_success_frame > currentSuccessFrame)
                ? (retired.retire_after_success_frame - currentSuccessFrame)
                : 0u;
            ss << "\nRetiredGraph[" << retiredIndex++ << "]"
               << " hasGraph=" << (retired.graph ? "true" : "false")
               << " retireAfterSuccessFrame=" << retired.retire_after_success_frame
               << " remainingSuccessFrames=" << remainingFrames;
        }
    }

    if (!state->graph) {
        ss << "\nRenderGraph Debug Report unavailable: no active graph";
        return ss.str();
    }

    auto* graph = static_cast<RenderGraph*>(state->graph.get());
    if (!graph) {
        ss << "\nRenderGraph Debug Report unavailable: graph pointer cast failed";
        return ss.str();
    }

    auto reportResult = graph->debug_dump_text();
    if (reportResult.is_error()) {
        ss << "\nRenderGraph Debug Report error: " << reportResult.error();
        return ss.str();
    }
    ss << "\n" << reportResult.value();
    return ss.str();
}

// ============ Registration ============

void register_window_systems(flecs::world& world) {
    const auto* phases = world.get<EnginePhases>();

    world.system("WindowInit")
        .kind(flecs::OnStart)
        .run(window_init);

    world.system("WindowPoll")
        .kind(phases->poll_events)
        .run(window_poll);
}

void register_input_systems(flecs::world& world) {
    const auto* phases = world.get<EnginePhases>();

    world.system("InputProcess")
        .kind(phases->input_process)
        .run(input_process);
}

void register_time_systems(flecs::world& world) {
    world.system("TimeUpdate")
        .kind(flecs::PreUpdate)
        .run(time_update);
}

void register_render_systems(flecs::world& world) {
    const auto* phases = world.get<EnginePhases>();

    world.system("RenderInit")
        .kind(flecs::OnStart)
        .run(render_init);

    world.system("RenderBegin")
        .kind(phases->render_begin)
        .run(render_begin);

    world.system("RenderEnd")
        .kind(phases->render_end)
        .run(render_end);
}

void register_engine_systems(flecs::world& world) {
    register_window_systems(world);
    register_input_systems(world);
    register_time_systems(world);
    register_render_systems(world);
}

} // namespace firefly::ecs
