module;
#include <chrono>

export module firefly.core.time;

import firefly.core.types;

export namespace firefly {

class Time {
public:
    void init();
    void update();

    auto delta_time() const -> f32 { return m_deltaTime; }
    auto total_time() const -> f64 { return m_totalTime; }
    auto frame_count() const -> u64 { return m_frameCount; }
    auto fps() const -> f32 { return m_fps; }

    // 固定时间步长
    auto fixed_delta_time() const -> f32 { return m_fixedDeltaTime; }
    void set_fixed_delta_time(f32 dt) { m_fixedDeltaTime = dt; }
    auto should_fixed_update() -> bool;

private:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;

    TimePoint m_startTime{};
    TimePoint m_lastFrameTime{};
    f32 m_deltaTime = 0.0f;
    f64 m_totalTime = 0.0;
    u64 m_frameCount = 0;

    // FPS 计算
    f32 m_fps = 0.0f;
    f32 m_fpsAccumulator = 0.0f;
    u32 m_fpsFrameCount = 0;

    // 固定时间步长
    f32 m_fixedDeltaTime = 1.0f / 60.0f;
    f32 m_fixedAccumulator = 0.0f;
};

} // namespace firefly
