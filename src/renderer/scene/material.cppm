module;
#include <cstdint>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

export module firefly.renderer.material;

import firefly.core.types;
import firefly.renderer.shader;
import firefly.renderer.texture;

export namespace firefly {

struct MaterialProperty {
    enum class Type { Float, Vec2, Vec3, Vec4, Mat4, Texture };
    Type type = Type::Float;
    union {
        f32 floatVal;
        f32 vec2Val[2];
        f32 vec3Val[3];
        f32 vec4Val[4];
        f32 mat4Val[16];
    };
    Texture* textureVal = nullptr;
};

class Material {
public:
    Material() = default;

    void set_shader(Shader* shader) { m_shader = shader; }
    auto shader() const -> Shader* { return m_shader; }

    void set_float(const String& name, f32 value);
    void set_vec3(const String& name, const glm::vec3& value);
    void set_vec4(const String& name, const glm::vec4& value);
    void set_texture(const String& name, Texture* tex);

    auto properties() const -> const std::unordered_map<String, MaterialProperty>& {
        return m_properties;
    }

    auto bind_group() const -> void* { return m_bindGroup; }
    void set_bind_group(void* bg) { m_bindGroup = bg; }

private:
    Shader* m_shader = nullptr;
    std::unordered_map<String, MaterialProperty> m_properties;
    void* m_bindGroup = nullptr;
};

} // namespace firefly
