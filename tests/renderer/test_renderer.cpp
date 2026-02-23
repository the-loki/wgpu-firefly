#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

import firefly.renderer.buffer;
import firefly.renderer.texture;
import firefly.renderer.shader;
import firefly.renderer.pipeline;
import firefly.renderer.material;
import firefly.renderer.mesh;
import firefly.renderer.camera;
import firefly.renderer.device;
import firefly.core.types;

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

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
    }
}
