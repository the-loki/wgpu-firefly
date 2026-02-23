module;
#include <flecs.h>
#include <string>
#include <memory>

module firefly.scene.scene_manager;

namespace firefly {

Scene::Scene(ecs::World& world, const String& name)
    : m_name(name), m_world(world) {
    m_root = std::make_unique<SceneNode>(world, name + "_Root");
    m_nodeMap[m_root->name()] = m_root.get();
}

Scene::~Scene() = default;

auto Scene::create_node(const String& name) -> SceneNode* {
    auto* node = m_root->add_child(name);
    m_nodeMap[name] = node;
    return node;
}

void Scene::destroy_node(SceneNode* node) {
    if (!node || node == m_root.get()) return;
    m_nodeMap.erase(node->name());
    if (node->parent()) {
        node->parent()->remove_child(node);
    }
}

auto Scene::find_node(const String& name) -> SceneNode* {
    auto it = m_nodeMap.find(name);
    return it != m_nodeMap.end() ? it->second : nullptr;
}

void Scene::update(f32 deltaTime) {
    m_world.progress(deltaTime);
}

auto SceneManager::create_scene(ecs::World& world, const String& name) -> Scene* {
    auto scene = std::make_unique<Scene>(world, name);
    auto* ptr = scene.get();
    m_scenes.push_back(std::move(scene));
    if (!m_currentScene) m_currentScene = ptr;
    return ptr;
}

void SceneManager::set_current_scene(Scene* scene) {
    m_currentScene = scene;
}

} // namespace firefly
