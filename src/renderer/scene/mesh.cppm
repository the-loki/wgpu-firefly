module;
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

export module firefly.renderer.mesh;

import firefly.core.types;

export namespace firefly {

struct Vertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec2 texCoord{0.0f};
    glm::vec4 color{1.0f};
};

struct SubMesh {
    u32 indexOffset = 0;
    u32 indexCount = 0;
    u32 materialIndex = 0;
};

class Mesh {
public:
    Mesh() = default;

    auto vertices() const -> const std::vector<Vertex>& { return m_vertices; }
    auto indices() const -> const std::vector<u32>& { return m_indices; }
    auto submeshes() const -> const std::vector<SubMesh>& { return m_submeshes; }

    void set_vertices(std::vector<Vertex> verts) { m_vertices = std::move(verts); }
    void set_indices(std::vector<u32> idx) { m_indices = std::move(idx); }
    void add_submesh(const SubMesh& sub) { m_submeshes.push_back(sub); }

    auto vertex_buffer() const -> void* { return m_vertexBuffer; }
    auto index_buffer() const -> void* { return m_indexBuffer; }
    void set_gpu_buffers(void* vb, void* ib) { m_vertexBuffer = vb; m_indexBuffer = ib; }

    auto bounding_min() const -> glm::vec3 { return m_boundMin; }
    auto bounding_max() const -> glm::vec3 { return m_boundMax; }
    void compute_bounds();

private:
    std::vector<Vertex> m_vertices;
    std::vector<u32> m_indices;
    std::vector<SubMesh> m_submeshes;
    void* m_vertexBuffer = nullptr;
    void* m_indexBuffer = nullptr;
    glm::vec3 m_boundMin{0.0f};
    glm::vec3 m_boundMax{0.0f};
};

} // namespace firefly
