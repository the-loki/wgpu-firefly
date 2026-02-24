#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

import firefly.renderer.buffer;
import firefly.renderer.texture;
import firefly.renderer.sampler;
import firefly.renderer.shader;
import firefly.renderer.pipeline;
import firefly.renderer.material;
import firefly.renderer.mesh;
import firefly.renderer.camera;
import firefly.renderer.device;
import firefly.renderer.render_graph;
import firefly.renderer.forward_renderer;
import firefly.core.types;

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <string>

// ============================================================
// Phase 5: Renderer 模块测试 (不需要 GPU 的部分)
// ============================================================

TEST_SUITE("BufferDesc") {
    TEST_CASE("default values") {
        firefly::BufferDesc desc;
        CHECK(desc.size == 0);
        CHECK(desc.usage == firefly::BufferUsage::Vertex);
        CHECK_FALSE(desc.mappedAtCreation);
    }

    TEST_CASE("custom values") {
        firefly::BufferDesc desc;
        desc.label = "TestBuffer";
        desc.size = 1024;
        desc.usage = firefly::BufferUsage::Uniform;
        CHECK(desc.label == "TestBuffer");
        CHECK(desc.size == 1024);
    }
}

TEST_SUITE("TextureDesc") {
    TEST_CASE("default values") {
        firefly::TextureDesc desc;
        CHECK(desc.width == 1);
        CHECK(desc.height == 1);
        CHECK(desc.format == firefly::TextureFormat::RGBA8Unorm);
    }

    TEST_CASE("usage bitmask helpers") {
        const auto usage = firefly::TextureUsage::RenderAttachment |
            firefly::TextureUsage::StorageBinding;
        CHECK(firefly::has_usage(usage, firefly::TextureUsage::RenderAttachment));
        CHECK(firefly::has_usage(usage, firefly::TextureUsage::StorageBinding));
        CHECK_FALSE(firefly::has_usage(usage, firefly::TextureUsage::TextureBinding));
    }
}

TEST_SUITE("SamplerDesc") {
    TEST_CASE("default values") {
        firefly::SamplerDesc desc;
        CHECK(desc.addressModeU == firefly::AddressMode::ClampToEdge);
        CHECK(desc.addressModeV == firefly::AddressMode::ClampToEdge);
        CHECK(desc.addressModeW == firefly::AddressMode::ClampToEdge);
        CHECK(desc.magFilter == firefly::FilterMode::Linear);
        CHECK(desc.minFilter == firefly::FilterMode::Linear);
        CHECK(desc.mipmapFilter == firefly::FilterMode::Nearest);
    }
}

TEST_SUITE("ShaderDesc") {
    TEST_CASE("wgsl code") {
        firefly::ShaderDesc desc;
        desc.label = "TestShader";
        desc.wgslCode = "@vertex fn vs_main() -> @builtin(position) vec4f { return vec4f(0); }";
        CHECK_FALSE(desc.wgslCode.empty());
    }
}

TEST_SUITE("PipelineDesc") {
    TEST_CASE("default values") {
        firefly::PipelineDesc desc;
        CHECK(desc.topology == firefly::PrimitiveTopology::TriangleList);
        CHECK(desc.cullMode == firefly::CullMode::Back);
        CHECK(desc.depthTest);
    }

    TEST_CASE("compute defaults") {
        firefly::ComputePipelineDesc desc;
        CHECK(desc.computeShader == nullptr);
        CHECK(desc.entryPoint == "cs_main");
    }

    TEST_CASE("vertex buffer layouts can be described") {
        firefly::PipelineDesc desc;
        firefly::VertexBufferLayoutDesc layout;
        layout.arrayStride = 32;
        layout.stepMode = firefly::VertexStepMode::Vertex;
        layout.attributes = {
            {.location = 0, .offset = 0, .format = firefly::VertexFormat::Float32x3},
            {.location = 1, .offset = 12, .format = firefly::VertexFormat::Float32x3},
            {.location = 2, .offset = 24, .format = firefly::VertexFormat::Float32x2},
        };
        desc.vertexBuffers = {layout};

        REQUIRE(desc.vertexBuffers.size() == 1);
        CHECK(desc.vertexBuffers[0].arrayStride == 32);
        CHECK(desc.vertexBuffers[0].attributes.size() == 3);
        CHECK(desc.vertexBuffers[0].attributes[2].format == firefly::VertexFormat::Float32x2);
    }
}

