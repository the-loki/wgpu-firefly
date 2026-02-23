module firefly.core.engine;

namespace firefly {

auto Engine::instance() -> Engine& {
    static Engine s_instance;
    return s_instance;
}

void Engine::init() {
    if (m_initialized) return;
    Logger::init();
    m_time.init();
    Logger::info("Firefly Engine initialized");
    m_initialized = true;
}

void Engine::shutdown() {
    if (!m_initialized) return;
    Logger::info("Firefly Engine shutting down");
    m_events.clear();
    m_initialized = false;
    Logger::shutdown();
}

} // namespace firefly
