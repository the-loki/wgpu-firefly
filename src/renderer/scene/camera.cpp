module;
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

module firefly.renderer.camera;

namespace firefly {

void Camera::set_perspective(f32 fov, f32 aspect, f32 near, f32 far) {
    m_projection = glm::perspective(glm::radians(fov), aspect, near, far);
}

void Camera::set_orthographic(f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far) {
    m_projection = glm::ortho(left, right, bottom, top, near, far);
}

void Camera::look_at(const glm::vec3& eye, const glm::vec3& target, const glm::vec3& up) {
    m_position = eye;
    m_view = glm::lookAt(eye, target, up);
}

} // namespace firefly
