module;
#include <glm/glm.hpp>
#include <limits>

module firefly.renderer.mesh;

namespace firefly {

void Mesh::compute_bounds() {
    if (m_vertices.empty()) return;
    m_boundMin = glm::vec3(std::numeric_limits<float>::max());
    m_boundMax = glm::vec3(std::numeric_limits<float>::lowest());
    for (const auto& v : m_vertices) {
        m_boundMin = glm::min(m_boundMin, v.position);
        m_boundMax = glm::max(m_boundMax, v.position);
    }
}

} // namespace firefly
