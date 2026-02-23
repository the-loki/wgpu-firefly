module;
#include <flecs.h>
#include <string>

module firefly.ecs.world;

namespace firefly::ecs {

World::World() = default;

World::~World() = default;

auto World::create_entity(const String& name) -> flecs::entity {
    if (name.empty()) {
        return m_world.entity();
    }
    return m_world.entity(name.c_str());
}

void World::destroy_entity(flecs::entity entity) {
    entity.destruct();
}

void World::progress(f32 delta_time) {
    m_world.progress(static_cast<ecs_ftime_t>(delta_time));
}

} // namespace firefly::ecs
