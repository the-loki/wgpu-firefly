export module firefly.core.engine;

import firefly.core.types;
import firefly.core.logger;
import firefly.core.time;
import firefly.core.event;

export namespace firefly {

class Engine {
public:
    static auto instance() -> Engine&;

    void init();
    void shutdown();

    auto time() -> Time& { return m_time; }
    auto events() -> EventDispatcher& { return m_events; }

private:
    Engine() = default;
    ~Engine() = default;
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    Time m_time;
    EventDispatcher m_events;
    bool m_initialized = false;
};

} // namespace firefly
