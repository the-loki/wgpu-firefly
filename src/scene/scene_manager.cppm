module;
#include <flecs.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

export module firefly.scene.scene_manager;

import firefly.core.types;
import firefly.ecs.world;
import firefly.scene.scene_node;

export namespace firefly {

class Scene {
public:
    explicit Scene(ecs::World& world, const String& name = "Scene");
    ~Scene();

    auto name() const -> const String& { return m_name; }
    auto root() -> SceneNode* { return m_root.get(); }

    auto create_node(const String& name = "Node") -> SceneNode*;
    void destroy_node(SceneNode* node);
    auto find_node(const String& name) -> SceneNode*;

    void update(f32 deltaTime);
    auto world() -> ecs::World& { return m_world; }

private:
    String m_name;
    Ptr<SceneNode> m_root;
    ecs::World& m_world;
    std::unordered_map<String, SceneNode*> m_nodeMap;
};

class SceneManager {
public:
    SceneManager() = default;

    auto create_scene(ecs::World& world, const String& name) -> Scene*;
    auto current_scene() -> Scene* { return m_currentScene; }
    void set_current_scene(Scene* scene);
    auto scene_count() const -> size_t { return m_scenes.size(); }

private:
    std::vector<Ptr<Scene>> m_scenes;
    Scene* m_currentScene = nullptr;
};

} // namespace firefly
