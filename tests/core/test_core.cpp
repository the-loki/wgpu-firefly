#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

import firefly.core.types;
import firefly.core.event;
import firefly.core.time;
import firefly.core.logger;
import firefly.core.engine;

#include <string>
#include <thread>
#include <chrono>

// ============================================================
// Phase 2: Core 模块测试
// ============================================================

TEST_SUITE("Core Types") {
    TEST_CASE("type aliases") {
        CHECK(sizeof(firefly::i8) == 1);
        CHECK(sizeof(firefly::i16) == 2);
        CHECK(sizeof(firefly::i32) == 4);
        CHECK(sizeof(firefly::i64) == 8);
        CHECK(sizeof(firefly::u8) == 1);
        CHECK(sizeof(firefly::u16) == 2);
        CHECK(sizeof(firefly::u32) == 4);
        CHECK(sizeof(firefly::u64) == 8);
        CHECK(sizeof(firefly::f32) == 4);
        CHECK(sizeof(firefly::f64) == 8);
    }

    TEST_CASE("Result<T> ok") {
        auto r = firefly::Result<int>::ok(42);
        CHECK(r.is_ok());
        CHECK_FALSE(r.is_error());
        CHECK(r.value() == 42);
    }

    TEST_CASE("Result<T> error") {
        auto r = firefly::Result<int>::error("not found");
        CHECK_FALSE(r.is_ok());
        CHECK(r.is_error());
        CHECK(r.error() == "not found");
    }

    TEST_CASE("Result<void> ok") {
        auto r = firefly::Result<void>::ok();
        CHECK(r.is_ok());
    }

    TEST_CASE("Result<void> error") {
        auto r = firefly::Result<void>::error("failed");
        CHECK(r.is_error());
        CHECK(r.error() == "failed");
    }

    TEST_CASE("ScopeExit") {
        bool called = false;
        {
            firefly::ScopeExit guard([&] { called = true; });
            CHECK_FALSE(called);
        }
        CHECK(called);
    }

    TEST_CASE("Ptr and SharedPtr") {
        auto p = std::make_unique<int>(10);
        CHECK(*p == 10);

        firefly::SharedPtr<int> sp = std::make_shared<int>(20);
        CHECK(*sp == 20);

        firefly::WeakPtr<int> wp = sp;
        CHECK_FALSE(wp.expired());
    }
}

TEST_SUITE("Event System") {
    TEST_CASE("subscribe and publish") {
        firefly::EventDispatcher dispatcher;
        int received_width = 0;

        dispatcher.subscribe<firefly::WindowResizeEvent>([&](firefly::WindowResizeEvent& e) {
            received_width = e.width;
        });

        firefly::WindowResizeEvent event(800, 600);
        dispatcher.publish(event);
        CHECK(received_width == 800);
    }

    TEST_CASE("event handled stops propagation") {
        firefly::EventDispatcher dispatcher;
        int call_count = 0;

        dispatcher.subscribe<firefly::WindowCloseEvent>([&](firefly::WindowCloseEvent& e) {
            call_count++;
            e.handled = true;
        });
        dispatcher.subscribe<firefly::WindowCloseEvent>([&](firefly::WindowCloseEvent&) {
            call_count++;
        });

        firefly::WindowCloseEvent event;
        dispatcher.publish(event);
        CHECK(call_count == 1);
    }

    TEST_CASE("multiple event types") {
        firefly::EventDispatcher dispatcher;
        bool key_received = false;
        bool mouse_received = false;

        dispatcher.subscribe<firefly::KeyEvent>([&](firefly::KeyEvent&) {
            key_received = true;
        });
        dispatcher.subscribe<firefly::MouseMoveEvent>([&](firefly::MouseMoveEvent&) {
            mouse_received = true;
        });

        firefly::KeyEvent ke(65, 0, 1, 0);
        dispatcher.publish(ke);
        CHECK(key_received);
        CHECK_FALSE(mouse_received);

        firefly::MouseMoveEvent me(100.0, 200.0, 1.0, 2.0);
        dispatcher.publish(me);
        CHECK(mouse_received);
    }

    TEST_CASE("unsubscribe_all") {
        firefly::EventDispatcher dispatcher;
        int count = 0;

        dispatcher.subscribe<firefly::WindowCloseEvent>([&](firefly::WindowCloseEvent&) {
            count++;
        });

        firefly::WindowCloseEvent e1;
        dispatcher.publish(e1);
        CHECK(count == 1);

        dispatcher.unsubscribe_all<firefly::WindowCloseEvent>();
        firefly::WindowCloseEvent e2;
        dispatcher.publish(e2);
        CHECK(count == 1);
    }

    TEST_CASE("event to_string") {
        firefly::WindowResizeEvent resize(1920, 1080);
        CHECK(resize.to_string() == "WindowResizeEvent(1920, 1080)");

        firefly::WindowCloseEvent close;
        CHECK(close.to_string() == "WindowCloseEvent");

        firefly::KeyEvent key(65, 0, 1, 0);
        CHECK(key.to_string() == "KeyEvent(key=65, action=1)");
    }
}

TEST_SUITE("Time") {
    TEST_CASE("init and update") {
        firefly::Time time;
        time.init();

        CHECK(time.frame_count() == 0);
        CHECK(time.total_time() == doctest::Approx(0.0).epsilon(0.1));

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        time.update();

        CHECK(time.frame_count() == 1);
        CHECK(time.delta_time() > 0.0f);
        CHECK(time.total_time() > 0.0);
    }

    TEST_CASE("fixed timestep") {
        firefly::Time time;
        time.init();
        time.set_fixed_delta_time(1.0f / 60.0f);

        // Simulate a large frame
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        time.update();

        // Should have at least one fixed update
        CHECK(time.should_fixed_update());
    }
}

TEST_SUITE("Logger") {
    TEST_CASE("init and log") {
        firefly::Logger::init("test", firefly::LogLevel::Trace);
        CHECK_NOTHROW(firefly::Logger::trace("trace msg"));
        CHECK_NOTHROW(firefly::Logger::debug("debug msg: {}", 42));
        CHECK_NOTHROW(firefly::Logger::info("info msg"));
        CHECK_NOTHROW(firefly::Logger::warn("warn msg"));
        CHECK_NOTHROW(firefly::Logger::error("error msg"));
        CHECK_NOTHROW(firefly::Logger::fatal("fatal msg"));
        firefly::Logger::shutdown();
    }

    TEST_CASE("set_level") {
        firefly::Logger::init("test", firefly::LogLevel::Info);
        CHECK_NOTHROW(firefly::Logger::set_level(firefly::LogLevel::Debug));
        CHECK_NOTHROW(firefly::Logger::debug("now visible"));
        firefly::Logger::shutdown();
    }
}

TEST_SUITE("Engine") {
    TEST_CASE("singleton") {
        auto& e1 = firefly::Engine::instance();
        auto& e2 = firefly::Engine::instance();
        CHECK(&e1 == &e2);
    }

    TEST_CASE("init and shutdown") {
        auto& engine = firefly::Engine::instance();
        CHECK_NOTHROW(engine.init());
        CHECK_NOTHROW(engine.shutdown());
    }

    TEST_CASE("time access") {
        auto& engine = firefly::Engine::instance();
        engine.init();
        auto& time = engine.time();
        CHECK(time.frame_count() == 0);
        engine.shutdown();
    }
}
