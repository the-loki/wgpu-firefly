module;
#include <GLFW/glfw3.h>

module firefly.platform.window;

import firefly.core.logger;

namespace firefly {

static bool s_glfwInitialized = false;

Window::Window(const WindowConfig& config)
    : m_width(config.width), m_height(config.height) {
    if (!s_glfwInitialized) {
        if (!glfwInit()) {
            Logger::fatal("Failed to initialize GLFW");
            return;
        }
        s_glfwInitialized = true;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);

    m_window = glfwCreateWindow(
        static_cast<int>(config.width),
        static_cast<int>(config.height),
        config.title.c_str(),
        config.fullscreen ? glfwGetPrimaryMonitor() : nullptr,
        nullptr
    );

    if (!m_window) {
        Logger::fatal("Failed to create GLFW window");
        return;
    }

    glfwSetWindowUserPointer(m_window, this);
    setup_callbacks();
    Logger::info("Window created: {}x{}", config.width, config.height);
}

Window::~Window() {
    if (m_window) {
        glfwDestroyWindow(m_window);
    }
}

void Window::poll_events() {
    glfwPollEvents();
}

auto Window::should_close() const -> bool {
    return m_window && glfwWindowShouldClose(m_window);
}

void Window::setup_callbacks() {
    glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* win, int w, int h) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
        self->m_width = static_cast<u32>(w);
        self->m_height = static_cast<u32>(h);
        WindowResizeEvent event(self->m_width, self->m_height);
        self->m_events.publish(event);
    });

    glfwSetWindowCloseCallback(m_window, [](GLFWwindow* win) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
        WindowCloseEvent event;
        self->m_events.publish(event);
    });

    glfwSetKeyCallback(m_window, [](GLFWwindow* win, int key, int scancode, int action, int mods) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
        KeyEvent event(key, scancode, action, mods);
        self->m_events.publish(event);
    });

    glfwSetCursorPosCallback(m_window, [](GLFWwindow* win, double x, double y) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
        static double lastX = x, lastY = y;
        MouseMoveEvent event(x, y, x - lastX, y - lastY);
        lastX = x;
        lastY = y;
        self->m_events.publish(event);
    });

    glfwSetMouseButtonCallback(m_window, [](GLFWwindow* win, int button, int action, int mods) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
        MouseButtonEvent event(button, action, mods);
        self->m_events.publish(event);
    });

    glfwSetScrollCallback(m_window, [](GLFWwindow* win, double xoff, double yoff) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
        MouseScrollEvent event(xoff, yoff);
        self->m_events.publish(event);
    });
}

} // namespace firefly
