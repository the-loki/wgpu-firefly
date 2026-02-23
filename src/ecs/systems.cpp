module;
#include <flecs.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

module firefly.ecs.systems;

namespace firefly::ecs {

void register_transform_system(World& world) {
    world.raw().system<Transform>("TransformSystem")
        .each([](flecs::entity, Transform& t) {
            if (t.scale.x == 0.0f) t.scale.x = 0.001f;
            if (t.scale.y == 0.0f) t.scale.y = 0.001f;
            if (t.scale.z == 0.0f) t.scale.z = 0.001f;
            t.rotation = glm::normalize(t.rotation);
        });
}

void register_script_system(World& world) {
    world.raw().system<NativeScriptComponent>("ScriptSystem")
        .each([](flecs::entity e, NativeScriptComponent& script) {
            if (script.on_update) {
                script.on_update(e.world().delta_time());
            }
        });
}

void register_all_systems(World& world) {
    register_transform_system(world);
    register_script_system(world);
}

} // namespace firefly::ecs
