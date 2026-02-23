// Firefly Engine - Hello Triangle Example (ECS-driven)
// All engine subsystems run as Flecs systems. The main loop is world.progress().

import firefly.core.app;
import firefly.core.types;
import firefly.ecs.engine_components;
import firefly.ecs.phases;
import firefly.core.logger;
import firefly.renderer.device;
import firefly.renderer.sampler;
import firefly.renderer.shader;
import firefly.renderer.pipeline;
import firefly.renderer.render_graph;

#include <flecs.h>
#include <algorithm>

// Helper to get typed RenderDevice from RenderState
static auto device_from(firefly::ecs::RenderState* rs) -> firefly::RenderDevice* {
    return static_cast<firefly::RenderDevice*>(rs->device.get());
}

static const char* TRIANGLE_SHADER = R"(
@group(0) @binding(0)
var historyTex: texture_2d<f32>;
@group(0) @binding(1)
var historySampler: sampler;

@vertex
fn vs_main(@builtin(vertex_index) idx: u32) -> @builtin(position) vec4f {
    var pos = array<vec2f, 3>(
        vec2f( 0.0,  0.5),
        vec2f(-0.5, -0.5),
        vec2f( 0.5, -0.5),
    );
    return vec4f(pos[idx], 0.0, 1.0);
}

@fragment
fn fs_main(@builtin(position) pos: vec4f) -> @location(0) vec4f {
    let dims = vec2f(textureDimensions(historyTex));
    let uv = clamp(pos.xy / max(dims, vec2f(1.0, 1.0)), vec2f(0.0, 0.0), vec2f(1.0, 1.0));
    return textureSampleLevel(historyTex, historySampler, uv, 0.0);
}
)";

static const char* TRIANGLE_COMPUTE_SHADER = R"(
@group(0) @binding(0)
var historyOut: texture_storage_2d<rgba8unorm, write>;

@compute @workgroup_size(8, 8, 1)
fn cs_main(@builtin(global_invocation_id) gid: vec3<u32>) {
    let dims = textureDimensions(historyOut);
    if (gid.x >= dims.x || gid.y >= dims.y) {
        return;
    }

    let uv = vec2f(
        f32(gid.x) / f32(max(dims.x, 1u)),
        f32(gid.y) / f32(max(dims.y, 1u))
    );
    textureStore(historyOut, vec2i(gid.xy), vec4f(uv, 0.2, 1.0));
}
)";

// Triangle-specific resources as a singleton component
struct TriangleResources {
    firefly::Ptr<firefly::Shader> shader;
    firefly::Ptr<firefly::RenderPipeline> pipeline;
    firefly::Ptr<firefly::Shader> computeShader;
    firefly::Ptr<firefly::ComputePipeline> computePipeline;
    firefly::Ptr<firefly::Sampler> sampler;
    firefly::u32 graphWidth = 0;
    firefly::u32 graphHeight = 0;
};

static auto build_triangle_graph(
    firefly::RenderPipeline& pipeline,
    firefly::ComputePipeline& computePipeline,
    firefly::Sampler& sampler,
    firefly::u32 width,
    firefly::u32 height) -> firefly::SharedPtr<firefly::RenderGraph> {
    auto graph = firefly::SharedPtr<firefly::RenderGraph>(new firefly::RenderGraph());
    auto backbuffer = graph->import_backbuffer("Backbuffer");
    firefly::TextureDesc historyDesc;
    historyDesc.label = "TriangleHistory";
    historyDesc.width = 0;
    historyDesc.height = 0;
    historyDesc.format = firefly::TextureFormat::RGBA8Unorm;
    historyDesc.usage = firefly::TextureUsage::RenderAttachment |
        firefly::TextureUsage::TextureBinding |
        firefly::TextureUsage::StorageBinding;
    auto history = graph->create_texture("TriangleHistory", historyDesc);

    firefly::ComputeDispatchPassDesc computeDesc;
    computeDesc.pass.name = "TrianglePrepare";
    computeDesc.pipeline = &computePipeline;
    computeDesc.bindings = {
        {
            .group = 0,
            .binding = 0,
            .resource = history,
            .access = firefly::RenderGraphBindingAccess::Write,
        },
    };
    computeDesc.groupCountX = std::max(1u, (width + 7u) / 8u);
    computeDesc.groupCountY = std::max(1u, (height + 7u) / 8u);
    auto compute = graph->add_compute_dispatch_pass(computeDesc);
    if (!compute.valid()) {
        firefly::Logger::error("Failed to create triangle compute pass");
        return {};
    }

    firefly::RasterDrawPassDesc passDesc;
    passDesc.pass.name = "TrianglePass";
    passDesc.pass.colorTarget = backbuffer;
    passDesc.pass.clearColor = true;
    passDesc.pass.clearR = 0.1f;
    passDesc.pass.clearG = 0.1f;
    passDesc.pass.clearB = 0.1f;
    passDesc.pass.clearA = 1.0f;
    passDesc.pipeline = &pipeline;
    passDesc.bindings = {
        {
            .group = 0,
            .binding = 0,
            .resource = history,
            .access = firefly::RenderGraphBindingAccess::Read,
        },
    };
    passDesc.samplerBindings = {
        {
            .group = 0,
            .binding = 1,
            .sampler = &sampler,
        },
    };
    passDesc.vertexCount = 3;

    auto pass = graph->add_raster_draw_pass(passDesc);
    if (!pass.valid()) {
        firefly::Logger::error("Failed to create triangle render-graph pass");
        return {};
    }

    auto compileResult = graph->compile();
    if (compileResult.is_error()) {
        firefly::Logger::error("Failed to compile render graph: {}", compileResult.error());
        return {};
    }
    auto dumpResult = graph->debug_dump_text();
    if (dumpResult.is_ok()) {
        firefly::Logger::debug("{}", dumpResult.value());
    }
    return graph;
}

