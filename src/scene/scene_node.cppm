module;
#include <flecs.h>
#include <string>
#include <vector>
#include <memory>

export module firefly.scene.scene_node;

import firefly.core.types;
import firefly.ecs.world;
import firefly.ecs.components;

export namespace firefly {

class SceneNode {
public:
    SceneNode(ecs::World& world, const String& name);
    ~SceneNode();

    auto name() const -> const String& { return m_name; }
    auto entity() const -> flecs::entity { return m_entity; }

    // Hierarchy
    auto parent() const -> SceneNode* { return m_parent; }
    auto children() const -> const std::vector<Ptr<SceneNode>>& { return m_children; }
    auto add_child(const String& name) -> SceneNode*;
    void remove_child(SceneNode* child);

    // Transform shortcuts
    void set_position(float x, float y, float z);
    void set_scale(float x, float y, float z);
    auto get_position() const -> std::tuple<float, float, float>;

    // Component access via ECS
    template<typename T, typename... Args>
    auto add_component(Args&&... args) -> T& {
        return m_world.add_component<T>(m_entity, std::forward<Args>(args)...);
    }

    template<typename T>
    auto get_component() -> T* {
        return m_world.get_component<T>(m_entity);
    }

    template<typename T>
    auto has_component() const -> bool {
        return m_world.has_component<T>(m_entity);
    }

private:
    String m_name;
    flecs::entity m_entity;
    ecs::World& m_world;
    SceneNode* m_parent = nullptr;
    std::vector<Ptr<SceneNode>> m_children;
};

} // namespace firefly
