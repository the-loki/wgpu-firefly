module firefly.core.engine;

namespace firefly {

auto Engine::instance() -> Engine& {
    static Engine s_instance;
    return s_instance;
}

void Engine::init() {
    if (m_initialized) return;
    m_time.init();
    m_initialized = true;
}

void Engine::shutdown() {
    if (!m_initialized) return;
    m_events.clear();
    m_initialized = false;
}

} // namespace firefly
