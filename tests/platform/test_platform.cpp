#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

import firefly.platform.input;
import firefly.platform.file_system;
import firefly.core.types;

#include <string>
#include <filesystem>

// ============================================================
// Phase 3: Platform 模块测试
// ============================================================

TEST_SUITE("Input") {
    TEST_CASE("init resets state") {
        firefly::Input::init();
        CHECK_FALSE(firefly::Input::is_key_down(firefly::Key::A));
        CHECK_FALSE(firefly::Input::is_mouse_down(firefly::MouseButton::Left));
        auto [mx, my] = firefly::Input::mouse_position();
        CHECK(mx == doctest::Approx(0.0));
        CHECK(my == doctest::Approx(0.0));
    }

    TEST_CASE("key press and release") {
        firefly::Input::init();

        firefly::Input::on_key(static_cast<int>(firefly::Key::A), 1);
        CHECK(firefly::Input::is_key_down(firefly::Key::A));
        CHECK(firefly::Input::is_key_pressed(firefly::Key::A));

        firefly::Input::update();
        CHECK(firefly::Input::is_key_down(firefly::Key::A));
        CHECK_FALSE(firefly::Input::is_key_pressed(firefly::Key::A));

        firefly::Input::on_key(static_cast<int>(firefly::Key::A), 0);
        CHECK_FALSE(firefly::Input::is_key_down(firefly::Key::A));
        CHECK(firefly::Input::is_key_released(firefly::Key::A));
    }

    TEST_CASE("mouse move") {
        firefly::Input::init();
        firefly::Input::on_mouse_move(100.0, 200.0);
        auto [x, y] = firefly::Input::mouse_position();
        CHECK(x == doctest::Approx(100.0));
        CHECK(y == doctest::Approx(200.0));
    }

    TEST_CASE("mouse button") {
        firefly::Input::init();
        firefly::Input::on_mouse_button(0, 1);
        CHECK(firefly::Input::is_mouse_down(firefly::MouseButton::Left));
        firefly::Input::on_mouse_button(0, 0);
        CHECK_FALSE(firefly::Input::is_mouse_down(firefly::MouseButton::Left));
    }

    TEST_CASE("scroll") {
        firefly::Input::init();
        firefly::Input::on_scroll(3.0);
        CHECK(firefly::Input::mouse_scroll() == doctest::Approx(3.0));
        firefly::Input::update();
        CHECK(firefly::Input::mouse_scroll() == doctest::Approx(0.0));
    }
}

TEST_SUITE("FileSystem") {
    TEST_CASE("write and read text") {
        std::string testPath = "build/test_output.txt";
        std::string content = "Hello Firefly!";

        auto writeResult = firefly::FileSystem::write_file(
            testPath, content.data(), content.size());
        CHECK(writeResult.is_ok());

        auto readResult = firefly::FileSystem::read_text(testPath);
        CHECK(readResult.is_ok());
        CHECK(readResult.value() == content);

        std::filesystem::remove(testPath);
    }

    TEST_CASE("write and read binary") {
        std::string testPath = "build/test_binary.bin";
        std::vector<firefly::u8> data = {0x00, 0xFF, 0x42, 0xAB};

        auto writeResult = firefly::FileSystem::write_file(
            testPath, data.data(), data.size());
        CHECK(writeResult.is_ok());

        auto readResult = firefly::FileSystem::read_file(testPath);
        CHECK(readResult.is_ok());
        CHECK(readResult.value() == data);

        std::filesystem::remove(testPath);
    }

    TEST_CASE("exists and is_file") {
        std::string testPath = "build/test_exists.txt";
        std::string content = "test";
        firefly::FileSystem::write_file(testPath, content.data(), content.size());

        CHECK(firefly::FileSystem::exists(testPath));
        CHECK(firefly::FileSystem::is_file(testPath));
        CHECK_FALSE(firefly::FileSystem::is_directory(testPath));

        std::filesystem::remove(testPath);
        CHECK_FALSE(firefly::FileSystem::exists(testPath));
    }

    TEST_CASE("read nonexistent file") {
        auto result = firefly::FileSystem::read_text("nonexistent_file.txt");
        CHECK(result.is_error());
    }

    TEST_CASE("asset path resolution") {
        firefly::FileSystem::set_asset_root("res/");
        auto path = firefly::FileSystem::resolve_asset_path("textures/test.png");
        CHECK(path == "res/textures/test.png");

        firefly::FileSystem::set_asset_root("assets");
        path = firefly::FileSystem::resolve_asset_path("model.gltf");
        CHECK(path == "assets/model.gltf");
    }
}
