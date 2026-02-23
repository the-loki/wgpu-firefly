module;
#define WGPU_SHARED_LIBRARY
#include <webgpu/webgpu.h>

module firefly.renderer.command;

namespace firefly {

CommandBuffer::~CommandBuffer() {
    if (m_computePass) wgpuComputePassEncoderRelease(static_cast<WGPUComputePassEncoder>(m_computePass));
    if (m_renderPass) wgpuRenderPassEncoderRelease(static_cast<WGPURenderPassEncoder>(m_renderPass));
    if (m_encoder) wgpuCommandEncoderRelease(static_cast<WGPUCommandEncoder>(m_encoder));
}

void CommandBuffer::begin_render_pass(void* textureView, void* depthView) {
    begin_render_pass(textureView, depthView, 0.1f, 0.1f, 0.1f, 1.0f, true);
}

void CommandBuffer::begin_render_pass(void* textureView, void* depthView,
                                      f32 clearR, f32 clearG, f32 clearB, f32 clearA,
                                      bool clearColor, bool clearDepth, f32 depthClearValue) {
    auto depthAttachment = WGPURenderPassDepthStencilAttachment{};
    auto passDesc = WGPURenderPassDescriptor{};
    WGPURenderPassColorAttachment colorAttachment{};
    if (textureView) {
        colorAttachment.view = static_cast<WGPUTextureView>(textureView);
        colorAttachment.loadOp = clearColor ? WGPULoadOp_Clear : WGPULoadOp_Load;
        colorAttachment.storeOp = WGPUStoreOp_Store;
        colorAttachment.clearValue = {clearR, clearG, clearB, clearA};
        colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
        passDesc.colorAttachmentCount = 1;
        passDesc.colorAttachments = &colorAttachment;
    } else {
        passDesc.colorAttachmentCount = 0;
        passDesc.colorAttachments = nullptr;
    }

    if (depthView) {
        depthAttachment.view = static_cast<WGPUTextureView>(depthView);
        depthAttachment.depthLoadOp = clearDepth ? WGPULoadOp_Clear : WGPULoadOp_Load;
        depthAttachment.depthStoreOp = WGPUStoreOp_Store;
        depthAttachment.depthClearValue = depthClearValue;
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

void CommandBuffer::begin_compute_pass() {
    if (!m_encoder || m_computePass || m_renderPass) return;
    WGPUComputePassDescriptor passDesc{};
    m_computePass = wgpuCommandEncoderBeginComputePass(
        static_cast<WGPUCommandEncoder>(m_encoder), &passDesc);
}

void CommandBuffer::end_compute_pass() {
    if (m_computePass) {
        wgpuComputePassEncoderEnd(static_cast<WGPUComputePassEncoder>(m_computePass));
        wgpuComputePassEncoderRelease(static_cast<WGPUComputePassEncoder>(m_computePass));
        m_computePass = nullptr;
    }
}

void CommandBuffer::set_compute_pipeline(const ComputePipeline& pipeline) {
    if (m_computePass && pipeline.native_handle()) {
        wgpuComputePassEncoderSetPipeline(
            static_cast<WGPUComputePassEncoder>(m_computePass),
            static_cast<WGPUComputePipeline>(pipeline.native_handle()));
    }
}

void CommandBuffer::dispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ) {
    if (m_computePass) {
        wgpuComputePassEncoderDispatchWorkgroups(
            static_cast<WGPUComputePassEncoder>(m_computePass),
            groupCountX, groupCountY, groupCountZ);
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
    } else if (m_computePass) {
        wgpuComputePassEncoderSetBindGroup(
            static_cast<WGPUComputePassEncoder>(m_computePass),
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
    // Ensure passes are properly closed even if caller forgot.
    end_compute_pass();
    end_render_pass();
    auto cmdBuffer = wgpuCommandEncoderFinish(
        static_cast<WGPUCommandEncoder>(m_encoder), nullptr);
    wgpuCommandEncoderRelease(static_cast<WGPUCommandEncoder>(m_encoder));
    m_encoder = nullptr;
    return cmdBuffer;
}

} // namespace firefly
