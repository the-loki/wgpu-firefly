module;
#include <cstdint>
#include <string>
#include <vector>

export module firefly.renderer.shader;

import firefly.core.types;

export namespace firefly {

struct ShaderDesc {
    String label;
    String wgslCode;
};

class Shader {
public:
    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    auto native_handle() const -> void* { return m_module; }

private:
    friend class RenderDevice;
    void* m_module = nullptr;
};

} // namespace firefly
