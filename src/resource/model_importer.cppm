module;
#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>

export module firefly.resource.model_importer;

import firefly.core.types;
import firefly.resource.manager;

export namespace firefly {

struct ModelVertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec2 texCoord{0.0f};
    glm::vec4 color{1.0f};
    glm::vec3 emissive{0.0f};
    f32 metallic = 1.0f;
    f32 roughness = 1.0f;
    f32 ao = 1.0f;
};

struct ModelMeshData {
    String name;
    std::vector<ModelVertex> vertices;
    std::vector<u32> indices;
};

struct FlattenedModelData {
    std::vector<ModelVertex> vertices;
    std::vector<u32> indices;
};

class ModelData : public Resource {
public:
    std::vector<ModelMeshData> meshes;
    glm::vec3 boundingMin{0.0f};
    glm::vec3 boundingMax{0.0f};

    [[nodiscard]] auto total_vertex_count() const -> u32;
    [[nodiscard]] auto total_index_count() const -> u32;

    auto type_name() const -> String override { return "ModelData"; }
};

class ModelImporter : public LoaderBase {
public:
    auto load(const String& path) -> SharedPtr<Resource> override;
};

[[nodiscard]] auto flatten_model(const ModelData& model) -> FlattenedModelData;

} // namespace firefly
