import firefly.core.app;
import firefly.core.logger;
import firefly.core.types;
import firefly.ecs.engine_components;
import firefly.ecs.phases;
import firefly.renderer.camera;
import firefly.renderer.device;
import firefly.renderer.forward_renderer;
import firefly.renderer.mesh;
import firefly.resource.model_importer;

#include <flecs.h>
#include <cmath>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <vector>

static auto device_from(firefly::ecs::RenderState* rs) -> firefly::RenderDevice* {
    return static_cast<firefly::RenderDevice*>(rs->device.get());
}

static auto resolve_model_path() -> firefly::String {
    std::vector<std::filesystem::path> candidates;
    candidates.emplace_back("assets/models/cube.obj");
#ifdef FIREFLY_SOURCE_DIR
    candidates.emplace_back(std::filesystem::path(FIREFLY_SOURCE_DIR) / "assets/models/cube.obj");
#endif
    candidates.emplace_back(std::filesystem::path("..") / "assets/models/cube.obj");
    candidates.emplace_back(std::filesystem::path("..") / ".." / "assets/models/cube.obj");
    candidates.emplace_back(std::filesystem::path("..") / ".." / ".." / "assets/models/cube.obj");

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate.string();
        }
    }
    return {};
}

static auto load_model_as_render_mesh(
    const firefly::String& path,
    std::vector<firefly::Vertex>& outVertices,
    std::vector<firefly::u32>& outIndices) -> bool {
    firefly::ModelImporter importer;
    auto resource = importer.load(path);
    if (!resource) {
        return false;
    }

    auto* model = dynamic_cast<firefly::ModelData*>(resource.get());
    if (!model) {
        return false;
    }

    const auto flattened = firefly::flatten_model(*model);
    if (flattened.vertices.empty() || flattened.indices.empty()) {
        return false;
    }

    outVertices.clear();
    outVertices.reserve(flattened.vertices.size());
    for (const auto& importedVertex : flattened.vertices) {
        firefly::Vertex vertex{};
        vertex.position = importedVertex.position;
        vertex.normal = importedVertex.normal;
        vertex.texCoord = importedVertex.texCoord;
        vertex.color = importedVertex.color;
        vertex.emissive = importedVertex.emissive;
        vertex.metallic = importedVertex.metallic;
        vertex.roughness = importedVertex.roughness;
        vertex.ao = importedVertex.ao;
        outVertices.push_back(vertex);
    }
    outIndices = flattened.indices;
    return true;
}

struct ModelViewerResources {
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
    const firefly::f32 orbitRadius = 4.3f;
    const glm::vec3 eye{
        std::sin(t * 0.42f) * orbitRadius,
        2.1f,
        std::cos(t * 0.42f) * orbitRadius,
    };

