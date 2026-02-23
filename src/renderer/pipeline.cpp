module;
#define WGPU_SHARED_LIBRARY
#include <webgpu/webgpu.h>

module firefly.renderer.pipeline;

namespace firefly {

RenderPipeline::~RenderPipeline() {
    if (m_pipeline) wgpuRenderPipelineRelease(static_cast<WGPURenderPipeline>(m_pipeline));
}

RenderPipeline::RenderPipeline(RenderPipeline&& other) noexcept
    : m_pipeline(other.m_pipeline) {
    other.m_pipeline = nullptr;
}

RenderPipeline& RenderPipeline::operator=(RenderPipeline&& other) noexcept {
    if (this != &other) {
        if (m_pipeline) wgpuRenderPipelineRelease(static_cast<WGPURenderPipeline>(m_pipeline));
        m_pipeline = other.m_pipeline;
        other.m_pipeline = nullptr;
    }
    return *this;
}

ComputePipeline::~ComputePipeline() {
    if (m_pipeline) wgpuComputePipelineRelease(static_cast<WGPUComputePipeline>(m_pipeline));
}

ComputePipeline::ComputePipeline(ComputePipeline&& other) noexcept
    : m_pipeline(other.m_pipeline) {
    other.m_pipeline = nullptr;
}

ComputePipeline& ComputePipeline::operator=(ComputePipeline&& other) noexcept {
    if (this != &other) {
        if (m_pipeline) wgpuComputePipelineRelease(static_cast<WGPUComputePipeline>(m_pipeline));
        m_pipeline = other.m_pipeline;
        other.m_pipeline = nullptr;
    }
    return *this;
}

} // namespace firefly
