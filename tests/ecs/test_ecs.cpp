#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include <flecs.h>

import firefly.ecs.world;
import firefly.ecs.components;
import firefly.ecs.systems;
import firefly.core.types;

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>

// ============================================================
// Phase 4: ECS 模块测试
// ============================================================

TEST_SUITE("Components") {
    TEST_CASE("Transform default values") {
        firefly::ecs::Transform t;
        CHECK(t.position == glm::vec3(0.0f));
        CHECK(t.scale == glm::vec3(1.0f));
        CHECK(t.rotation == glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    }

    TEST_CASE("Transform model matrix - identity") {
        firefly::ecs::Transform t;
        auto mat = t.model_matrix();
        auto identity = glm::mat4(1.0f);
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                CHECK(mat[i][j] == doctest::Approx(identity[i][j]));
    }

    TEST_CASE("Transform model matrix - translation") {
        firefly::ecs::Transform t;
        t.position = glm::vec3(1.0f, 2.0f, 3.0f);
        auto mat = t.model_matrix();
        CHECK(mat[3][0] == doctest::Approx(1.0f));
        CHECK(mat[3][1] == doctest::Approx(2.0f));
        CHECK(mat[3][2] == doctest::Approx(3.0f));
    }

    TEST_CASE("Transform model matrix - scale") {
        firefly::ecs::Transform t;
        t.scale = glm::vec3(2.0f, 3.0f, 4.0f);
        auto mat = t.model_matrix();
        CHECK(mat[0][0] == doctest::Approx(2.0f));
        CHECK(mat[1][1] == doctest::Approx(3.0f));
        CHECK(mat[2][2] == doctest::Approx(4.0f));
    }

    TEST_CASE("NameComponent") {
        firefly::ecs::NameComponent nc;
        nc.name = "TestEntity";
        CHECK(nc.name == "TestEntity");
    }

    TEST_CASE("CameraComponent defaults") {
        firefly::ecs::CameraComponent cam;
        CHECK(cam.fov == doctest::Approx(60.0f));
        CHECK(cam.near_plane == doctest::Approx(0.1f));
        CHECK(cam.far_plane == doctest::Approx(1000.0f));
        CHECK_FALSE(cam.is_primary);
    }

    TEST_CASE("LightComponent defaults") {
        firefly::ecs::LightComponent light;
        CHECK(light.type == firefly::ecs::LightType::Directional);
        CHECK(light.intensity == doctest::Approx(1.0f));
        CHECK(light.color == glm::vec3(1.0f));
    }

    TEST_CASE("MeshComponent defaults") {
        firefly::ecs::MeshComponent mesh;
        CHECK(mesh.mesh_id == 0);
        CHECK(mesh.material_id == 0);
        CHECK(mesh.visible);
    }
}

TEST_SUITE("World") {
    TEST_CASE("create and destroy entity") {
        firefly::ecs::World world;
        auto e = world.create_entity("TestEntity");
        CHECK(e.is_alive());
        world.destroy_entity(e);
        CHECK_FALSE(e.is_alive());
    }

    TEST_CASE("add and get component") {
        firefly::ecs::World world;
        auto e = world.create_entity("Player");

        world.add_component<firefly::ecs::Transform>(e);
        CHECK(world.has_component<firefly::ecs::Transform>(e));

        auto* t = world.get_component<firefly::ecs::Transform>(e);
        CHECK(t != nullptr);
        t->position = glm::vec3(5.0f, 10.0f, 0.0f);
        CHECK(t->position.x == doctest::Approx(5.0f));
    }

    TEST_CASE("remove component") {
        firefly::ecs::World world;
        auto e = world.create_entity();
        world.add_component<firefly::ecs::Transform>(e);
        CHECK(world.has_component<firefly::ecs::Transform>(e));

        world.remove_component<firefly::ecs::Transform>(e);
        CHECK_FALSE(world.has_component<firefly::ecs::Transform>(e));
    }

    TEST_CASE("multiple components on entity") {
        firefly::ecs::World world;
        auto e = world.create_entity("Camera");

        world.add_component<firefly::ecs::Transform>(e);
        world.add_component<firefly::ecs::CameraComponent>(e);

        CHECK(world.has_component<firefly::ecs::Transform>(e));
        CHECK(world.has_component<firefly::ecs::CameraComponent>(e));

        auto* cam = world.get_component<firefly::ecs::CameraComponent>(e);
        cam->is_primary = true;
        CHECK(cam->is_primary);
    }

    TEST_CASE("world progress") {
        firefly::ecs::World world;
        // Should not crash
        world.progress(0.016f);
        world.progress(0.016f);
    }

    TEST_CASE("named entity") {
        firefly::ecs::World world;
        auto e = world.create_entity("MyEntity");
        CHECK(e.is_alive());
    }
}

TEST_SUITE("Systems") {
    TEST_CASE("register all systems") {
        firefly::ecs::World world;
        // Should not crash
        firefly::ecs::register_all_systems(world);
        world.progress(0.016f);
    }

    TEST_CASE("transform system normalizes rotation") {
        firefly::ecs::World world;
        firefly::ecs::register_transform_system(world);

        auto e = world.create_entity();
        world.add_component<firefly::ecs::Transform>(e);
        auto* t = world.get_component<firefly::ecs::Transform>(e);
        // Set unnormalized quaternion
        t->rotation = glm::quat(2.0f, 0.0f, 0.0f, 0.0f);

        world.progress(0.016f);

        t = world.get_component<firefly::ecs::Transform>(e);
        float len = glm::length(t->rotation);
        CHECK(len == doctest::Approx(1.0f).epsilon(0.01));
    }

    TEST_CASE("script system calls on_update") {
        firefly::ecs::World world;
        firefly::ecs::register_script_system(world);

        int callCount = 0;
        auto e = world.create_entity();
        world.add_component<firefly::ecs::NativeScriptComponent>(e);
        auto* script = world.get_component<firefly::ecs::NativeScriptComponent>(e);
        script->on_update = [&callCount](float) { callCount++; };

        world.progress(0.016f);
        CHECK(callCount == 1);

        world.progress(0.016f);
        CHECK(callCount == 2);
    }
}