int main() {
    firefly::App app;
    app.configure({
        .title = "Firefly - Hello Triangle",
        .width = 800,
        .height = 600,
        .enable_render_graph_diagnostics = true,
        .render_graph_diagnostics_interval_frames = 240,
    });

    app.setup([](flecs::world& world) {
        // Create shader/pipeline after render device init (OnStart)
        world.system("CreateTriangleResources")
            .kind(flecs::OnStart)
            .run([](flecs::iter& it) {
                auto w = it.world();
                auto* render = w.get_mut<firefly::ecs::RenderState>();
                if (!render || !render->initialized) return;
                auto* dev = device_from(render);

                firefly::ShaderDesc sd;
                sd.label = "TriangleShader";
                sd.wgslCode = TRIANGLE_SHADER;
                auto shader = dev->create_shader(sd);

                firefly::PipelineDesc pd;
                pd.label = "TrianglePipeline";
                pd.vertexShader = shader.get();
                pd.depthTest = false;
                pd.cullMode = firefly::CullMode::None;
                auto pipeline = dev->create_pipeline(pd);

                firefly::ShaderDesc csd;
                csd.label = "TriangleComputeShader";
                csd.wgslCode = TRIANGLE_COMPUTE_SHADER;
                auto computeShader = dev->create_shader(csd);

                firefly::ComputePipelineDesc cpd;
                cpd.label = "TriangleComputePipeline";
                cpd.computeShader = computeShader.get();
                auto computePipeline = dev->create_compute_pipeline(cpd);

                firefly::SamplerDesc samplerDesc;
                samplerDesc.magFilter = firefly::FilterMode::Linear;
                samplerDesc.minFilter = firefly::FilterMode::Linear;
                auto sampler = dev->create_sampler(samplerDesc);

                w.set<TriangleResources>({
                    std::move(shader), std::move(pipeline),
                    std::move(computeShader), std::move(computePipeline),
                    std::move(sampler),
                    0,
                    0
                });

                auto* res = w.get_mut<TriangleResources>();
                if (!res || !res->pipeline || !res->computePipeline || !res->sampler) {
                    firefly::Logger::error("Triangle resources are unavailable after setup");
                    return;
                }
                auto* window = w.get<firefly::ecs::WindowState>();
                const firefly::u32 graphWidth = window ? window->width : 800;
                const firefly::u32 graphHeight = window ? window->height : 600;
                auto graph = build_triangle_graph(
                    *res->pipeline,
                    *res->computePipeline,
                    *res->sampler,
                    graphWidth,
                    graphHeight
                );
                if (!graph) {
                    return;
                }
                render->graph = std::move(graph);
                render->auto_execute_graph = true;
                res->graphWidth = graphWidth;
                res->graphHeight = graphHeight;
            });

        const auto* phases = world.get<firefly::ecs::EnginePhases>();
        world.system("ResizeTriangleGraph")
            .kind(phases->render_begin)
            .run([](flecs::iter& it) {
                auto w = it.world();
                auto* render = w.get_mut<firefly::ecs::RenderState>();
                auto* res = w.get_mut<TriangleResources>();
                const auto* window = w.get<firefly::ecs::WindowState>();
                if (!render || !render->initialized || !res || !window) return;
                if (!res->pipeline || !res->computePipeline || !res->sampler) return;
                if (window->width == 0 || window->height == 0) return;

                if (res->graphWidth == window->width && res->graphHeight == window->height) {
                    return;
                }

                auto graph = build_triangle_graph(
                    *res->pipeline,
                    *res->computePipeline,
                    *res->sampler,
                    window->width,
                    window->height
                );
                if (!graph) {
                    return;
                }
                render->pending_graph = std::move(graph);
                res->graphWidth = window->width;
                res->graphHeight = window->height;
            });

    });

    return app.run();
}