TEST_SUITE("Mesh") {
    TEST_CASE("vertices and indices") {
        firefly::Mesh mesh;
        std::vector<firefly::Vertex> verts = {
            {glm::vec3(-1, -1, 0)},
            {glm::vec3( 1, -1, 0)},
            {glm::vec3( 0,  1, 0)},
        };
        mesh.set_vertices(verts);
        mesh.set_indices({0, 1, 2});
        CHECK(mesh.vertices().size() == 3);
        CHECK(mesh.indices().size() == 3);
    }

    TEST_CASE("compute bounds") {
        firefly::Mesh mesh;
        std::vector<firefly::Vertex> verts = {
            {glm::vec3(-1, -2, -3)},
            {glm::vec3( 4,  5,  6)},
        };
        mesh.set_vertices(verts);
        mesh.compute_bounds();
        CHECK(mesh.bounding_min().x == doctest::Approx(-1.0f));
        CHECK(mesh.bounding_max().y == doctest::Approx(5.0f));
    }

    TEST_CASE("submesh") {
        firefly::Mesh mesh;
        firefly::SubMesh sub;
        sub.indexOffset = 0;
        sub.indexCount = 6;
        sub.materialIndex = 0;
        mesh.add_submesh(sub);
        CHECK(mesh.submeshes().size() == 1);
        CHECK(mesh.submeshes()[0].indexCount == 6);
    }
}

