module;
#include <string>
struct GLFWwindow;

export module firefly.platform.window;

import firefly.core.types;
import firefly.core.event;

export namespace firefly {

struct WindowConfig {
    String title = "Firefly Engine";
    u32 width = 1280;
    u32 height = 720;
    bool fullscreen = false;
    bool vsync = true;
    bool resizable = true;
};

class Window {
public:
    explicit Window(const WindowConfig& config);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    void poll_events();
    auto should_close() const -> bool;

    auto width() const -> u32 { return m_width; }
    auto height() const -> u32 { return m_height; }
    auto native_handle() const -> GLFWwindow* { return m_window; }

    auto events() -> EventDispatcher& { return m_events; }

private:
    GLFWwindow* m_window = nullptr;
    u32 m_width = 0;
    u32 m_height = 0;
    EventDispatcher m_events;

    void setup_callbacks();
};

} // namespace firefly
