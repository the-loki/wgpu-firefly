#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include <flecs.h>
#include <string>

import firefly.ecs.engine_components;
import firefly.ecs.engine_systems;
import firefly.ecs.phases;
import firefly.core.types;

// ============================================================
// ECS Engine Components & Phases Tests
// ============================================================

TEST_SUITE("EngineComponents") {
    TEST_CASE("WindowConfig defaults") {
        firefly::ecs::WindowConfig config;
        CHECK(config.title == "Firefly Engine");
        CHECK(config.width == 1280);
        CHECK(config.height == 720);
        CHECK_FALSE(config.fullscreen);
        CHECK(config.vsync);
        CHECK(config.resizable);
    }

    TEST_CASE("WindowState defaults") {
        firefly::ecs::WindowState state;
        CHECK(state.handle == nullptr);
        CHECK(state.width == 0);
        CHECK(state.height == 0);
        CHECK_FALSE(state.should_close);
        CHECK_FALSE(state.resized);
    }

    TEST_CASE("InputState defaults and queries") {
        firefly::ecs::InputState input;
        CHECK_FALSE(input.is_key_down(firefly::ecs::Key::A));
        CHECK_FALSE(input.is_key_pressed(firefly::ecs::Key::A));
        CHECK_FALSE(input.is_mouse_down(firefly::ecs::MouseButton::Left));

        // Simulate key press
        input.current_keys[65] = true;  // Key::A = 65
        CHECK(input.is_key_down(firefly::ecs::Key::A));
        CHECK(input.is_key_pressed(firefly::ecs::Key::A));

        // After frame swap
        input.previous_keys = input.current_keys;
        CHECK(input.is_key_down(firefly::ecs::Key::A));
        CHECK_FALSE(input.is_key_pressed(firefly::ecs::Key::A));
    }

    TEST_CASE("InputState key released") {
        firefly::ecs::InputState input;
        input.previous_keys[65] = true;
        input.current_keys[65] = false;
        CHECK(input.is_key_released(firefly::ecs::Key::A));
    }

    TEST_CASE("TimeState defaults") {
        firefly::ecs::TimeState time;
        CHECK(time.fps == doctest::Approx(0.0f));
        CHECK(time.total_frame_count == 0);
        CHECK(time.fixed_delta_time == doctest::Approx(1.0f / 60.0f));
    }

    TEST_CASE("RenderConfig defaults") {
        firefly::ecs::RenderConfig config;
        CHECK(config.enable_validation);
        CHECK(config.enable_vsync);
        CHECK(config.max_frames_in_flight == 2);
        CHECK_FALSE(config.enable_render_graph_diagnostics);
        CHECK(config.render_graph_diagnostics_interval_frames == 240);
    }

    TEST_CASE("RenderState defaults") {
        firefly::ecs::RenderState state;
        CHECK(state.device == nullptr);
        CHECK(state.graph == nullptr);
        CHECK(state.pending_graph == nullptr);
        CHECK(state.retired_graphs.empty());
        CHECK(state.auto_execute_graph);
        CHECK(state.retired_graph_keep_frames == 4);
        CHECK(state.successful_graph_execute_frames == 0);
        CHECK(state.failed_graph_execute_frames == 0);
        CHECK(state.graph_swap_count == 0);
        CHECK(state.retired_graph_peak == 0);
        CHECK(state.diagnostics_log_frame_counter == 0);
        CHECK_FALSE(state.initialized);
    }
}

TEST_SUITE("Singleton Components in Flecs") {
    TEST_CASE("set and get singleton") {
        flecs::world world;
        world.set<firefly::ecs::WindowConfig>({
            .title = "Test",
            .width = 640,
            .height = 480,
        });

        const auto* config = world.get<firefly::ecs::WindowConfig>();
        CHECK(config != nullptr);
        CHECK(config->title == "Test");
        CHECK(config->width == 640);
    }

    TEST_CASE("get_mut singleton") {
        flecs::world world;
        world.set<firefly::ecs::InputState>({});

        auto* input = world.get_mut<firefly::ecs::InputState>();
        CHECK(input != nullptr);
        input->current_keys[65] = true;
        CHECK(input->is_key_down(firefly::ecs::Key::A));
    }

    TEST_CASE("multiple singletons") {
        flecs::world world;
        world.set<firefly::ecs::WindowConfig>({});
        world.set<firefly::ecs::InputState>({});
        world.set<firefly::ecs::TimeState>({});
        world.set<firefly::ecs::RenderConfig>({});

        CHECK(world.get<firefly::ecs::WindowConfig>() != nullptr);
        CHECK(world.get<firefly::ecs::InputState>() != nullptr);
        CHECK(world.get<firefly::ecs::TimeState>() != nullptr);
        CHECK(world.get<firefly::ecs::RenderConfig>() != nullptr);
    }
}