    firefly::Camera camera;
    camera.set_perspective(60.0f, aspect, 0.1f, 100.0f);
    camera.look_at(eye, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    forward.set_view_params({
        .viewProjection = camera.view_projection(),
        .cameraWorldPosition = eye,
    });

    firefly::ForwardObjectParams object{};
    const auto scale = glm::scale(glm::mat4(1.0f), glm::vec3(0.75f));
    const auto rotateY = glm::rotate(glm::mat4(1.0f), t * 0.9f, glm::vec3(0.0f, 1.0f, 0.0f));
    const auto rotateX = glm::rotate(glm::mat4(1.0f), t * 0.3f, glm::vec3(1.0f, 0.0f, 0.0f));
    object.model = rotateY * rotateX * scale;
    object.material.baseColor = glm::vec4(0.85f, 0.9f, 1.0f, 1.0f);
    object.material.metallic = 0.08f;
    object.material.roughness = 0.34f;
    object.material.ao = 1.0f;
    forward.set_object(object);

    std::vector<firefly::ForwardPointLight> pointLights;
    pointLights.reserve(3);
    for (firefly::u32 i = 0; i < 3; ++i) {
        const firefly::f32 phase = t * (0.9f + 0.15f * static_cast<firefly::f32>(i));
        const firefly::f32 angle = phase + static_cast<firefly::f32>(i) * 2.0943951f;
        firefly::ForwardPointLight point{};
        point.position = glm::vec3(
            std::cos(angle) * 2.7f,
            1.4f + 0.5f * std::sin(phase * 1.3f),
            std::sin(angle) * 2.7f);
        point.range = 6.0f;
        point.intensity = 16.0f;
        if (i == 0) {
            point.color = glm::vec3(1.0f, 0.55f, 0.45f);
        } else if (i == 1) {
            point.color = glm::vec3(0.45f, 0.8f, 1.0f);
        } else {
            point.color = glm::vec3(0.6f, 1.0f, 0.6f);
        }
        pointLights.push_back(point);
    }
    forward.set_point_lights(pointLights);
}

int main() {
    firefly::App app;
    app.configure({
        .title = "Firefly - Model Viewer (Assimp)",
        .width = 1280,
        .height = 720,
        .enable_render_graph_diagnostics = false,
    });

    app.setup([](flecs::world& world) {
        world.system("CreateModelViewerResources")
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
                    firefly::Logger::error("Render device unavailable for model viewer setup");
                    return;
                }

                auto forward = std::make_unique<firefly::ForwardRenderer>();
                auto initResult = forward->initialize(*device, device->surface_format());
                if (initResult.is_error()) {
                    firefly::Logger::error("Failed to initialize forward renderer: {}", initResult.error());
                    return;
                }

                const auto modelPath = resolve_model_path();
                if (!modelPath.empty()) {
                    std::vector<firefly::Vertex> modelVertices;
                    std::vector<firefly::u32> modelIndices;
                    if (load_model_as_render_mesh(modelPath, modelVertices, modelIndices)) {
                        auto meshResult = forward->set_mesh_data(modelVertices, modelIndices);
                        if (meshResult.is_error()) {
                            firefly::Logger::warn("Model mesh upload failed, fallback to sphere: {}", meshResult.error());
                        } else {
                            firefly::Logger::info("Loaded model mesh from {}", modelPath);
                        }
                    } else {
                        firefly::Logger::warn("Failed to load model from {}, using default sphere", modelPath);
                    }
                } else {
                    firefly::Logger::warn("Model asset assets/models/cube.obj not found, using default sphere");
                }

                forward->set_directional_lights({
                    {
                        .direction = glm::vec3(-0.5f, -1.0f, -0.35f),
                        .intensity = 3.4f,
                        .color = glm::vec3(1.0f, 0.98f, 0.95f),
                        .ambientIntensity = 0.08f,
                    },
                    {
                        .direction = glm::vec3(0.6f, -0.8f, 0.25f),
                        .intensity = 1.0f,
                        .color = glm::vec3(0.45f, 0.55f, 1.0f),
                        .ambientIntensity = 0.0f,
                    }
                });
                forward->set_grid_params({
                    .enabled = false,
                    .columns = 1,
                    .rows = 1,
                    .spacing = 2.0f,
                });
                forward->set_shadow_settings({
                    .constantBias = 0.0009f,
                    .slopeBias = 0.0038f,
                    .enablePCF = true,
                    .enableCascades = true,
                    .cascadeSplitDistance = 8.0f,
                    .cascadeBlendDistance = 1.2f,
                    .shadowMapResolution = 1024,
                });

                update_forward_view_and_object(*forward, *window, w.get<firefly::ecs::TimeState>());
                render->graph.reset();
                render->pending_graph.reset();
                render->retired_graphs.clear();
                render->auto_execute_graph = false;
                w.set<ModelViewerResources>({
                    .forward = std::move(forward),
                });
            });

        const auto* phases = world.get<firefly::ecs::EnginePhases>();
        world.system("UpdateModelViewerFrame")
            .kind(phases->render_begin)
            .run([](flecs::iter& it) {
                auto w = it.world();
                auto* resources = w.get_mut<ModelViewerResources>();
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

        world.system("RenderModelViewerFrame")
            .kind(phases->render_end)
            .run([](flecs::iter& it) {
                auto w = it.world();
                const auto* window = w.get<firefly::ecs::WindowState>();
                auto* resources = w.get_mut<ModelViewerResources>();
                if (!window || !resources || !resources->forward) {
                    return;
                }
                if (window->width == 0 || window->height == 0) {
                    return;
                }

                auto renderResult = resources->forward->render_frame(window->width, window->height);
                if (renderResult.is_error()) {
                    firefly::Logger::error("Failed to render model viewer frame: {}", renderResult.error());
                }
            });
    });

    return app.run();
}
