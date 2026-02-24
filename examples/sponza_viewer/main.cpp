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
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <vector>

static auto device_from(firefly::ecs::RenderState* rs) -> firefly::RenderDevice* {
    return static_cast<firefly::RenderDevice*>(rs->device.get());
}

static auto resolve_sponza_path() -> firefly::String {
    std::vector<std::filesystem::path> candidates;
    candidates.emplace_back("assets/models/sponza-png.glb");
#ifdef FIREFLY_SOURCE_DIR
    candidates.emplace_back(std::filesystem::path(FIREFLY_SOURCE_DIR) / "assets/models/sponza-png.glb");
#endif
    candidates.emplace_back(std::filesystem::path("..") / "assets/models/sponza-png.glb");
    candidates.emplace_back(std::filesystem::path("..") / ".." / "assets/models/sponza-png.glb");
    candidates.emplace_back(std::filesystem::path("..") / ".." / ".." / "assets/models/sponza-png.glb");

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate.string();
        }
    }
    return {};
}

static auto load_sponza_mesh(
    const firefly::String& path,
    std::vector<firefly::Vertex>& outVertices,
    std::vector<firefly::u32>& outIndices,
    glm::vec3& outCenter,
    firefly::f32& outFitScale) -> bool {
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

    outCenter = (model->boundingMin + model->boundingMax) * 0.5f;
    const glm::vec3 extent = model->boundingMax - model->boundingMin;
    const firefly::f32 maxExtent = std::max({extent.x, extent.y, extent.z, 0.001f});
    outFitScale = 20.0f / maxExtent;
    return true;
}

struct SponzaViewerResources {
    firefly::Ptr<firefly::ForwardRenderer> forward;
    glm::vec3 modelCenter{0.0f};
    firefly::f32 modelScale = 1.0f;
    bool modelLoaded = false;
    glm::vec3 cameraPosition{0.0f, 3.2f, 18.0f};
    firefly::f32 cameraYaw = -90.0f;
    firefly::f32 cameraPitch = -10.0f;
    firefly::f32 moveSpeed = 8.0f;
    firefly::f32 sprintMultiplier = 3.0f;
    firefly::f32 mouseSensitivity = 0.085f;
};

static auto camera_forward(firefly::f32 yawDeg, firefly::f32 pitchDeg) -> glm::vec3 {
    const firefly::f32 yawRad = glm::radians(yawDeg);
    const firefly::f32 pitchRad = glm::radians(pitchDeg);
    glm::vec3 forward{
        std::cos(pitchRad) * std::cos(yawRad),
        std::sin(pitchRad),
        std::cos(pitchRad) * std::sin(yawRad),
    };
    return glm::normalize(forward);
}

