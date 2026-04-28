module;
#include <array>
#include <deque>
#include <utility>
#include <memory>
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

export module firefly.ecs.engine_components;

import firefly.core.types;

export namespace firefly::ecs {

// ============ Window ============

struct WindowConfig {
    String title = "Firefly Engine";
    u32 width = 1280;
    u32 height = 720;
    bool fullscreen = false;
    bool vsync = true;
    bool resizable = true;
};

struct WindowState {
    GLFWwindow* handle = nullptr;
    u32 width = 0;
    u32 height = 0;
    bool should_close = false;
    bool resized = false;
};

// ============ Input ============

enum class Key : i32 {
    Space = 32,
    Apostrophe = 39, Comma = 44, Minus = 45, Period = 46, Slash = 47,
    Num0 = 48, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    A = 65, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Escape = 256, Enter = 257, Tab = 258, Backspace = 259,
    Right = 262, Left = 263, Down = 264, Up = 265,
    LeftShift = 340, LeftControl = 341, LeftAlt = 342,
    RightShift = 344, RightControl = 345, RightAlt = 346,
};

enum class MouseButton : i32 { Left = 0, Right = 1, Middle = 2 };

struct InputState {
    static constexpr int MAX_KEYS = 350;
    static constexpr int MAX_BUTTONS = 8;

    std::array<bool, MAX_KEYS> current_keys{};
    std::array<bool, MAX_KEYS> previous_keys{};
    std::array<bool, MAX_BUTTONS> current_buttons{};
    std::array<bool, MAX_BUTTONS> previous_buttons{};
    std::pair<f64, f64> mouse_pos{};
    std::pair<f64, f64> mouse_delta{};
    f64 scroll = 0.0;

    auto is_key_down(Key key) const -> bool {
        auto k = static_cast<int>(key);
        return k >= 0 && k < MAX_KEYS && current_keys[k];
    }
    auto is_key_pressed(Key key) const -> bool {
        auto k = static_cast<int>(key);
        return k >= 0 && k < MAX_KEYS && current_keys[k] && !previous_keys[k];
    }
    auto is_key_released(Key key) const -> bool {
        auto k = static_cast<int>(key);
        return k >= 0 && k < MAX_KEYS && !current_keys[k] && previous_keys[k];
    }
    auto is_mouse_down(MouseButton button) const -> bool {
        auto b = static_cast<int>(button);
        return b >= 0 && b < MAX_BUTTONS && current_buttons[b];
    }
};

// ============ Time ============

struct TimeState {
    f32 fps = 0.0f;
    f32 fps_accumulator = 0.0f;
    u32 fps_frame_count = 0;
    u64 total_frame_count = 0;
    f64 total_time = 0.0;
    f32 fixed_delta_time = 1.0f / 60.0f;
    f32 fixed_accumulator = 0.0f;
};

// ============ Render ============

struct RenderConfig {
    bool enable_validation = true;
    bool enable_vsync = true;
    u32 max_frames_in_flight = 2;
    bool enable_render_graph_diagnostics = false;
    u32 render_graph_diagnostics_interval_frames = 240;
    bool enable_screenshot_hotkey = true;
    i32 screenshot_hotkey = 293; // GLFW_KEY_F12
    String screenshot_output_dir = "captures/firefly";
};

// RenderState uses type-erased device pointer to avoid circular dependency.
// The actual RenderDevice is managed by engine_systems.cpp.
struct RenderState {
    struct RetiredGraphEntry {
        SharedPtr<void> graph; // Actually SharedPtr<RenderGraph>
        u64 retire_after_success_frame = 0;
    };

    SharedPtr<void> device;  // Actually SharedPtr<RenderDevice>
    SharedPtr<void> graph;         // Actually SharedPtr<RenderGraph>
    SharedPtr<void> pending_graph; // Actually SharedPtr<RenderGraph>
    std::deque<RetiredGraphEntry> retired_graphs;
    glm::vec4 clear_color{0.1f, 0.1f, 0.1f, 1.0f};
    bool auto_execute_graph = true;
    bool initialized = false;
    u32 retired_graph_keep_frames = 4;
    u64 successful_graph_execute_frames = 0;
    u64 failed_graph_execute_frames = 0;
    u64 graph_swap_count = 0;
    u64 retired_graph_peak = 0;
    u64 diagnostics_log_frame_counter = 0;
};

} // namespace firefly::ecs
