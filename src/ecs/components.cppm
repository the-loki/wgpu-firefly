module;
#include <string>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

export module firefly.ecs.components;

import firefly.core.types;

export namespace firefly::ecs {

struct Transform {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f, 1.0f, 1.0f};

    auto model_matrix() const -> glm::mat4;
};

struct NameComponent {
    String name;
};

struct MeshComponent {
    u64 mesh_id = 0;
    u64 material_id = 0;
    bool visible = true;
};

struct CameraComponent {
    f32 fov = 60.0f;
    f32 near_plane = 0.1f;
    f32 far_plane = 1000.0f;
    f32 aspect_ratio = 16.0f / 9.0f;
    bool is_primary = false;
};

enum class LightType : u8 {
    Directional = 0,
    Point = 1,
    Spot = 2,
};

struct LightComponent {
    LightType type = LightType::Directional;
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    f32 intensity = 1.0f;
    f32 range = 10.0f;
    f32 spot_angle = 45.0f;
};

struct NativeScriptComponent {
    std::function<void(f32)> on_update;
    std::function<void()> on_create;
    std::function<void()> on_destroy;
};

} // namespace firefly::ecs
