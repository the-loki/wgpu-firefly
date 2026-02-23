module;
#include <cstdint>

export module firefly.renderer.sampler;

import firefly.core.types;

export namespace firefly {

enum class AddressMode : u32 {
    ClampToEdge = 0,
    Repeat = 1,
    MirrorRepeat = 2,
};

enum class FilterMode : u32 {
    Nearest = 0,
    Linear = 1,
};

enum class CompareFunction : u32 {
    Never = 1,
    Less = 2,
    LessEqual = 3,
    Greater = 4,
    GreaterEqual = 5,
    Equal = 6,
    NotEqual = 7,
    Always = 8,
};

struct SamplerDesc {
    AddressMode addressModeU = AddressMode::ClampToEdge;
    AddressMode addressModeV = AddressMode::ClampToEdge;
    AddressMode addressModeW = AddressMode::ClampToEdge;
    FilterMode magFilter = FilterMode::Linear;
    FilterMode minFilter = FilterMode::Linear;
    FilterMode mipmapFilter = FilterMode::Nearest;
    bool enableCompare = false;
    CompareFunction compareFunction = CompareFunction::LessEqual;
};

class Sampler {
public:
    Sampler() = default;
    ~Sampler();

    Sampler(const Sampler&) = delete;
    Sampler& operator=(const Sampler&) = delete;
    Sampler(Sampler&& other) noexcept;
    Sampler& operator=(Sampler&& other) noexcept;

    auto native_handle() const -> void* { return m_sampler; }

private:
    friend class RenderDevice;
    void* m_sampler = nullptr;
};

} // namespace firefly
