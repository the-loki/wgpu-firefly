module;
#include <glm/glm.hpp>
#include <string>
#include <cstring>

module firefly.renderer.material;

namespace firefly {

void Material::set_float(const String& name, f32 value) {
    MaterialProperty prop;
    prop.type = MaterialProperty::Type::Float;
    prop.floatVal = value;
    m_properties[name] = prop;
}

void Material::set_vec3(const String& name, const glm::vec3& value) {
    MaterialProperty prop;
    prop.type = MaterialProperty::Type::Vec3;
    std::memcpy(prop.vec3Val, &value, sizeof(glm::vec3));
    m_properties[name] = prop;
}

void Material::set_vec4(const String& name, const glm::vec4& value) {
    MaterialProperty prop;
    prop.type = MaterialProperty::Type::Vec4;
    std::memcpy(prop.vec4Val, &value, sizeof(glm::vec4));
    m_properties[name] = prop;
}

void Material::set_texture(const String& name, Texture* tex) {
    MaterialProperty prop;
    prop.type = MaterialProperty::Type::Texture;
    prop.textureVal = tex;
    m_properties[name] = prop;
}

} // namespace firefly
