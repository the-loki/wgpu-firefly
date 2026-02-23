module;
#include <cstdint>
#include <string>
#include <vector>

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

enum class VertexFormat : u32 {
    Float32 = 0,
    Float32x2 = 1,
    Float32x3 = 2,
    Float32x4 = 3,
    Uint32 = 4,
};

enum class VertexStepMode : u32 {
    Vertex = 0,
    Instance = 1,
};

struct VertexAttribute {
    u32 location = 0;
    u32 offset = 0;
    VertexFormat format = VertexFormat::Float32x3;
};

struct VertexBufferLayoutDesc {
    u32 arrayStride = 0;
    VertexStepMode stepMode = VertexStepMode::Vertex;
    std::vector<VertexAttribute> attributes;
};

struct PipelineDesc {
    String label;
    Shader* vertexShader = nullptr;
    Shader* fragmentShader = nullptr;
    bool enableFragment = true;
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    CullMode cullMode = CullMode::Back;
    TextureFormat colorFormat = TextureFormat::BGRA8Unorm;
    bool depthTest = true;
    TextureFormat depthFormat = TextureFormat::Depth24PlusStencil8;
    std::vector<VertexBufferLayoutDesc> vertexBuffers;
};

struct ComputePipelineDesc {
    String label;
    Shader* computeShader = nullptr;
    String entryPoint = "cs_main";
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

class ComputePipeline {
public:
    ComputePipeline() = default;
    ~ComputePipeline();

    ComputePipeline(const ComputePipeline&) = delete;
    ComputePipeline& operator=(const ComputePipeline&) = delete;
    ComputePipeline(ComputePipeline&& other) noexcept;
    ComputePipeline& operator=(ComputePipeline&& other) noexcept;

    auto native_handle() const -> void* { return m_pipeline; }

private:
    friend class RenderDevice;
    void* m_pipeline = nullptr;
};

} // namespace firefly
