module;
#include <cstdint>

export module firefly.renderer.command;

import firefly.core.types;
import firefly.renderer.buffer;
import firefly.renderer.pipeline;

export namespace firefly {

class CommandBuffer {
public:
    CommandBuffer() = default;
    ~CommandBuffer();

    CommandBuffer(const CommandBuffer&) = delete;
    CommandBuffer& operator=(const CommandBuffer&) = delete;

    void begin_render_pass(void* textureView, void* depthView = nullptr);
    void begin_render_pass(void* textureView, void* depthView,
                           f32 clearR, f32 clearG, f32 clearB, f32 clearA,
                           bool clearColor = true,
                           bool clearDepth = true,
                           f32 depthClearValue = 1.0f);
    void end_render_pass();
    void begin_compute_pass();
    void end_compute_pass();
    void set_compute_pipeline(const ComputePipeline& pipeline);
    void dispatch(u32 groupCountX, u32 groupCountY = 1, u32 groupCountZ = 1);

    void set_pipeline(const RenderPipeline& pipeline);
    void set_vertex_buffer(u32 slot, const Buffer& buffer);
    void set_index_buffer(const Buffer& buffer);
    void set_bind_group(u32 index, void* bindGroup);

    void draw(u32 vertexCount, u32 instanceCount = 1,
              u32 firstVertex = 0, u32 firstInstance = 0);
    void draw_indexed(u32 indexCount, u32 instanceCount = 1,
                      u32 firstIndex = 0, i32 baseVertex = 0,
                      u32 firstInstance = 0);

    auto finish() -> void*;

    auto native_encoder() const -> void* { return m_encoder; }

private:
    friend class RenderDevice;
    void* m_encoder = nullptr;
    void* m_renderPass = nullptr;
    void* m_computePass = nullptr;
    void* m_device = nullptr;
};

} // namespace firefly
