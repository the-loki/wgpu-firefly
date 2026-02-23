module;
#define WGPU_SHARED_LIBRARY
#include <webgpu/webgpu.h>

module firefly.renderer.buffer;

namespace firefly {

Buffer::~Buffer() {
    if (m_buffer) {
        wgpuBufferRelease(static_cast<WGPUBuffer>(m_buffer));
    }
}

Buffer::Buffer(Buffer&& other) noexcept
    : m_buffer(other.m_buffer), m_device(other.m_device),
      m_queue(other.m_queue), m_size(other.m_size), m_usage(other.m_usage) {
    other.m_buffer = nullptr;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        if (m_buffer) wgpuBufferRelease(static_cast<WGPUBuffer>(m_buffer));
        m_buffer = other.m_buffer;
        m_device = other.m_device;
        m_queue = other.m_queue;
        m_size = other.m_size;
        m_usage = other.m_usage;
        other.m_buffer = nullptr;
    }
    return *this;
}

void Buffer::write(const void* data, u64 size, u64 offset) {
    if (m_buffer && m_queue) {
        wgpuQueueWriteBuffer(
            static_cast<WGPUQueue>(m_queue),
            static_cast<WGPUBuffer>(m_buffer),
            offset, data, static_cast<size_t>(size));
    }
}

} // namespace firefly
