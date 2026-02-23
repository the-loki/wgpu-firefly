#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include <flecs.h>

import firefly.core.types;
import firefly.core.event;
import firefly.core.time;
import firefly.ecs.world;
import firefly.ecs.components;
import firefly.ecs.systems;
import firefly.scene.scene_manager;
import firefly.scene.scene_node;
import firefly.resource.manager;
import firefly.renderer.mesh;
import firefly.renderer.camera;
import firefly.renderer.material;
import firefly.platform.input;
import firefly.platform.file_system;

#include <glm/glm.hpp>
#include <filesystem>

// ============================================================
// Phase 8: 集成测试 - 跨模块协作
// ============================================================

TEST_SUITE("Integration: ECS + Scene") {
    TEST_CASE("scene nodes have ECS components") {
        firefly::ecs::World world;
        firefly::Scene scene(world, "TestLevel");

        auto* player = scene.create_node("Player");
        player->set_position(10.0f, 0.0f, 5.0f);
        player->add_component<firefly::ecs::CameraComponent>();

        auto* cam = player->get_component<firefly::ecs::CameraComponent>();
        CHECK(cam != nullptr);
        cam->is_primary = true;
        CHECK(cam->is_primary);

        auto [x, y, z] = player->get_position();
        CHECK(x == doctest::Approx(10.0f));
    }

    TEST_CASE("scene with systems") {
        firefly::ecs::World world;
        firefly::ecs::register_transform_system(world);

        firefly::Scene scene(world, "SystemTest");
        auto* node = scene.create_node("Entity");
        auto* t = node->get_component<firefly::ecs::Transform>();
        t->rotation = glm::quat(2.0f, 0.0f, 0.0f, 0.0f);

        scene.update(0.016f);

        t = node->get_component<firefly::ecs::Transform>();
        auto len = glm::length(glm::vec4(t->rotation.w, t->rotation.x, t->rotation.y, t->rotation.z));
        CHECK(len == doctest::Approx(1.0f).epsilon(0.01));
    }
}

TEST_SUITE("Integration: Scene + SceneManager") {
    TEST_CASE("multiple scenes with shared world") {
        firefly::ecs::World world;
        firefly::SceneManager mgr;

        auto* level1 = mgr.create_scene(world, "Level1");
        auto* level2 = mgr.create_scene(world, "Level2");

        level1->create_node("Player1");
        level2->create_node("Player2");

        CHECK(level1->find_node("Player1") != nullptr);
        CHECK(level2->find_node("Player2") != nullptr);

        mgr.set_current_scene(level2);
        CHECK(mgr.current_scene()->name() == "Level2");
    }
}

TEST_SUITE("Integration: Renderer types") {
    TEST_CASE("mesh with camera") {
        firefly::Mesh mesh;
        std::vector<firefly::Vertex> verts = {
            {glm::vec3(0, 0, 0)},
            {glm::vec3(1, 0, 0)},
            {glm::vec3(0, 1, 0)},
        };
        mesh.set_vertices(verts);
        mesh.set_indices({0, 1, 2});
        mesh.compute_bounds();

        firefly::Camera cam;
        cam.set_perspective(60.0f, 16.0f / 9.0f, 0.1f, 100.0f);
        cam.look_at(glm::vec3(0, 0, 5), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

        auto vp = cam.view_projection();
        CHECK(vp != glm::mat4(0.0f));
        CHECK(mesh.vertices().size() == 3);
    }
}

TEST_SUITE("Integration: Event + Input") {
    TEST_CASE("event dispatch with input") {
        firefly::EventDispatcher dispatcher;
        firefly::Input::init();

        bool keyHandled = false;
        dispatcher.subscribe<firefly::KeyEvent>([&](firefly::KeyEvent& e) {
            firefly::Input::on_key(e.key, e.action);
            keyHandled = true;
        });

        firefly::KeyEvent keyEvent(65, 0, 1, 0); // A key press
        dispatcher.publish(keyEvent);

        CHECK(keyHandled);
        CHECK(firefly::Input::is_key_down(firefly::Key::A));
    }
}

TEST_SUITE("Integration: FileSystem + Resource") {
    TEST_CASE("write and read via filesystem") {
        firefly::String path = "build/integration_test.txt";
        firefly::String content = "Firefly Integration Test";

        auto writeResult = firefly::FileSystem::write_file(
            path, content.data(), content.size());
        CHECK(writeResult.is_ok());

        auto readResult = firefly::FileSystem::read_text(path);
        CHECK(readResult.is_ok());
        CHECK(readResult.value() == content);

        std::filesystem::remove(path.c_str());
    }
}
