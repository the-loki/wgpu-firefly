module;
#include <flecs.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

export module firefly.ecs.systems;

import firefly.core.types;
import firefly.ecs.components;
import firefly.ecs.world;

export namespace firefly::ecs {

// Register built-in systems into a World
void register_transform_system(World& world);
void register_script_system(World& world);

// Register all built-in systems
void register_all_systems(World& world);

} // namespace firefly::ecs
