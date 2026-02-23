module;
#define WGPU_SHARED_LIBRARY
#include <webgpu/webgpu.h>

module firefly.renderer.command;

namespace firefly {

CommandBuffer::~CommandBuffer() {
    if (m_renderPass) wgpuRenderPassEncoderRelease(static_cast<WGPURenderPassEncoder>(m_renderPass));
    if (m_encoder) wgpuCommandEncoderRelease(static_cast<WGPUCommandEncoder>(m_encoder));
}

void CommandBuffer::begin_render_pass(void* textureView, void* depthView) {
    auto colorAttachment = WGPURenderPassColorAttachment{};
    colorAttachment.view = static_cast<WGPUTextureView>(textureView);
    colorAttachment.loadOp = WGPULoadOp_Clear;
    colorAttachment.storeOp = WGPUStoreOp_Store;
    colorAttachment.clearValue = {0.1, 0.1, 0.1, 1.0};
    colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

    auto depthAttachment = WGPURenderPassDepthStencilAttachment{};
    auto passDesc = WGPURenderPassDescriptor{};
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments = &colorAttachment;

    if (depthView) {
        depthAttachment.view = static_cast<WGPUTextureView>(depthView);
        depthAttachment.depthLoadOp = WGPULoadOp_Clear;
        depthAttachment.depthStoreOp = WGPUStoreOp_Store;
        depthAttachment.depthClearValue = 1.0f;
        passDesc.depthStencilAttachment = &depthAttachment;
    }

    m_renderPass = wgpuCommandEncoderBeginRenderPass(
        static_cast<WGPUCommandEncoder>(m_encoder), &passDesc);
}

void CommandBuffer::end_render_pass() {
    if (m_renderPass) {
        wgpuRenderPassEncoderEnd(static_cast<WGPURenderPassEncoder>(m_renderPass));
        wgpuRenderPassEncoderRelease(static_cast<WGPURenderPassEncoder>(m_renderPass));
        m_renderPass = nullptr;
    }
}

void CommandBuffer::set_pipeline(const RenderPipeline& pipeline) {
    if (m_renderPass) {
        wgpuRenderPassEncoderSetPipeline(
            static_cast<WGPURenderPassEncoder>(m_renderPass),
            static_cast<WGPURenderPipeline>(pipeline.native_handle()));
    }
}

void CommandBuffer::set_vertex_buffer(u32 slot, const Buffer& buffer) {
    if (m_renderPass) {
        wgpuRenderPassEncoderSetVertexBuffer(
            static_cast<WGPURenderPassEncoder>(m_renderPass),
            slot, static_cast<WGPUBuffer>(buffer.native_handle()),
            0, buffer.size());
    }
}

void CommandBuffer::set_index_buffer(const Buffer& buffer) {
    if (m_renderPass) {
        wgpuRenderPassEncoderSetIndexBuffer(
            static_cast<WGPURenderPassEncoder>(m_renderPass),
            static_cast<WGPUBuffer>(buffer.native_handle()),
            WGPUIndexFormat_Uint32, 0, buffer.size());
    }
}

void CommandBuffer::set_bind_group(u32 index, void* bindGroup) {
    if (m_renderPass) {
        wgpuRenderPassEncoderSetBindGroup(
            static_cast<WGPURenderPassEncoder>(m_renderPass),
            index, static_cast<WGPUBindGroup>(bindGroup), 0, nullptr);
    }
}

void CommandBuffer::draw(u32 vertexCount, u32 instanceCount,
                         u32 firstVertex, u32 firstInstance) {
    if (m_renderPass) {
        wgpuRenderPassEncoderDraw(
            static_cast<WGPURenderPassEncoder>(m_renderPass),
            vertexCount, instanceCount, firstVertex, firstInstance);
    }
}

void CommandBuffer::draw_indexed(u32 indexCount, u32 instanceCount,
                                 u32 firstIndex, i32 baseVertex,
                                 u32 firstInstance) {
    if (m_renderPass) {
        wgpuRenderPassEncoderDrawIndexed(
            static_cast<WGPURenderPassEncoder>(m_renderPass),
            indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
    }
}

auto CommandBuffer::finish() -> void* {
    auto cmdBuffer = wgpuCommandEncoderFinish(
        static_cast<WGPUCommandEncoder>(m_encoder), nullptr);
    wgpuCommandEncoderRelease(static_cast<WGPUCommandEncoder>(m_encoder));
    m_encoder = nullptr;
    return cmdBuffer;
}

} // namespace firefly
