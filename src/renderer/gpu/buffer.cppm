module;
#include <cstdint>
#include <string>

export module firefly.renderer.buffer;

import firefly.core.types;

export namespace firefly {

enum class BufferUsage : u32 {
    Vertex   = 0x0020,
    Index    = 0x0010,
    Uniform  = 0x0040,
    Storage  = 0x0080,
    CopySrc  = 0x0004,
    CopyDst  = 0x0008,
};

struct BufferDesc {
    String label;
    u64 size = 0;
    BufferUsage usage = BufferUsage::Vertex;
    bool mappedAtCreation = false;
};

class Buffer {
public:
    Buffer() = default;
    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;

    auto size() const -> u64 { return m_size; }
    auto usage() const -> BufferUsage { return m_usage; }
    auto native_handle() const -> void* { return m_buffer; }

    void write(const void* data, u64 size, u64 offset = 0);

private:
    friend class RenderDevice;
    void* m_buffer = nullptr;
    void* m_device = nullptr;
    void* m_queue = nullptr;
    u64 m_size = 0;
    BufferUsage m_usage = BufferUsage::Vertex;
};

} // namespace firefly
