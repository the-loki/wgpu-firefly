module firefly.platform.input;

namespace firefly {

void Input::init() {
    s_currentKeys.fill(false);
    s_previousKeys.fill(false);
    s_currentButtons.fill(false);
    s_previousButtons.fill(false);
    s_mousePos = {0.0, 0.0};
    s_mouseDelta = {0.0, 0.0};
    s_scroll = 0.0;
}

void Input::update() {
    s_previousKeys = s_currentKeys;
    s_previousButtons = s_currentButtons;
    s_mouseDelta = {0.0, 0.0};
    s_scroll = 0.0;
}

auto Input::is_key_down(Key key) -> bool {
    auto k = static_cast<int>(key);
    return k >= 0 && k < MAX_KEYS && s_currentKeys[k];
}

auto Input::is_key_pressed(Key key) -> bool {
    auto k = static_cast<int>(key);
    return k >= 0 && k < MAX_KEYS && s_currentKeys[k] && !s_previousKeys[k];
}

auto Input::is_key_released(Key key) -> bool {
    auto k = static_cast<int>(key);
    return k >= 0 && k < MAX_KEYS && !s_currentKeys[k] && s_previousKeys[k];
}

auto Input::is_mouse_down(MouseButton button) -> bool {
    auto b = static_cast<int>(button);
    return b >= 0 && b < MAX_BUTTONS && s_currentButtons[b];
}

auto Input::mouse_position() -> std::pair<f64, f64> { return s_mousePos; }
auto Input::mouse_delta() -> std::pair<f64, f64> { return s_mouseDelta; }
auto Input::mouse_scroll() -> f64 { return s_scroll; }

void Input::on_key(i32 key, i32 action) {
    if (key >= 0 && key < MAX_KEYS) {
        s_currentKeys[key] = (action != 0); // 0 = GLFW_RELEASE
    }
}

void Input::on_mouse_move(f64 x, f64 y) {
    s_mouseDelta = {x - s_mousePos.first, y - s_mousePos.second};
    s_mousePos = {x, y};
}

void Input::on_mouse_button(i32 button, i32 action) {
    if (button >= 0 && button < MAX_BUTTONS) {
        s_currentButtons[button] = (action != 0);
    }
}

void Input::on_scroll(f64 yoffset) {
    s_scroll = yoffset;
}

} // namespace firefly