static void update_view_and_object(
    SponzaViewerResources& resources,
    const firefly::ecs::WindowState& window,
    const firefly::ecs::InputState* input,
    firefly::f32 deltaSeconds) {
    const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    const int rightButtonIndex = static_cast<int>(firefly::ecs::MouseButton::Right);
    const bool hasRightButtonState = input &&
        rightButtonIndex >= 0 &&
        rightButtonIndex < firefly::ecs::InputState::MAX_BUTTONS;
    const bool rightMouseDown = hasRightButtonState &&
        input->current_buttons[rightButtonIndex];
    const bool rightMousePressed = rightMouseDown &&
        !input->previous_buttons[rightButtonIndex];
    const bool rightMouseReleased = hasRightButtonState &&
        !input->current_buttons[rightButtonIndex] &&
        input->previous_buttons[rightButtonIndex];

    if (window.handle) {
        if (rightMousePressed) {
            glfwSetInputMode(window.handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            if (glfwRawMouseMotionSupported()) {
                glfwSetInputMode(window.handle, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
            }
        } else if (rightMouseReleased) {
            glfwSetInputMode(window.handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }

    if (input && std::abs(input->scroll) > 0.0) {
        const firefly::f32 speedScale = std::pow(1.2f, static_cast<firefly::f32>(input->scroll));
        resources.moveSpeed = std::clamp(resources.moveSpeed * speedScale, 1.0f, 80.0f);
    }

    if (rightMouseDown && !rightMousePressed && input) {
        resources.cameraYaw += static_cast<firefly::f32>(input->mouse_delta.first) * resources.mouseSensitivity;
        resources.cameraPitch -= static_cast<firefly::f32>(input->mouse_delta.second) * resources.mouseSensitivity;
        resources.cameraPitch = std::clamp(resources.cameraPitch, -89.0f, 89.0f);
    }

    glm::vec3 viewForward = camera_forward(resources.cameraYaw, resources.cameraPitch);
    const glm::vec3 right = glm::normalize(glm::cross(viewForward, worldUp));

    if (input && rightMouseDown && deltaSeconds > 0.0f) {
        firefly::f32 speed = resources.moveSpeed;
        if (input->is_key_down(firefly::ecs::Key::LeftShift) ||
            input->is_key_down(firefly::ecs::Key::RightShift)) {
            speed *= resources.sprintMultiplier;
        }
        speed *= deltaSeconds;

        glm::vec3 movement(0.0f);
        if (input->is_key_down(firefly::ecs::Key::W)) {
            movement += viewForward;
        }
        if (input->is_key_down(firefly::ecs::Key::S)) {
            movement -= viewForward;
        }
        if (input->is_key_down(firefly::ecs::Key::D)) {
            movement += right;
        }
        if (input->is_key_down(firefly::ecs::Key::A)) {
            movement -= right;
        }
        if (input->is_key_down(firefly::ecs::Key::E) ||
            input->is_key_down(firefly::ecs::Key::Space)) {
            movement += worldUp;
        }
        if (input->is_key_down(firefly::ecs::Key::Q) ||
            input->is_key_down(firefly::ecs::Key::LeftControl) ||
            input->is_key_down(firefly::ecs::Key::RightControl)) {
            movement -= worldUp;
        }

        if (glm::dot(movement, movement) > 1e-6f) {
            resources.cameraPosition += glm::normalize(movement) * speed;
        }
    }

    viewForward = camera_forward(resources.cameraYaw, resources.cameraPitch);
    const firefly::f32 aspect = window.height > 0
        ? static_cast<firefly::f32>(window.width) / static_cast<firefly::f32>(window.height)
        : (16.0f / 9.0f);

    firefly::Camera camera;
    camera.set_perspective(60.0f, aspect, 0.1f, 300.0f);
    camera.look_at(resources.cameraPosition, resources.cameraPosition + viewForward, worldUp);
    resources.forward->set_view_params({
        .viewProjection = camera.view_projection(),
        .cameraWorldPosition = resources.cameraPosition,
    });

    firefly::ForwardObjectParams object{};
    const glm::mat4 centerTranslation = glm::translate(glm::mat4(1.0f), -resources.modelCenter);
    const glm::mat4 fitScale = glm::scale(glm::mat4(1.0f), glm::vec3(resources.modelScale));
    object.model = fitScale * centerTranslation;
    object.material.baseColor = glm::vec4(1.0f);
    object.material.metallic = 1.0f;
    object.material.roughness = 1.0f;
    object.material.ao = 1.0f;
    resources.forward->set_object(object);
}

int main() {
    firefly::App app;
    app.configure({
        .title = "Firefly - Sponza Viewer (glb)",
        .width = 1600,
        .height = 900,
        .enable_render_graph_diagnostics = false,
    });

    app.setup([](flecs::world& world) {
        world.system("CreateSponzaViewerResources")
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
                    firefly::Logger::error("Render device unavailable for sponza setup");
                    return;
                }

                auto resources = SponzaViewerResources{};
                resources.forward = std::make_unique<firefly::ForwardRenderer>();

                auto initResult = resources.forward->initialize(*device, device->surface_format());
                if (initResult.is_error()) {
                    firefly::Logger::error("Failed to initialize forward renderer: {}", initResult.error());
                    return;
                }

                const auto sponzaPath = resolve_sponza_path();
                if (sponzaPath.empty()) {
                    firefly::Logger::error("Sponza glb not found: assets/models/sponza-png.glb");
                    return;
                }

                if (window->handle) {
                    glfwSetInputMode(window->handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                }

                std::vector<firefly::Vertex> vertices;
                std::vector<firefly::u32> indices;
                if (!load_sponza_mesh(
                    sponzaPath,
                    vertices,
                    indices,
                    resources.modelCenter,
                    resources.modelScale)) {
                    firefly::Logger::error("Failed to import Sponza model: {}", sponzaPath);
                    return;
                }

                auto meshResult = resources.forward->set_mesh_data(vertices, indices);
                if (meshResult.is_error()) {
                    firefly::Logger::error("Failed to upload Sponza mesh: {}", meshResult.error());
                    return;
                }
                resources.modelLoaded = true;

                firefly::Logger::info(
                    "Sponza loaded: path={}, vertices={}, indices={}, fitScale={}",
                    sponzaPath,
                    vertices.size(),
                    indices.size(),
                    resources.modelScale);
                firefly::Logger::info(
                    "Sponza UE camera: hold RMB + mouse look, hold RMB + WASD fly, Q/E down/up, Shift speed up, wheel adjust speed");

                resources.forward->set_directional_lights({
                    {
                        .direction = glm::vec3(-0.55f, -1.0f, -0.15f),
                        .intensity = 4.2f,
                        .color = glm::vec3(1.0f, 0.98f, 0.95f),
                        .ambientIntensity = 0.08f,
                    },
                    {
                        .direction = glm::vec3(0.4f, -0.7f, 0.5f),
                        .intensity = 1.0f,
                        .color = glm::vec3(0.45f, 0.55f, 1.0f),
                        .ambientIntensity = 0.0f,
                    }
                });
                resources.forward->set_point_lights({
                    {
                        .position = glm::vec3(0.0f, 7.5f, 0.0f),
                        .range = 22.0f,
                        .color = glm::vec3(1.0f, 0.8f, 0.7f),
                        .intensity = 10.0f,
                    }
                });
                resources.forward->set_grid_params({
                    .enabled = false,
                    .columns = 1,
                    .rows = 1,
                    .spacing = 2.0f,
                });
                resources.forward->set_shadow_settings({
                    .constantBias = 0.0012f,
                    .slopeBias = 0.0040f,
                    .enablePCF = true,
                    .enableCascades = true,
                    .cascadeSplitDistance = 14.0f,
                    .cascadeBlendDistance = 2.0f,
                    .shadowMapResolution = 2048,
                });

                update_view_and_object(resources, *window, w.get<firefly::ecs::InputState>(), 0.0f);

                render->graph.reset();
                render->pending_graph.reset();
                render->retired_graphs.clear();
                render->auto_execute_graph = false;
                w.set<SponzaViewerResources>(std::move(resources));
            });

        const auto* phases = world.get<firefly::ecs::EnginePhases>();
        world.system("UpdateSponzaFrame")
            .kind(phases->render_begin)
            .run([](flecs::iter& it) {
                auto w = it.world();
                auto* resources = w.get_mut<SponzaViewerResources>();
                const auto* window = w.get<firefly::ecs::WindowState>();
                const auto* input = w.get<firefly::ecs::InputState>();
                if (!resources || !resources->forward || !resources->modelLoaded || !window) {
                    return;
                }
                if (window->width == 0 || window->height == 0) {
                    return;
                }

                update_view_and_object(*resources, *window, input, static_cast<firefly::f32>(it.delta_time()));
            });

        world.system("RenderSponzaFrame")
            .kind(phases->render_end)
            .run([](flecs::iter& it) {
                auto w = it.world();
                auto* resources = w.get_mut<SponzaViewerResources>();
                const auto* window = w.get<firefly::ecs::WindowState>();
                if (!resources || !resources->forward || !resources->modelLoaded || !window) {
                    return;
                }
                if (window->width == 0 || window->height == 0) {
                    return;
                }

                auto renderResult = resources->forward->render_frame(window->width, window->height);
                if (renderResult.is_error()) {
                    firefly::Logger::error("Failed to render sponza frame: {}", renderResult.error());
                }
            });
    });

    return app.run();
}