TEST_SUITE("Engine Phases") {
    TEST_CASE("register engine phases") {
        flecs::world world;
        firefly::ecs::register_engine_phases(world);

        const auto* phases = world.get<firefly::ecs::EnginePhases>();
        CHECK(phases != nullptr);
        CHECK(phases->poll_events.is_alive());
        CHECK(phases->input_process.is_alive());
        CHECK(phases->render_begin.is_alive());
        CHECK(phases->render_end.is_alive());
    }

    TEST_CASE("phases execute in order") {
        flecs::world world;
        firefly::ecs::register_engine_phases(world);
        const auto* phases = world.get<firefly::ecs::EnginePhases>();

        int order = 0;
        int poll_order = -1, input_order = -1, render_begin_order = -1, render_end_order = -1;

        world.system("TestPoll")
            .kind(phases->poll_events)
            .run([&](flecs::iter&) { poll_order = order++; });

        world.system("TestInput")
            .kind(phases->input_process)
            .run([&](flecs::iter&) { input_order = order++; });

        world.system("TestRenderBegin")
            .kind(phases->render_begin)
            .run([&](flecs::iter&) { render_begin_order = order++; });

        world.system("TestRenderEnd")
            .kind(phases->render_end)
            .run([&](flecs::iter&) { render_end_order = order++; });

        world.progress(0.016f);

        CHECK(input_order < poll_order);
        CHECK(poll_order < render_begin_order);
        CHECK(render_begin_order < render_end_order);
    }
}

TEST_SUITE("Engine RenderGraph Diagnostics") {
    TEST_CASE("runtime stats when render state is missing") {
        flecs::world world;
        const auto stats = firefly::ecs::query_render_graph_runtime_stats(world);
        CHECK_FALSE(stats.hasRenderState);
        CHECK_FALSE(stats.hasActiveGraph);
        CHECK_FALSE(stats.hasPendingGraph);
        CHECK(stats.retiredGraphCount == 0);
    }

    TEST_CASE("runtime stats text includes key fields") {
        flecs::world world;
        world.set<firefly::ecs::RenderState>({});
        const auto text = firefly::ecs::render_graph_runtime_stats_text(world);
        CHECK(text.find("RenderGraphRuntimeStats") != std::string::npos);
        CHECK(text.find("hasRenderState=true") != std::string::npos);
        CHECK(text.find("retiredCount=") != std::string::npos);
        CHECK(text.find("successFrames=") != std::string::npos);
    }

    TEST_CASE("runtime stats text reports concrete counters") {
        flecs::world world;
        world.set<firefly::ecs::RenderState>({});

        auto* state = world.get_mut<firefly::ecs::RenderState>();
        REQUIRE(state != nullptr);
        state->pending_graph = firefly::SharedPtr<int>(new int(7));
        state->retired_graph_keep_frames = 5;
        state->successful_graph_execute_frames = 12;
        state->failed_graph_execute_frames = 2;
        state->graph_swap_count = 3;
        state->retired_graph_peak = 4;
        state->diagnostics_log_frame_counter = 99;

        firefly::ecs::RenderState::RetiredGraphEntry retiredA{};
        retiredA.graph = firefly::SharedPtr<int>(new int(1));
        retiredA.retire_after_success_frame = 14;
        state->retired_graphs.push_back(retiredA);

        firefly::ecs::RenderState::RetiredGraphEntry retiredB{};
        retiredB.graph = firefly::SharedPtr<int>(new int(2));
        retiredB.retire_after_success_frame = 16;
        state->retired_graphs.push_back(retiredB);

        const auto text = firefly::ecs::render_graph_runtime_stats_text(world);
        CHECK(text.find("pending=true") != std::string::npos);
        CHECK(text.find("retiredCount=2") != std::string::npos);
        CHECK(text.find("keepFrames=5") != std::string::npos);
        CHECK(text.find("successFrames=12") != std::string::npos);
        CHECK(text.find("failedFrames=2") != std::string::npos);
        CHECK(text.find("swaps=3") != std::string::npos);
        CHECK(text.find("retiredPeak=4") != std::string::npos);
        CHECK(text.find("diagFrames=99") != std::string::npos);
    }

    TEST_CASE("full debug text reports missing render state") {
        flecs::world world;
        const auto text = firefly::ecs::render_graph_full_debug_text(world);
        CHECK(text.find("RenderGraphRuntimeStats") != std::string::npos);
        CHECK(text.find("no RenderState") != std::string::npos);
    }

    TEST_CASE("full debug text reports missing active graph") {
        flecs::world world;
        world.set<firefly::ecs::RenderState>({});
        const auto text = firefly::ecs::render_graph_full_debug_text(world);
        CHECK(text.find("hasRenderState=true") != std::string::npos);
        CHECK(text.find("no active graph") != std::string::npos);
    }

    TEST_CASE("full debug text includes retired graph details without active graph") {
        flecs::world world;
        world.set<firefly::ecs::RenderState>({});

        auto* state = world.get_mut<firefly::ecs::RenderState>();
        REQUIRE(state != nullptr);
        state->successful_graph_execute_frames = 10;
        state->pending_graph = firefly::SharedPtr<int>(new int(7));

        firefly::ecs::RenderState::RetiredGraphEntry retired{};
        retired.graph = firefly::SharedPtr<int>(new int(3));
        retired.retire_after_success_frame = 13;
        state->retired_graphs.push_back(retired);

        const auto text = firefly::ecs::render_graph_full_debug_text(world);
        CHECK(text.find("RenderGraphQueueState") != std::string::npos);
        CHECK(text.find("RetiredGraph[0]") != std::string::npos);
        CHECK(text.find("retireAfterSuccessFrame=13") != std::string::npos);
        CHECK(text.find("remainingSuccessFrames=3") != std::string::npos);
        CHECK(text.find("no active graph") != std::string::npos);
    }
}
