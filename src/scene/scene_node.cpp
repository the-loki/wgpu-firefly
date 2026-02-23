module;
#include <flecs.h>
#include <glm/glm.hpp>
#include <string>
#include <algorithm>
#include <memory>
#include <tuple>

module firefly.scene.scene_node;

namespace firefly {

SceneNode::SceneNode(ecs::World& world, const String& name)
    : m_name(name), m_world(world) {
    m_entity = m_world.create_entity(name);
    m_world.add_component<ecs::Transform>(m_entity);
    m_world.add_component<ecs::NameComponent>(m_entity);
    auto* nc = m_world.get_component<ecs::NameComponent>(m_entity);
    if (nc) nc->name = name;
}

SceneNode::~SceneNode() {
    m_children.clear();
    if (m_entity.is_alive()) {
        m_world.destroy_entity(m_entity);
    }
}

auto SceneNode::add_child(const String& name) -> SceneNode* {
    auto child = std::make_unique<SceneNode>(m_world, name);
    child->m_parent = this;
    auto* ptr = child.get();
    m_children.push_back(std::move(child));
    return ptr;
}

void SceneNode::remove_child(SceneNode* child) {
    auto it = std::find_if(m_children.begin(), m_children.end(),
        [child](const auto& c) { return c.get() == child; });
    if (it != m_children.end()) {
        m_children.erase(it);
    }
}

void SceneNode::set_position(float x, float y, float z) {
    auto* t = m_world.get_component<ecs::Transform>(m_entity);
    if (t) t->position = glm::vec3(x, y, z);
}

void SceneNode::set_scale(float x, float y, float z) {
    auto* t = m_world.get_component<ecs::Transform>(m_entity);
    if (t) t->scale = glm::vec3(x, y, z);
}

auto SceneNode::get_position() const -> std::tuple<float, float, float> {
    auto* t = m_world.get_component<ecs::Transform>(m_entity);
    if (t) return {t->position.x, t->position.y, t->position.z};
    return {0, 0, 0};
}

} // namespace firefly
