module;
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

export module firefly.renderer.camera;

import firefly.core.types;

export namespace firefly {

class Camera {
public:
    Camera() = default;

    void set_perspective(f32 fov, f32 aspect, f32 near, f32 far);
    void set_orthographic(f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far);
    void look_at(const glm::vec3& eye, const glm::vec3& target, const glm::vec3& up);

    auto projection_matrix() const -> const glm::mat4& { return m_projection; }
    auto view_matrix() const -> const glm::mat4& { return m_view; }
    auto view_projection() const -> glm::mat4 { return m_projection * m_view; }

    auto position() const -> glm::vec3 { return m_position; }

private:
    glm::mat4 m_projection{1.0f};
    glm::mat4 m_view{1.0f};
    glm::vec3 m_position{0.0f};
};

} // namespace firefly
