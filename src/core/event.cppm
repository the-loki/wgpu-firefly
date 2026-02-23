module;
#include <concepts>
#include <functional>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

export module firefly.core.event;

import firefly.core.types;

export namespace firefly {

struct Event {
    virtual ~Event() = default;
    virtual auto to_string() const -> String = 0;
    bool handled = false;
};

class EventDispatcher {
public:
    template<std::derived_from<Event> T, typename F>
    void subscribe(F&& callback) {
        auto key = std::type_index(typeid(T));
        auto wrapper = [cb = std::forward<F>(callback)](Event& e) {
            cb(static_cast<T&>(e));
        };
        m_handlers[key].push_back(std::move(wrapper));
    }

    template<std::derived_from<Event> T>
    void publish(T& event) {
        auto key = std::type_index(typeid(T));
        auto it = m_handlers.find(key);
        if (it != m_handlers.end()) {
            for (auto& handler : it->second) {
                handler(event);
                if (event.handled) break;
            }
        }
    }

    template<std::derived_from<Event> T>
    void unsubscribe_all() {
        m_handlers.erase(std::type_index(typeid(T)));
    }

    void clear() { m_handlers.clear(); }

private:
    std::unordered_map<std::type_index, std::vector<std::function<void(Event&)>>> m_handlers;
};

struct WindowResizeEvent : Event {
    u32 width, height;
    WindowResizeEvent(u32 w, u32 h) : width(w), height(h) {}
    auto to_string() const -> String override {
        return "WindowResizeEvent(" + std::to_string(width) + ", " + std::to_string(height) + ")";
    }
};

struct WindowCloseEvent : Event {
    auto to_string() const -> String override { return "WindowCloseEvent"; }
};

struct KeyEvent : Event {
    i32 key, scancode, action, mods;
    KeyEvent(i32 k, i32 sc, i32 a, i32 m) : key(k), scancode(sc), action(a), mods(m) {}
    auto to_string() const -> String override {
        return "KeyEvent(key=" + std::to_string(key) + ", action=" + std::to_string(action) + ")";
    }
};

struct MouseMoveEvent : Event {
    f64 x, y, dx, dy;
    MouseMoveEvent(f64 px, f64 py, f64 pdx, f64 pdy) : x(px), y(py), dx(pdx), dy(pdy) {}
    auto to_string() const -> String override {
        return "MouseMoveEvent(" + std::to_string(x) + ", " + std::to_string(y) + ")";
    }
};

struct MouseButtonEvent : Event {
    i32 button, action, mods;
    MouseButtonEvent(i32 b, i32 a, i32 m) : button(b), action(a), mods(m) {}
    auto to_string() const -> String override {
        return "MouseButtonEvent(button=" + std::to_string(button) + ", action=" + std::to_string(action) + ")";
    }
};

struct MouseScrollEvent : Event {
    f64 xOffset, yOffset;
    MouseScrollEvent(f64 x, f64 y) : xOffset(x), yOffset(y) {}
    auto to_string() const -> String override {
        return "MouseScrollEvent(" + std::to_string(xOffset) + ", " + std::to_string(yOffset) + ")";
    }
};

} // namespace firefly
