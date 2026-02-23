module;
#define WGPU_SHARED_LIBRARY
#include <webgpu/webgpu.h>

module firefly.renderer.texture;

namespace firefly {

Texture::~Texture() {
    if (m_view) wgpuTextureViewRelease(static_cast<WGPUTextureView>(m_view));
    if (m_texture) wgpuTextureRelease(static_cast<WGPUTexture>(m_texture));
}

Texture::Texture(Texture&& other) noexcept
    : m_texture(other.m_texture), m_view(other.m_view),
      m_width(other.m_width), m_height(other.m_height), m_format(other.m_format) {
    other.m_texture = nullptr;
    other.m_view = nullptr;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        if (m_view) wgpuTextureViewRelease(static_cast<WGPUTextureView>(m_view));
        if (m_texture) wgpuTextureRelease(static_cast<WGPUTexture>(m_texture));
        m_texture = other.m_texture;
        m_view = other.m_view;
        m_width = other.m_width;
        m_height = other.m_height;
        m_format = other.m_format;
        other.m_texture = nullptr;
        other.m_view = nullptr;
    }
    return *this;
}

} // namespace firefly
