module;
#include <array>
#include <utility>

export module firefly.platform.input;

import firefly.core.types;

export namespace firefly {

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

class Input {
public:
    static void init();
    static void update();

    static auto is_key_down(Key key) -> bool;
    static auto is_key_pressed(Key key) -> bool;
    static auto is_key_released(Key key) -> bool;

    static auto is_mouse_down(MouseButton button) -> bool;
    static auto mouse_position() -> std::pair<f64, f64>;
    static auto mouse_delta() -> std::pair<f64, f64>;
    static auto mouse_scroll() -> f64;

    // Called by Window callbacks
    static void on_key(i32 key, i32 action);
    static void on_mouse_move(f64 x, f64 y);
    static void on_mouse_button(i32 button, i32 action);
    static void on_scroll(f64 yoffset);

private:
    static constexpr int MAX_KEYS = 350;
    static constexpr int MAX_BUTTONS = 8;
    static inline std::array<bool, MAX_KEYS> s_currentKeys{};
    static inline std::array<bool, MAX_KEYS> s_previousKeys{};
    static inline std::array<bool, MAX_BUTTONS> s_currentButtons{};
    static inline std::array<bool, MAX_BUTTONS> s_previousButtons{};
    static inline std::pair<f64, f64> s_mousePos{};
    static inline std::pair<f64, f64> s_mouseDelta{};
    static inline f64 s_scroll = 0.0;
};

} // namespace firefly
