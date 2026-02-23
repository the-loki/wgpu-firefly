module;
#include <flecs.h>
#include <string>

export module firefly.ecs.world;

import firefly.core.types;
import firefly.ecs.components;

export namespace firefly::ecs {

class World {
public:
    World();
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // Entity management
    auto create_entity(const String& name = "") -> flecs::entity;
    void destroy_entity(flecs::entity entity);

    // Component operations (template, defined in header)
    template<typename T, typename... Args>
    auto add_component(flecs::entity entity, Args&&... args) -> T& {
        entity.set<T>(T{std::forward<Args>(args)...});
        return *entity.get_mut<T>();
    }

    template<typename T>
    auto get_component(flecs::entity entity) -> T* {
        return entity.get_mut<T>();
    }

    template<typename T>
    auto get_component(flecs::entity entity) const -> const T* {
        return entity.get<T>();
    }

    template<typename T>
    auto has_component(flecs::entity entity) const -> bool {
        return entity.has<T>();
    }

    template<typename T>
    void remove_component(flecs::entity entity) {
        entity.remove<T>();
    }

    // Query
    template<typename... Components>
    auto query() {
        return m_world.query<Components...>();
    }

    // System registration
    template<typename... Components, typename F>
    auto system(const String& name, F&& callback) {
        return m_world.system<Components...>(name.c_str())
            .each(std::forward<F>(callback));
    }

    // Progress the world
    void progress(f32 delta_time);

    // Access underlying flecs world
    auto raw() -> flecs::world& { return m_world; }
    auto raw() const -> const flecs::world& { return m_world; }

private:
    flecs::world m_world;
};

} // namespace firefly::ecs
