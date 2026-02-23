module;
#include <cstdint>
#include <string>

export module firefly.renderer.pipeline;

import firefly.core.types;
import firefly.renderer.shader;
import firefly.renderer.texture;

export namespace firefly {

enum class PrimitiveTopology : u32 {
    TriangleList = 0,
    TriangleStrip = 1,
    LineList = 2,
    PointList = 3,
};

enum class CullMode : u32 { None = 0, Front = 1, Back = 2 };

struct VertexAttribute {
    u32 location = 0;
    u32 offset = 0;
    u32 format = 0; // WGPUVertexFormat
};

struct PipelineDesc {
    String label;
    Shader* vertexShader = nullptr;
    Shader* fragmentShader = nullptr;
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    CullMode cullMode = CullMode::Back;
    TextureFormat colorFormat = TextureFormat::BGRA8Unorm;
    bool depthTest = true;
    TextureFormat depthFormat = TextureFormat::Depth24PlusStencil8;
};

class RenderPipeline {
public:
    RenderPipeline() = default;
    ~RenderPipeline();

    RenderPipeline(const RenderPipeline&) = delete;
    RenderPipeline& operator=(const RenderPipeline&) = delete;
    RenderPipeline(RenderPipeline&& other) noexcept;
    RenderPipeline& operator=(RenderPipeline&& other) noexcept;

    auto native_handle() const -> void* { return m_pipeline; }

private:
    friend class RenderDevice;
    void* m_pipeline = nullptr;
};

} // namespace firefly
