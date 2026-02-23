module;
#define WGPU_SHARED_LIBRARY
#include <webgpu/webgpu.h>

module firefly.renderer.shader;

namespace firefly {

Shader::~Shader() {
    if (m_module) wgpuShaderModuleRelease(static_cast<WGPUShaderModule>(m_module));
}

Shader::Shader(Shader&& other) noexcept : m_module(other.m_module) {
    other.m_module = nullptr;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        if (m_module) wgpuShaderModuleRelease(static_cast<WGPUShaderModule>(m_module));
        m_module = other.m_module;
        other.m_module = nullptr;
    }
    return *this;
}

} // namespace firefly
