module;
#include <flecs.h>

export module firefly.ecs.phases;

import firefly.core.types;

export namespace firefly::ecs {

// Register custom engine phases into the Flecs world.
// Call this before registering any engine systems.
void register_engine_phases(flecs::world& world);

// Phase entities (set by register_engine_phases)
struct EnginePhases {
    flecs::entity input_process;  // after OnLoad
    flecs::entity poll_events;    // after input_process
    flecs::entity render_begin;   // after PostUpdate, before OnStore
    flecs::entity render_end;     // after OnStore
};

} // namespace firefly::ecs
