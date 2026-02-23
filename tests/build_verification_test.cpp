#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include <concepts>
#include <format>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

// ============================================================
// Phase 1 构建系统验证测试
// 验证: C++20 特性、第三方库集成、编译环境
// ============================================================

TEST_SUITE("Build Verification") {

    TEST_CASE("C++20 concepts") {
        auto is_integral = std::integral<int>;
        CHECK(is_integral);
        CHECK_FALSE(std::integral<float>);
    }

    TEST_CASE("C++20 ranges") {
        std::vector<int> v = {1, 2, 3, 4, 5};
        auto even = v | std::views::filter([](int n) {
            return n % 2 == 0;
        });
        std::vector<int> result(even.begin(), even.end());
        CHECK(result.size() == 2);
        CHECK(result[0] == 2);
        CHECK(result[1] == 4);
    }

    TEST_CASE("C++20 std::format") {
        auto s = std::format("Hello, {}!", "Firefly");
        CHECK(s == "Hello, Firefly!");
    }

    TEST_CASE("GLM math library") {
        glm::vec3 v(1.0f, 2.0f, 3.0f);
        CHECK(v.x == doctest::Approx(1.0f));
        CHECK(v.y == doctest::Approx(2.0f));
        CHECK(v.z == doctest::Approx(3.0f));

        glm::mat4 identity = glm::mat4(1.0f);
        CHECK(identity[0][0] == doctest::Approx(1.0f));
        CHECK(identity[1][1] == doctest::Approx(1.0f));

        auto proj = glm::perspective(glm::radians(45.0f), 16.0f / 9.0f, 0.1f, 100.0f);
        CHECK(proj[0][0] != doctest::Approx(0.0f));
    }

    TEST_CASE("spdlog logging") {
        CHECK_NOTHROW(spdlog::info("Build verification test running"));
        CHECK_NOTHROW(spdlog::debug("Debug message: {}", 42));
    }

    TEST_CASE("nlohmann/json") {
        nlohmann::json j;
        j["engine"] = "Firefly";
        j["version"] = 1;
        j["features"] = {"ecs", "rendering", "scripting"};

        CHECK(j["engine"] == "Firefly");
        CHECK(j["version"] == 1);
        CHECK(j["features"].size() == 3);

        auto s = j.dump();
        auto parsed = nlohmann::json::parse(s);
        CHECK(parsed == j);
    }
}
