module;
#include <flecs.h>

module firefly.ecs.phases;

namespace firefly::ecs {

void register_engine_phases(flecs::world& world) {
    EnginePhases phases;

    // InputProcess runs during OnLoad
    phases.input_process = world.entity("InputProcessPhase")
        .add(flecs::Phase)
        .depends_on(flecs::OnLoad);

    // PollEvents runs right after InputProcess so edge states are preserved.
    phases.poll_events = world.entity("PollEventsPhase")
        .add(flecs::Phase)
        .depends_on(phases.input_process);

    // RenderBegin runs after PostUpdate
    phases.render_begin = world.entity("RenderBeginPhase")
        .add(flecs::Phase)
        .depends_on(flecs::PostUpdate);

    // RenderEnd runs after RenderBegin
    phases.render_end = world.entity("RenderEndPhase")
        .add(flecs::Phase)
        .depends_on(phases.render_begin);

    world.set<EnginePhases>(phases);
}

} // namespace firefly::ecs
