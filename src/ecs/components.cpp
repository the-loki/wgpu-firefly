module;
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

module firefly.ecs.components;

namespace firefly::ecs {

auto Transform::model_matrix() const -> glm::mat4 {
    auto mat = glm::mat4(1.0f);
    mat = glm::translate(mat, position);
    mat *= glm::mat4_cast(rotation);
    mat = glm::scale(mat, scale);
    return mat;
}

} // namespace firefly::ecs
