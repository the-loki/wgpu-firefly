#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include <flecs.h>

import firefly.scene.scene_node;
import firefly.scene.scene_manager;
import firefly.ecs.world;
import firefly.ecs.components;
import firefly.core.types;

// ============================================================
// Phase 7: Scene 模块测试
// ============================================================

TEST_SUITE("SceneNode") {
    TEST_CASE("create node with transform") {
        firefly::ecs::World world;
        firefly::SceneNode node(world, "TestNode");
        CHECK(node.name() == "TestNode");
        CHECK(node.entity().is_alive());
        CHECK(node.has_component<firefly::ecs::Transform>());
        CHECK(node.has_component<firefly::ecs::NameComponent>());
    }

    TEST_CASE("set and get position") {
        firefly::ecs::World world;
        firefly::SceneNode node(world, "Pos");
        node.set_position(1.0f, 2.0f, 3.0f);
        auto [x, y, z] = node.get_position();
        CHECK(x == doctest::Approx(1.0f));
        CHECK(y == doctest::Approx(2.0f));
        CHECK(z == doctest::Approx(3.0f));
    }

    TEST_CASE("parent-child hierarchy") {
        firefly::ecs::World world;
        firefly::SceneNode root(world, "Root");
        auto* child = root.add_child("Child");
        CHECK(child != nullptr);
        CHECK(child->name() == "Child");
        CHECK(child->parent() == &root);
        CHECK(root.children().size() == 1);
    }

    TEST_CASE("remove child") {
        firefly::ecs::World world;
        firefly::SceneNode root(world, "Root");
        auto* child = root.add_child("Child");
        CHECK(root.children().size() == 1);
        root.remove_child(child);
        CHECK(root.children().empty());
    }
}

TEST_SUITE("Scene") {
    TEST_CASE("create scene with root") {
        firefly::ecs::World world;
        firefly::Scene scene(world, "TestScene");
        CHECK(scene.name() == "TestScene");
        CHECK(scene.root() != nullptr);
    }

    TEST_CASE("create and find node") {
        firefly::ecs::World world;
        firefly::Scene scene(world, "TestScene");
        auto* node = scene.create_node("Player");
        CHECK(node != nullptr);
        CHECK(node->name() == "Player");

        auto* found = scene.find_node("Player");
        CHECK(found == node);
    }

    TEST_CASE("find nonexistent node") {
        firefly::ecs::World world;
        firefly::Scene scene(world, "TestScene");
        CHECK(scene.find_node("Ghost") == nullptr);
    }

    TEST_CASE("destroy node") {
        firefly::ecs::World world;
        firefly::Scene scene(world, "TestScene");
        auto* node = scene.create_node("Temp");
        CHECK(scene.find_node("Temp") != nullptr);
        scene.destroy_node(node);
        CHECK(scene.find_node("Temp") == nullptr);
    }
}

TEST_SUITE("SceneManager") {
    TEST_CASE("create and manage scenes") {
        firefly::ecs::World world;
        firefly::SceneManager mgr;

        auto* scene1 = mgr.create_scene(world, "Level1");
        CHECK(scene1 != nullptr);
        CHECK(mgr.current_scene() == scene1);
        CHECK(mgr.scene_count() == 1);

        auto* scene2 = mgr.create_scene(world, "Level2");
        CHECK(mgr.scene_count() == 2);
        CHECK(mgr.current_scene() == scene1);

        mgr.set_current_scene(scene2);
        CHECK(mgr.current_scene() == scene2);
    }
}