TEST_SUITE("Camera") {
    TEST_CASE("perspective projection") {
        firefly::Camera cam;
        cam.set_perspective(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
        auto proj = cam.projection_matrix();
        CHECK(proj[0][0] != doctest::Approx(0.0f));
        CHECK(proj[1][1] != doctest::Approx(0.0f));
    }

    TEST_CASE("look at") {
        firefly::Camera cam;
        cam.look_at(glm::vec3(0, 0, 5), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        CHECK(cam.position().z == doctest::Approx(5.0f));
        auto view = cam.view_matrix();
        CHECK(view[3][2] == doctest::Approx(-5.0f));
    }

    TEST_CASE("view projection") {
        firefly::Camera cam;
        cam.set_perspective(60.0f, 1.0f, 0.1f, 100.0f);
        cam.look_at(glm::vec3(0, 0, 5), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        auto vp = cam.view_projection();
        CHECK(vp != glm::mat4(1.0f));
    }
}

TEST_SUITE("Material") {
    TEST_CASE("set properties") {
        firefly::Material mat;
        mat.set_float("roughness", 0.5f);
        mat.set_vec3("albedo", glm::vec3(1.0f, 0.0f, 0.0f));
        CHECK(mat.properties().size() == 2);
    }

    TEST_CASE("shader assignment") {
        firefly::Material mat;
        CHECK(mat.shader() == nullptr);
    }
}

TEST_SUITE("RenderDevice") {
    TEST_CASE("default state") {
        firefly::RenderDevice device;
        CHECK(device.wgpu_device() == nullptr);
        CHECK(device.wgpu_surface() == nullptr);
        CHECK(device.surface_width() == 0);
        CHECK(device.surface_height() == 0);
    }
}

TEST_SUITE("RenderGraph") {
    TEST_CASE("single pass compile order") {
        firefly::RenderGraph graph;
        auto backbuffer = graph.import_backbuffer();

        firefly::RasterPassDesc desc;
        desc.name = "MainPass";
        desc.colorTarget = backbuffer;

        auto pass = graph.add_raster_pass(
            desc,
            [](firefly::RenderGraphPassContext&) {}
        );
        CHECK(pass.valid());

        auto result = graph.compile();
        CHECK(result.is_ok());
        CHECK(graph.pass_count() == 1);
        REQUIRE(graph.compiled_order().size() == 1);
        CHECK(graph.compiled_order()[0] == pass.id);
    }

    TEST_CASE("read dependency enforces pass order") {
        firefly::RenderGraph graph;
        auto backbuffer = graph.import_backbuffer();

        firefly::TextureDesc tempDesc;
        tempDesc.label = "LightingTarget";
        tempDesc.width = 1280;
        tempDesc.height = 720;
        tempDesc.format = firefly::TextureFormat::RGBA8Unorm;
        tempDesc.usage = firefly::TextureUsage::RenderAttachment;
        auto lighting = graph.create_texture("LightingTarget", tempDesc);

        firefly::RasterPassDesc gbufferDesc;
        gbufferDesc.name = "GBuffer";
        gbufferDesc.colorTarget = lighting;
        auto gbuffer = graph.add_raster_pass(
            gbufferDesc,
            [](firefly::RenderGraphPassContext&) {}
        );
        REQUIRE(gbuffer.valid());

        firefly::RasterPassDesc compositeDesc;
        compositeDesc.name = "Composite";
        compositeDesc.colorTarget = backbuffer;
        auto composite = graph.add_raster_pass(
            compositeDesc,
            [](firefly::RenderGraphPassContext&) {}
        );
        REQUIRE(composite.valid());

        auto readResult = graph.add_reads(composite, {lighting});
        CHECK(readResult.is_ok());

        auto compileResult = graph.compile();
        CHECK(compileResult.is_ok());
        REQUIRE(graph.compiled_order().size() == 2);
        CHECK(graph.compiled_order()[0] == gbuffer.id);
        CHECK(graph.compiled_order()[1] == composite.id);
    }

    TEST_CASE("transient read before write fails compile") {
        firefly::RenderGraph graph;
        auto backbuffer = graph.import_backbuffer();

        firefly::TextureDesc historyDesc;
        historyDesc.label = "History";
        historyDesc.width = 800;
        historyDesc.height = 600;
        historyDesc.format = firefly::TextureFormat::RGBA8Unorm;
        historyDesc.usage = firefly::TextureUsage::RenderAttachment;
        auto history = graph.create_texture("History", historyDesc);

        firefly::RasterPassDesc passDesc;
        passDesc.name = "TemporalPass";
        passDesc.colorTarget = backbuffer;
        auto pass = graph.add_raster_pass(
            passDesc,
            [](firefly::RenderGraphPassContext&) {}
        );
        REQUIRE(pass.valid());
        REQUIRE(graph.add_read(pass, history).is_ok());

        auto compileResult = graph.compile();
        CHECK(compileResult.is_error());
    }

    TEST_CASE("same pass read-write same resource is invalid") {
        firefly::RenderGraph graph;
        auto backbuffer = graph.import_backbuffer();

        firefly::RasterPassDesc passDesc;
        passDesc.name = "InvalidPass";
        passDesc.colorTarget = backbuffer;
        auto pass = graph.add_raster_pass(
            passDesc,
            [](firefly::RenderGraphPassContext&) {}
        );
        REQUIRE(pass.valid());

        REQUIRE(graph.add_read(pass, backbuffer).is_ok());
        auto compileResult = graph.compile();
        CHECK(compileResult.is_error());
    }

    TEST_CASE("unused pass is culled when not contributing to backbuffer") {
        firefly::RenderGraph graph;
        auto backbuffer = graph.import_backbuffer();

        firefly::TextureDesc tempDesc;
        tempDesc.label = "Unused";
        tempDesc.width = 640;
        tempDesc.height = 360;
        tempDesc.format = firefly::TextureFormat::RGBA8Unorm;
        tempDesc.usage = firefly::TextureUsage::RenderAttachment;
        auto unusedTarget = graph.create_texture("Unused", tempDesc);

        firefly::RasterPassDesc orphanDesc;
        orphanDesc.name = "Orphan";
        orphanDesc.colorTarget = unusedTarget;
        auto orphan = graph.add_raster_pass(
            orphanDesc,
            [](firefly::RenderGraphPassContext&) {}
        );
        REQUIRE(orphan.valid());

        firefly::RasterPassDesc mainDesc;
        mainDesc.name = "Main";
        mainDesc.colorTarget = backbuffer;
        auto main = graph.add_raster_pass(
            mainDesc,
            [](firefly::RenderGraphPassContext&) {}
        );
        REQUIRE(main.valid());

        auto compileResult = graph.compile();
        CHECK(compileResult.is_ok());
        REQUIRE(graph.compiled_order().size() == 1);
        CHECK(graph.compiled_order()[0] == main.id);
    }

    TEST_CASE("compute pass producer is ordered before raster consumer") {
        firefly::RenderGraph graph;
        auto backbuffer = graph.import_backbuffer();

        firefly::TextureDesc tempDesc;
        tempDesc.label = "History";
        tempDesc.width = 640;
        tempDesc.height = 360;
        tempDesc.format = firefly::TextureFormat::RGBA8Unorm;
        tempDesc.usage = firefly::TextureUsage::RenderAttachment |
            firefly::TextureUsage::TextureBinding |
            firefly::TextureUsage::StorageBinding;
        auto history = graph.create_texture("History", tempDesc);

        firefly::ComputePassDesc computeDesc;
        computeDesc.name = "HistoryBuild";
        computeDesc.writes = {history};
        auto compute = graph.add_compute_pass(
            computeDesc,
            [](firefly::RenderGraphPassContext&) {}
        );
        REQUIRE(compute.valid());

        firefly::RasterPassDesc presentDesc;
        presentDesc.name = "Present";
        presentDesc.colorTarget = backbuffer;
        auto present = graph.add_raster_pass(
            presentDesc,
            [](firefly::RenderGraphPassContext&) {}
        );
        REQUIRE(present.valid());
        REQUIRE(graph.add_read(present, history).is_ok());

        auto compileResult = graph.compile();
        CHECK(compileResult.is_ok());
        REQUIRE(graph.compiled_order().size() == 2);
        CHECK(graph.compiled_order()[0] == compute.id);
        CHECK(graph.compiled_order()[1] == present.id);
    }

    TEST_CASE("draw and dispatch helper passes wire bindings and dependencies") {
        firefly::RenderGraph graph;
        firefly::RenderPipeline rasterPipeline;
        firefly::ComputePipeline computePipeline;

        auto backbuffer = graph.import_backbuffer();

        firefly::TextureDesc tempDesc;
        tempDesc.label = "History";
        tempDesc.width = 640;
        tempDesc.height = 360;
        tempDesc.format = firefly::TextureFormat::RGBA8Unorm;
        tempDesc.usage = firefly::TextureUsage::RenderAttachment |
            firefly::TextureUsage::TextureBinding |
            firefly::TextureUsage::StorageBinding;
        auto history = graph.create_texture("History", tempDesc);

        firefly::ComputeDispatchPassDesc computeDesc;
        computeDesc.pass.name = "HistoryBuild";
        computeDesc.pipeline = &computePipeline;
        computeDesc.bindings = {{
            .group = 0,
            .binding = 0,
            .resource = history,
            .access = firefly::RenderGraphBindingAccess::Write,
        }};
        computeDesc.groupCountX = 1;
        auto compute = graph.add_compute_dispatch_pass(computeDesc);
        REQUIRE(compute.valid());

        firefly::RasterDrawPassDesc drawDesc;
        drawDesc.pass.name = "Present";
        drawDesc.pass.colorTarget = backbuffer;
        drawDesc.pipeline = &rasterPipeline;
        drawDesc.bindings = {{
            .group = 0,
            .binding = 0,
            .resource = history,
            .access = firefly::RenderGraphBindingAccess::Read,
        }};
        drawDesc.vertexCount = 3;
        auto draw = graph.add_raster_draw_pass(drawDesc);
        REQUIRE(draw.valid());

        auto compileResult = graph.compile();
        CHECK(compileResult.is_ok());
        REQUIRE(graph.compiled_order().size() == 2);
        CHECK(graph.compiled_order()[0] == compute.id);
        CHECK(graph.compiled_order()[1] == draw.id);
    }

    TEST_CASE("duplicate texture binding slot is rejected") {
        firefly::RenderGraph graph;
        firefly::RenderPipeline rasterPipeline;

        auto backbuffer = graph.import_backbuffer();
        firefly::TextureDesc tempDesc;
        tempDesc.width = 4;
        tempDesc.height = 4;
        tempDesc.format = firefly::TextureFormat::RGBA8Unorm;
        tempDesc.usage = firefly::TextureUsage::TextureBinding;
        auto history = graph.create_texture("History", tempDesc);

        firefly::RasterPassDesc passDesc;
        passDesc.name = "Present";
        passDesc.colorTarget = backbuffer;
        auto pass = graph.add_raster_pass(
            passDesc,
            [&](firefly::RenderGraphPassContext& ctx) {
                ctx.command().set_pipeline(rasterPipeline);
                ctx.command().draw(3);
            }
        );
        REQUIRE(pass.valid());

        auto first = graph.add_texture_binding(pass, {
            .group = 0,
            .binding = 0,
            .resource = history,
            .access = firefly::RenderGraphBindingAccess::Read,
        });
        CHECK(first.is_ok());

        auto duplicate = graph.add_texture_binding(pass, {
            .group = 0,
            .binding = 0,
            .resource = history,
            .access = firefly::RenderGraphBindingAccess::Read,
        });
        CHECK(duplicate.is_error());
    }

    TEST_CASE("buffer and sampler bindings can be registered for helper pass") {
        firefly::RenderGraph graph;
        firefly::RenderPipeline rasterPipeline;
        firefly::Buffer uniformBuffer;
        firefly::Sampler sampler;

        auto backbuffer = graph.import_backbuffer();

        firefly::RasterDrawPassDesc drawDesc;
        drawDesc.pass.name = "Present";
        drawDesc.pass.colorTarget = backbuffer;
        drawDesc.pipeline = &rasterPipeline;
        drawDesc.bufferBindings = {{
            .group = 0,
            .binding = 0,
            .buffer = &uniformBuffer,
            .offset = 0,
            .size = 16,
        }};
        drawDesc.samplerBindings = {{
            .group = 0,
            .binding = 1,
            .sampler = &sampler,
        }};
        auto draw = graph.add_raster_draw_pass(drawDesc);
        REQUIRE(draw.valid());
        CHECK(graph.compile().is_ok());
    }

    TEST_CASE("duplicate slot across binding kinds is rejected") {
        firefly::RenderGraph graph;
        firefly::Buffer uniformBuffer;
        firefly::Sampler sampler;

        auto backbuffer = graph.import_backbuffer();
        firefly::TextureDesc texDesc;
        texDesc.width = 4;
        texDesc.height = 4;
        texDesc.format = firefly::TextureFormat::RGBA8Unorm;
        texDesc.usage = firefly::TextureUsage::TextureBinding;
        auto history = graph.create_texture("History", texDesc);

        firefly::RasterPassDesc passDesc;
        passDesc.name = "Pass";
        passDesc.colorTarget = backbuffer;
        auto pass = graph.add_raster_pass(
            passDesc,
            [](firefly::RenderGraphPassContext&) {}
        );
        REQUIRE(pass.valid());

        REQUIRE(graph.add_texture_binding(pass, {
            .group = 0,
            .binding = 0,
            .resource = history,
            .access = firefly::RenderGraphBindingAccess::Read,
        }).is_ok());

        CHECK(graph.add_buffer_binding(pass, {
            .group = 0,
            .binding = 0,
            .buffer = &uniformBuffer,
            .offset = 0,
            .size = 16,
        }).is_error());

        CHECK(graph.add_sampler_binding(pass, {
            .group = 0,
            .binding = 0,
            .sampler = &sampler,
        }).is_error());
    }

    TEST_CASE("helper pass creation validates required pipeline and dispatch/draw counts") {
        firefly::RenderGraph graph;
        auto backbuffer = graph.import_backbuffer();

        firefly::RasterDrawPassDesc drawDesc;
        drawDesc.pass.colorTarget = backbuffer;
        drawDesc.vertexCount = 0;
        auto draw = graph.add_raster_draw_pass(drawDesc);
        CHECK_FALSE(draw.valid());

        firefly::TextureDesc tempDesc;
        tempDesc.width = 4;
        tempDesc.height = 4;
        tempDesc.format = firefly::TextureFormat::RGBA8Unorm;
        tempDesc.usage = firefly::TextureUsage::RenderAttachment;
        auto history = graph.create_texture("History", tempDesc);

        firefly::ComputeDispatchPassDesc computeDesc;
        computeDesc.pass.writes = {history};
        computeDesc.groupCountX = 0;
        auto compute = graph.add_compute_dispatch_pass(computeDesc);
        CHECK_FALSE(compute.valid());
    }

    TEST_CASE("compute write requires storage usage") {
        firefly::RenderGraph graph;
        auto backbuffer = graph.import_backbuffer();

        firefly::TextureDesc historyDesc;
        historyDesc.width = 8;
        historyDesc.height = 8;
        historyDesc.format = firefly::TextureFormat::RGBA8Unorm;
        historyDesc.usage = firefly::TextureUsage::RenderAttachment;
        auto history = graph.create_texture("History", historyDesc);

        firefly::ComputePassDesc computeDesc;
        computeDesc.name = "ComputeHistory";
        computeDesc.writes = {history};
        auto compute = graph.add_compute_pass(
            computeDesc,
            [](firefly::RenderGraphPassContext&) {}
        );
        REQUIRE(compute.valid());

        firefly::RasterPassDesc presentDesc;
        presentDesc.name = "Present";
        presentDesc.colorTarget = backbuffer;
        auto present = graph.add_raster_pass(
            presentDesc,
            [](firefly::RenderGraphPassContext&) {}
        );
        REQUIRE(present.valid());

        auto compileResult = graph.compile();
        CHECK(compileResult.is_error());
    }

    TEST_CASE("raster write requires render attachment usage") {
        firefly::RenderGraph graph;

        firefly::TextureDesc tempDesc;
        tempDesc.width = 8;
        tempDesc.height = 8;
        tempDesc.format = firefly::TextureFormat::RGBA8Unorm;
        tempDesc.usage = firefly::TextureUsage::StorageBinding;
        auto temp = graph.create_texture("Temp", tempDesc);

        firefly::RasterPassDesc passDesc;
        passDesc.name = "InvalidRasterWrite";
        passDesc.colorTarget = temp;
        auto pass = graph.add_raster_pass(
            passDesc,
            [](firefly::RenderGraphPassContext&) {}
        );
        REQUIRE(pass.valid());

        auto compileResult = graph.compile();
        CHECK(compileResult.is_error());
    }

    TEST_CASE("lifetime analysis and peak transient memory estimation") {
        firefly::RenderGraph graph;
        auto backbuffer = graph.import_backbuffer();

        firefly::TextureDesc tempDesc;
        tempDesc.width = 16;
        tempDesc.height = 8;
        tempDesc.format = firefly::TextureFormat::RGBA8Unorm;
        tempDesc.usage = firefly::TextureUsage::RenderAttachment |
            firefly::TextureUsage::StorageBinding;

        auto historyA = graph.create_texture("HistoryA", tempDesc);
        auto historyB = graph.create_texture("HistoryB", tempDesc);

        firefly::ComputePassDesc writeADesc;
        writeADesc.name = "WriteA";
        writeADesc.writes = {historyA};
        auto writeA = graph.add_compute_pass(
            writeADesc,
            [](firefly::RenderGraphPassContext&) {}
        );
        REQUIRE(writeA.valid());

        firefly::RasterPassDesc mixDesc;
        mixDesc.name = "MixAB";
        mixDesc.colorTarget = historyB;
        auto mixAB = graph.add_raster_pass(
            mixDesc,
            [](firefly::RenderGraphPassContext&) {}
        );
        REQUIRE(mixAB.valid());
        REQUIRE(graph.add_read(mixAB, historyA).is_ok());

        firefly::RasterPassDesc presentDesc;
        presentDesc.name = "Present";
        presentDesc.colorTarget = backbuffer;
        auto present = graph.add_raster_pass(
            presentDesc,
            [](firefly::RenderGraphPassContext&) {}
        );
        REQUIRE(present.valid());
        REQUIRE(graph.add_read(present, historyB).is_ok());

        auto compileResult = graph.compile();
        REQUIRE(compileResult.is_ok());

        auto lifetimesResult = graph.resource_lifetimes();
        REQUIRE(lifetimesResult.is_ok());
        const auto& lifetimes = lifetimesResult.value();
        REQUIRE(lifetimes.size() == 3);

        const auto& lifetimeA = lifetimes[historyA.id];
        CHECK(lifetimeA.used);
        CHECK(lifetimeA.firstPass == writeA.id);
        CHECK(lifetimeA.lastPass == mixAB.id);

        const auto& lifetimeB = lifetimes[historyB.id];
        CHECK(lifetimeB.used);
        CHECK(lifetimeB.firstPass == mixAB.id);
        CHECK(lifetimeB.lastPass == present.id);

        auto memoryStatsResult = graph.memory_stats();
        REQUIRE(memoryStatsResult.is_ok());
        const auto& stats = memoryStatsResult.value();
        CHECK(stats.peakAliveTransientResources == 2);
        CHECK(stats.peakEstimatedTransientBytes >= 2u * 16u * 8u * 4u);
    }

    TEST_CASE("alias allocation reduces transient texture count") {
        firefly::RenderGraph graph;
        auto backbuffer = graph.import_backbuffer();

        firefly::TextureDesc tempDesc;
        tempDesc.width = 16;
        tempDesc.height = 8;
        tempDesc.format = firefly::TextureFormat::RGBA8Unorm;
        tempDesc.usage = firefly::TextureUsage::RenderAttachment |
            firefly::TextureUsage::StorageBinding;

        auto historyA = graph.create_texture("HistoryA", tempDesc);
        auto historyB = graph.create_texture("HistoryB", tempDesc);

        firefly::ComputePassDesc writeADesc;
        writeADesc.name = "WriteA";
        writeADesc.writes = {historyA};
        auto writeA = graph.add_compute_pass(
            writeADesc,
            [](firefly::RenderGraphPassContext&) {}
        );
        REQUIRE(writeA.valid());

        firefly::RasterPassDesc presentADesc;
        presentADesc.name = "PresentA";
        presentADesc.colorTarget = backbuffer;
        auto presentA = graph.add_raster_pass(
            presentADesc,
            [](firefly::RenderGraphPassContext&) {}
        );
        REQUIRE(presentA.valid());
        REQUIRE(graph.add_read(presentA, historyA).is_ok());

        firefly::ComputePassDesc writeBDesc;
        writeBDesc.name = "WriteB";
        writeBDesc.writes = {historyB};
        auto writeB = graph.add_compute_pass(
            writeBDesc,
            [](firefly::RenderGraphPassContext&) {}
        );
        REQUIRE(writeB.valid());

        firefly::RasterPassDesc presentBDesc;
        presentBDesc.name = "PresentB";
        presentBDesc.colorTarget = backbuffer;
        auto presentB = graph.add_raster_pass(
            presentBDesc,
            [](firefly::RenderGraphPassContext&) {}
        );
        REQUIRE(presentB.valid());
        REQUIRE(graph.add_read(presentB, historyB).is_ok());

        auto compileResult = graph.compile();
        REQUIRE(compileResult.is_ok());

        auto allocationResult = graph.allocation_stats();
        REQUIRE(allocationResult.is_ok());
        const auto& alloc = allocationResult.value();
        CHECK(alloc.usedTransientResources == 2);
        CHECK(alloc.allocatedTransientTextures == 1);
        CHECK(alloc.aliasedTransientResources == 1);
        CHECK(alloc.allocatedEstimatedTransientBytes >= 16u * 8u * 4u);
    }

    TEST_CASE("debug dump contains key graph sections") {
        firefly::RenderGraph graph;
        auto backbuffer = graph.import_backbuffer();

        firefly::TextureDesc tempDesc;
        tempDesc.width = 8;
        tempDesc.height = 8;
        tempDesc.format = firefly::TextureFormat::RGBA8Unorm;
        tempDesc.usage = firefly::TextureUsage::RenderAttachment |
            firefly::TextureUsage::StorageBinding;
        auto history = graph.create_texture("History", tempDesc);

        firefly::ComputePassDesc computeDesc;
        computeDesc.name = "ComputeHistory";
        computeDesc.writes = {history};
        auto compute = graph.add_compute_pass(
            computeDesc,
            [](firefly::RenderGraphPassContext&) {}
        );
        REQUIRE(compute.valid());

        firefly::RasterPassDesc presentDesc;
        presentDesc.name = "Present";
        presentDesc.colorTarget = backbuffer;
        auto present = graph.add_raster_pass(
            presentDesc,
            [](firefly::RenderGraphPassContext&) {}
        );
        REQUIRE(present.valid());
        REQUIRE(graph.add_read(present, history).is_ok());
        REQUIRE(graph.compile().is_ok());

        auto reportResult = graph.debug_dump_text();
        REQUIRE(reportResult.is_ok());
        const auto& report = reportResult.value();
        CHECK(report.find("RenderGraph Debug Report") != std::string::npos);
        CHECK(report.find("PassOrder:") != std::string::npos);
        CHECK(report.find("ResourceLifetimes:") != std::string::npos);
        CHECK(report.find("AliasSlots:") != std::string::npos);
        CHECK(report.find("bindingCache.bindGroupCreates=") != std::string::npos);
        CHECK(report.find("bindingCache.staticCreates=") != std::string::npos);
        CHECK(report.find("bindingCache.dynamicCreates=") != std::string::npos);
    }

    TEST_CASE("binding cache stats default to zero before execute") {
        firefly::RenderGraph graph;
        const auto stats = graph.binding_cache_stats();
        CHECK(stats.layoutCreates == 0);
        CHECK(stats.bindGroupCreates == 0);
        CHECK(stats.bindGroupReuses == 0);
        CHECK(stats.staticBindGroupCreates == 0);
        CHECK(stats.staticBindGroupReuses == 0);
        CHECK(stats.dynamicBindGroupCreates == 0);
        CHECK(stats.dynamicBindGroupReuses == 0);
    }

    TEST_CASE("raster pass can declare dedicated depth target") {
        firefly::RenderGraph graph;
        firefly::RenderPipeline pipeline;
        auto backbuffer = graph.import_backbuffer();

        firefly::TextureDesc depthDesc;
        depthDesc.label = "Depth";
        depthDesc.width = 640;
        depthDesc.height = 360;
        depthDesc.format = firefly::TextureFormat::Depth32Float;
        depthDesc.usage = firefly::TextureUsage::RenderAttachment;
        auto depth = graph.create_texture("Depth", depthDesc);

        firefly::RasterPassDesc passDesc;
        passDesc.name = "MainWithDepth";
        passDesc.colorTarget = backbuffer;
        passDesc.depthTarget = depth;
        auto pass = graph.add_raster_pass(
            passDesc,
            [](firefly::RenderGraphPassContext&) {}
        );
        REQUIRE(pass.valid());
        CHECK(graph.set_raster_pipeline(pass, &pipeline).is_ok());
        CHECK(graph.compile().is_ok());
    }

    TEST_CASE("depth-only raster pass can omit color target") {
        firefly::RenderGraph graph;
        firefly::RenderPipeline pipeline;
        auto backbuffer = graph.import_backbuffer();

        firefly::TextureDesc shadowDepthDesc;
        shadowDepthDesc.label = "ShadowDepth";
        shadowDepthDesc.width = 1024;
        shadowDepthDesc.height = 1024;
        shadowDepthDesc.format = firefly::TextureFormat::Depth32Float;
        shadowDepthDesc.usage = firefly::TextureUsage::RenderAttachment |
            firefly::TextureUsage::TextureBinding;
        auto shadowDepth = graph.create_texture("ShadowDepth", shadowDepthDesc);

        firefly::RasterPassDesc shadowPassDesc;
        shadowPassDesc.name = "ShadowDepthOnly";
        shadowPassDesc.depthTarget = shadowDepth;
        shadowPassDesc.clearColor = false;
        auto shadowPass = graph.add_raster_pass(
            shadowPassDesc,
            [](firefly::RenderGraphPassContext&) {}
        );
        REQUIRE(shadowPass.valid());
        CHECK(graph.set_raster_pipeline(shadowPass, &pipeline).is_ok());

        firefly::RasterPassDesc presentDesc;
        presentDesc.name = "Present";
        presentDesc.colorTarget = backbuffer;
        auto present = graph.add_raster_pass(
            presentDesc,
            [](firefly::RenderGraphPassContext&) {}
        );
        REQUIRE(present.valid());
        REQUIRE(graph.add_read(present, shadowDepth).is_ok());

        CHECK(graph.compile().is_ok());
        REQUIRE(graph.compiled_order().size() == 2);
        CHECK(graph.compiled_order()[0] == shadowPass.id);
        CHECK(graph.compiled_order()[1] == present.id);
    }
}

TEST_SUITE("Forward Renderer API") {
    TEST_CASE("recommended depth descriptor uses depth attachment defaults") {
        firefly::ForwardRenderer forward;
        const auto desc = forward.recommended_depth_desc();
        CHECK(desc.format == firefly::TextureFormat::Depth32Float);
        CHECK(firefly::has_usage(desc.usage, firefly::TextureUsage::RenderAttachment));
        CHECK(desc.width == 0);
        CHECK(desc.height == 0);
    }

    TEST_CASE("recommended hdr descriptor uses render-attachment texture format") {
        firefly::ForwardRenderer forward;
        const auto desc = forward.recommended_hdr_desc();
        CHECK(desc.format == firefly::TextureFormat::RGBA16Float);
        CHECK(firefly::has_usage(desc.usage, firefly::TextureUsage::RenderAttachment));
        CHECK(firefly::has_usage(desc.usage, firefly::TextureUsage::TextureBinding));
    }

    TEST_CASE("recommended shadow descriptor exposes depth target") {
        firefly::ForwardRenderer forward;
        const auto depthDesc = forward.recommended_shadow_depth_desc();
        CHECK(depthDesc.format == firefly::TextureFormat::Depth32Float);
        CHECK(firefly::has_usage(depthDesc.usage, firefly::TextureUsage::RenderAttachment));
        CHECK(firefly::has_usage(depthDesc.usage, firefly::TextureUsage::TextureBinding));
    }

    TEST_CASE("light API accepts directional and point light arrays") {
        firefly::ForwardRenderer forward;
        forward.set_directional_lights({
            {
                .direction = glm::vec3(0.0f, -1.0f, 0.0f),
                .intensity = 3.0f,
                .color = glm::vec3(1.0f),
                .ambientIntensity = 0.04f,
            },
        });
        forward.set_point_lights({
            {
                .position = glm::vec3(1.0f, 2.0f, 3.0f),
                .range = 5.0f,
                .color = glm::vec3(1.0f, 0.5f, 0.4f),
                .intensity = 10.0f,
            },
            {
                .position = glm::vec3(-1.0f, 1.0f, -2.0f),
                .range = 4.0f,
                .color = glm::vec3(0.4f, 0.6f, 1.0f),
                .intensity = 8.0f,
            },
        });
        forward.set_light({});
        CHECK(forward.recommended_depth_desc().format == firefly::TextureFormat::Depth32Float);
    }

    TEST_CASE("grid params API accepts instancing configuration") {
        firefly::ForwardRenderer forward;
        forward.set_grid_params({
            .enabled = true,
            .columns = 6,
            .rows = 5,
            .spacing = 2.0f,
        });
        forward.set_grid_params({
            .enabled = false,
            .columns = 1,
            .rows = 1,
            .spacing = 2.3f,
        });
        CHECK(forward.recommended_depth_desc().format == firefly::TextureFormat::Depth32Float);
    }

    TEST_CASE("shadow settings API accepts bias and PCF toggles") {
        firefly::ForwardRenderer forward;
        forward.set_shadow_settings({
            .constantBias = 0.0012f,
            .slopeBias = 0.004f,
            .enablePCF = true,
            .enableCascades = true,
            .cascadeSplitDistance = 7.5f,
            .cascadeBlendDistance = 1.0f,
            .shadowMapResolution = 1024,
        });
        forward.set_shadow_settings({
            .constantBias = 0.0f,
            .slopeBias = 0.002f,
            .enablePCF = false,
            .enableCascades = false,
            .cascadeSplitDistance = 5.0f,
            .cascadeBlendDistance = 0.75f,
            .shadowMapResolution = 512,
        });
        CHECK(forward.recommended_shadow_depth_desc().format == firefly::TextureFormat::Depth32Float);
    }

    TEST_CASE("set_mesh_data returns error when renderer is not initialized") {
        firefly::ForwardRenderer forward;
        std::vector<firefly::Vertex> vertices = {
            {glm::vec3(-1.0f, -1.0f, 0.0f)},
            {glm::vec3(1.0f, -1.0f, 0.0f)},
            {glm::vec3(0.0f, 1.0f, 0.0f)},
        };
        std::vector<firefly::u32> indices = {0, 1, 2};
        CHECK(forward.set_mesh_data(vertices, indices).is_error());
    }
}

TEST_SUITE("Forward Renderer") {
    TEST_CASE("recommended descriptors align with forward shading defaults") {
        firefly::ForwardRenderer forward;
        const auto depth = forward.recommended_depth_desc();
        CHECK(depth.format == firefly::TextureFormat::Depth32Float);
        CHECK(firefly::has_usage(depth.usage, firefly::TextureUsage::RenderAttachment));

        const auto hdr = forward.recommended_hdr_desc();
        CHECK(hdr.format == firefly::TextureFormat::RGBA16Float);
        CHECK(firefly::has_usage(hdr.usage, firefly::TextureUsage::RenderAttachment));
        CHECK(firefly::has_usage(hdr.usage, firefly::TextureUsage::TextureBinding));
    }

    TEST_CASE("add_frame_passes returns error when renderer is not initialized") {
        firefly::ForwardRenderer forward;
        firefly::RenderGraph graph;
        auto backbuffer = graph.import_backbuffer();

        firefly::TextureDesc hdrDesc;
        hdrDesc.width = 640;
        hdrDesc.height = 360;
        hdrDesc.format = firefly::TextureFormat::RGBA16Float;
        hdrDesc.usage = firefly::TextureUsage::RenderAttachment |
            firefly::TextureUsage::TextureBinding;
        auto hdr = graph.create_texture("Hdr", hdrDesc);

        firefly::TextureDesc depthDesc;
        depthDesc.width = 640;
        depthDesc.height = 360;
        depthDesc.format = firefly::TextureFormat::Depth32Float;
        depthDesc.usage = firefly::TextureUsage::RenderAttachment;
        auto depth = graph.create_texture("Depth", depthDesc);

        firefly::TextureDesc shadowDesc;
        shadowDesc.width = 1024;
        shadowDesc.height = 1024;
        shadowDesc.format = firefly::TextureFormat::Depth32Float;
        shadowDesc.usage = firefly::TextureUsage::RenderAttachment |
            firefly::TextureUsage::TextureBinding;
        auto shadowNear = graph.create_texture("ShadowNear", shadowDesc);
        auto shadowFar = graph.create_texture("ShadowFar", shadowDesc);

        auto result = forward.add_frame_passes(graph, {
            .outputColorTarget = backbuffer,
            .hdrColorTarget = hdr,
            .depthTarget = depth,
            .shadowDepthNearTarget = shadowNear,
            .shadowDepthFarTarget = shadowFar,
        });
        CHECK(result.is_error());
    }

    TEST_CASE("render_frame returns error when renderer is not initialized") {
        firefly::ForwardRenderer forward;
        auto result = forward.render_frame(1280, 720);
        CHECK(result.is_error());
    }
}
