module;
#define WGPU_SHARED_LIBRARY
#include <webgpu/webgpu.h>

module firefly.renderer.sampler;

namespace firefly {

Sampler::~Sampler() {
    if (m_sampler) {
        wgpuSamplerRelease(static_cast<WGPUSampler>(m_sampler));
    }
}

Sampler::Sampler(Sampler&& other) noexcept
    : m_sampler(other.m_sampler) {
    other.m_sampler = nullptr;
}

Sampler& Sampler::operator=(Sampler&& other) noexcept {
    if (this != &other) {
        if (m_sampler) {
            wgpuSamplerRelease(static_cast<WGPUSampler>(m_sampler));
        }
        m_sampler = other.m_sampler;
        other.m_sampler = nullptr;
    }
    return *this;
}

} // namespace firefly
