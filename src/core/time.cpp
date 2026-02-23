module;
#include <chrono>

module firefly.core.time;

namespace firefly {

void Time::init() {
    m_startTime = Clock::now();
    m_lastFrameTime = m_startTime;
    m_deltaTime = 0.0f;
    m_totalTime = 0.0;
    m_frameCount = 0;
    m_fps = 0.0f;
    m_fpsAccumulator = 0.0f;
    m_fpsFrameCount = 0;
    m_fixedAccumulator = 0.0f;
}

void Time::update() {
    auto now = Clock::now();
    auto duration = std::chrono::duration<f32>(now - m_lastFrameTime);
    m_deltaTime = duration.count();
    m_lastFrameTime = now;

    auto totalDuration = std::chrono::duration<f64>(now - m_startTime);
    m_totalTime = totalDuration.count();
    m_frameCount++;

    // FPS 计算 (每秒更新一次)
    m_fpsAccumulator += m_deltaTime;
    m_fpsFrameCount++;
    if (m_fpsAccumulator >= 1.0f) {
        m_fps = static_cast<f32>(m_fpsFrameCount) / m_fpsAccumulator;
        m_fpsAccumulator = 0.0f;
        m_fpsFrameCount = 0;
    }

    // 固定时间步长累积
    m_fixedAccumulator += m_deltaTime;
}

auto Time::should_fixed_update() -> bool {
    if (m_fixedAccumulator >= m_fixedDeltaTime) {
        m_fixedAccumulator -= m_fixedDeltaTime;
        return true;
    }
    return false;
}

} // namespace firefly
