#pragma once

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace firefly::examples::sponza {

inline constexpr float kMaxPitchDeg = 89.0f;
inline constexpr glm::vec3 kWorldUp{0.0f, 1.0f, 0.0f};

struct FreeCameraState {
    glm::vec3 position{0.0f, 3.2f, 18.0f};
    glm::quat orientation{1.0f, 0.0f, 0.0f, 0.0f};
    float pitchDeg = -10.0f;
};

struct CameraBasis {
    glm::vec3 viewForward{0.0f, 0.0f, -1.0f};
    glm::vec3 moveForward{0.0f, 0.0f, -1.0f};
    glm::vec3 moveRight{1.0f, 0.0f, 0.0f};
};

inline auto make_initial_orientation(float yawDeg, float pitchDeg) -> glm::quat {
    const float adjustedYawDeg = yawDeg + 90.0f;
    const glm::quat yaw = glm::angleAxis(glm::radians(adjustedYawDeg), kWorldUp);
    const glm::vec3 rightAxis = glm::normalize(glm::mat3_cast(yaw) * glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::quat pitch = glm::angleAxis(glm::radians(pitchDeg), rightAxis);
    return glm::normalize(pitch * yaw);
}

inline void apply_mouse_look(FreeCameraState& state, float mouseDeltaX, float mouseDeltaY, float sensitivity) {
    const float yawDeltaDeg = mouseDeltaX * sensitivity;
    const float pitchDeltaDeg = -mouseDeltaY * sensitivity;

    if (std::abs(yawDeltaDeg) > 1e-6f) {
        const glm::quat yaw = glm::angleAxis(glm::radians(yawDeltaDeg), kWorldUp);
        state.orientation = glm::normalize(yaw * state.orientation);
    }

    const float previousPitchDeg = state.pitchDeg;
    state.pitchDeg = std::clamp(state.pitchDeg + pitchDeltaDeg, -kMaxPitchDeg, kMaxPitchDeg);
    const float appliedPitchDeltaDeg = state.pitchDeg - previousPitchDeg;

    if (std::abs(appliedPitchDeltaDeg) > 1e-6f) {
        const glm::vec3 rightAxis =
            glm::normalize(glm::mat3_cast(state.orientation) * glm::vec3(1.0f, 0.0f, 0.0f));
        const glm::quat pitch = glm::angleAxis(glm::radians(appliedPitchDeltaDeg), rightAxis);
        state.orientation = glm::normalize(pitch * state.orientation);
    }
}

inline auto camera_basis(const FreeCameraState& state) -> CameraBasis {
    CameraBasis basis{};
    const glm::mat3 rotation = glm::mat3_cast(state.orientation);
    basis.viewForward = glm::normalize(rotation * glm::vec3(0.0f, 0.0f, -1.0f));

    glm::vec3 horizontalForward = basis.viewForward - glm::dot(basis.viewForward, kWorldUp) * kWorldUp;
    if (glm::dot(horizontalForward, horizontalForward) <= 1e-6f) {
        const glm::vec3 rightAxis = rotation * glm::vec3(1.0f, 0.0f, 0.0f);
        horizontalForward = glm::cross(rightAxis, kWorldUp);
        if (glm::dot(horizontalForward, horizontalForward) <= 1e-6f) {
            horizontalForward = glm::vec3(0.0f, 0.0f, -1.0f);
        }
    }

    basis.moveForward = glm::normalize(horizontalForward);
    basis.moveRight = glm::normalize(glm::cross(kWorldUp, basis.moveForward));
    return basis;
}

} // namespace firefly::examples::sponza
