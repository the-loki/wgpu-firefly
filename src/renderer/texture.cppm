module;
#include <cstdint>
#include <string>

export module firefly.renderer.texture;

import firefly.core.types;

export namespace firefly {

enum class TextureFormat : u32 {
    RGBA8Unorm = 0x00000004,
    BGRA8Unorm = 0x00000017,
    Depth24PlusStencil8 = 0x00000028,
    Depth32Float = 0x00000027,
};

enum class TextureUsage : u32 {
    CopySrc = 0x01,
    CopyDst = 0x02,
    TextureBinding = 0x04,
    StorageBinding = 0x08,
    RenderAttachment = 0x10,
};

struct TextureDesc {
    String label;
    u32 width = 1;
    u32 height = 1;
    TextureFormat format = TextureFormat::RGBA8Unorm;
    TextureUsage usage = TextureUsage::TextureBinding;
};

class Texture {
public:
    Texture() = default;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    auto width() const -> u32 { return m_width; }
    auto height() const -> u32 { return m_height; }
    auto format() const -> TextureFormat { return m_format; }
    auto native_handle() const -> void* { return m_texture; }
    auto view() const -> void* { return m_view; }

private:
    friend class RenderDevice;
    void* m_texture = nullptr;
    void* m_view = nullptr;
    u32 m_width = 0;
    u32 m_height = 0;
    TextureFormat m_format = TextureFormat::RGBA8Unorm;
};

} // namespace firefly
