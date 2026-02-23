import firefly.core.app;
import firefly.core.logger;
import firefly.core.types;
import firefly.ecs.engine_components;
import firefly.ecs.phases;
import firefly.renderer.camera;
import firefly.renderer.device;
import firefly.renderer.forward_renderer;

#include <flecs.h>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <vector>

static auto device_from(firefly::ecs::RenderState* rs) -> firefly::RenderDevice* {
    return static_cast<firefly::RenderDevice*>(rs->device.get());
}

struct ForwardExampleResources {
    firefly::Ptr<firefly::ForwardRenderer> forward;
};

static void update_forward_view_and_object(
    firefly::ForwardRenderer& forward,
    const firefly::ecs::WindowState& window,
    const firefly::ecs::TimeState* time) {
    const firefly::f32 aspect = window.height > 0
        ? static_cast<firefly::f32>(window.width) / static_cast<firefly::f32>(window.height)
        : (16.0f / 9.0f);
    const firefly::f32 t = time ? static_cast<firefly::f32>(time->total_time) : 0.0f;
    const firefly::f32 orbitRadius = 3.2f;
    const glm::vec3 eye{
        std::sin(t * 0.55f) * orbitRadius,
        1.2f,
        std::cos(t * 0.55f) * orbitRadius,
    };

    firefly::Camera camera;
    camera.set_perspective(60.0f, aspect, 0.1f, 100.0f);
    camera.look_at(eye, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    forward.set_view_params({
        .viewProjection = camera.view_projection(),
        .cameraWorldPosition = eye,
    });

    firefly::ForwardObjectParams object{};
    object.model = glm::rotate(glm::mat4(1.0f), t * 0.9f, glm::vec3(0.0f, 1.0f, 0.0f));
    object.material.baseColor = glm::vec4(1.0f, 0.766f, 0.336f, 1.0f);
    object.material.metallic = 0.95f;
    object.material.roughness = 0.18f + 0.08f * std::sin(t * 0.7f);
    object.material.ao = 1.0f;
    forward.set_object(object);

    std::vector<firefly::ForwardPointLight> pointLights;
    pointLights.reserve(3);
    for (firefly::u32 i = 0; i < 3; ++i) {
        const firefly::f32 phase = t * (0.8f + 0.2f * static_cast<firefly::f32>(i));
        const firefly::f32 angle = phase + static_cast<firefly::f32>(i) * 2.0943951f;
        firefly::ForwardPointLight point{};
        point.position = glm::vec3(
            std::cos(angle) * 2.3f,
            1.2f + 0.4f * std::sin(phase * 1.7f),
            std::sin(angle) * 2.3f);
        point.range = 5.5f;
        point.intensity = 18.0f;
        if (i == 0) {
            point.color = glm::vec3(1.0f, 0.4f, 0.35f);
        } else if (i == 1) {
            point.color = glm::vec3(0.35f, 0.8f, 1.0f);
        } else {
            point.color = glm::vec3(0.45f, 1.0f, 0.55f);
        }
        pointLights.push_back(point);
    }
    forward.set_point_lights(pointLights);
}

int main() {
    firefly::App app;
    app.configure({
        .title = "Firefly - Forward Sphere",
        .width = 1280,
        .height = 720,
        .enable_render_graph_diagnostics = true,
        .render_graph_diagnostics_interval_frames = 240,
    });

    app.setup([](flecs::world& world) {
        world.system("CreateForwardResources")
            .kind(flecs::OnStart)
            .run([](flecs::iter& it) {
                auto w = it.world();
                auto* render = w.get_mut<firefly::ecs::RenderState>();
                const auto* window = w.get<firefly::ecs::WindowState>();
                if (!render || !render->initialized || !window) {
                    return;
                }

                auto* device = device_from(render);
                if (!device) {
                    firefly::Logger::error("Render device unavailable for forward setup");
                    return;
                }

                auto forward = std::make_unique<firefly::ForwardRenderer>();
                auto initResult = forward->initialize(*device, device->surface_format());
                if (initResult.is_error()) {
                    firefly::Logger::error("Failed to initialize forward renderer: {}", initResult.error());
                    return;
                }

                forward->set_directional_lights({
                    {
                        .direction = glm::vec3(-0.5f, -1.0f, -0.35f),
                        .intensity = 3.6f,
                        .color = glm::vec3(1.0f, 0.98f, 0.95f),
                        .ambientIntensity = 0.06f,
                    },
                    {
                        .direction = glm::vec3(0.6f, -0.8f, 0.25f),
                        .intensity = 1.2f,
                        .color = glm::vec3(0.45f, 0.55f, 1.0f),
                        .ambientIntensity = 0.0f,
                    }
                });
                forward->set_grid_params({
                    .enabled = true,
                    .columns = 7,
                    .rows = 7,
                    .spacing = 2.25f,
                });
                forward->set_shadow_settings({
                    .constantBias = 0.0009f,
                    .slopeBias = 0.0038f,
                    .enablePCF = true,
                    .enableCascades = true,
                    .cascadeSplitDistance = 8.0f,
                    .cascadeBlendDistance = 1.35f,
                    .shadowMapResolution = 1024,
                });

                update_forward_view_and_object(*forward, *window, w.get<firefly::ecs::TimeState>());
                render->graph.reset();
                render->pending_graph.reset();
                render->retired_graphs.clear();
                render->auto_execute_graph = false;
                w.set<ForwardExampleResources>({
                    .forward = std::move(forward),
                });
            });

        const auto* phases = world.get<firefly::ecs::EnginePhases>();
        world.system("UpdateForwardFrame")
            .kind(phases->render_begin)
            .run([](flecs::iter& it) {
                auto w = it.world();
                auto* resources = w.get_mut<ForwardExampleResources>();
                const auto* window = w.get<firefly::ecs::WindowState>();
                const auto* time = w.get<firefly::ecs::TimeState>();
                if (!resources || !resources->forward || !window) {
                    return;
                }
                if (window->width == 0 || window->height == 0) {
                    return;
                }

                update_forward_view_and_object(*resources->forward, *window, time);
            });

        world.system("RenderForwardFrame")
            .kind(phases->render_end)
            .run([](flecs::iter& it) {
                auto w = it.world();
                const auto* window = w.get<firefly::ecs::WindowState>();
                auto* resources = w.get_mut<ForwardExampleResources>();
                if (!window || !resources || !resources->forward) {
                    return;
                }
                if (window->width == 0 || window->height == 0) {
                    return;
                }
                auto renderResult = resources->forward->render_frame(window->width, window->height);
                if (renderResult.is_error()) {
                    firefly::Logger::error("Failed to render forward frame: {}", renderResult.error());
                }
            });
    });

    return app.run();
}

