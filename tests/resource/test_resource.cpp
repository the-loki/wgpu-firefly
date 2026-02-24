#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

import firefly.resource.manager;
import firefly.resource.texture_importer;
import firefly.resource.model_importer;
import firefly.core.types;

#include <filesystem>
#include <algorithm>
#include <string>
#include <memory>
#include <typeindex>
#include <vector>

// ============================================================
// Phase 6: Resource 模块测试
// ============================================================

// Test resource for unit testing
struct TestResource : public firefly::Resource {
    int value = 0;
    auto type_name() const -> firefly::String override { return "TestResource"; }
};

class TestLoader : public firefly::LoaderBase {
public:
    auto load(const firefly::String& path) -> firefly::SharedPtr<firefly::Resource> override {
        (void)path;
        auto res = std::make_shared<TestResource>();
        res->value = 42;
        return res;
    }
};

class CountingLoader : public firefly::LoaderBase {
public:
    explicit CountingLoader(int* count) : m_count(count) {}

    auto load(const firefly::String& path) -> firefly::SharedPtr<firefly::Resource> override {
        (void)path;
        if (m_count) {
            ++(*m_count);
        }
        return std::make_shared<TestResource>();
    }

private:
    int* m_count = nullptr;
};

static auto find_model_asset_path() -> std::string {
    std::vector<std::filesystem::path> candidates;
    candidates.emplace_back("assets/models/cube.obj");
#ifdef FIREFLY_SOURCE_DIR
    candidates.emplace_back(std::filesystem::path(FIREFLY_SOURCE_DIR) / "assets/models/cube.obj");
#endif
    candidates.emplace_back(std::filesystem::path("..") / "assets/models/cube.obj");
    candidates.emplace_back(std::filesystem::path("..") / ".." / "assets/models/cube.obj");
    candidates.emplace_back(std::filesystem::path("..") / ".." / ".." / "assets/models/cube.obj");

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate.string();
        }
    }
    return {};
}

static auto find_sponza_asset_path() -> std::string {
    std::vector<std::filesystem::path> candidates;
    candidates.emplace_back("assets/models/sponza-png.glb");
#ifdef FIREFLY_SOURCE_DIR
    candidates.emplace_back(std::filesystem::path(FIREFLY_SOURCE_DIR) / "assets/models/sponza-png.glb");
#endif
    candidates.emplace_back(std::filesystem::path("..") / "assets/models/sponza-png.glb");
    candidates.emplace_back(std::filesystem::path("..") / ".." / "assets/models/sponza-png.glb");
    candidates.emplace_back(std::filesystem::path("..") / ".." / ".." / "assets/models/sponza-png.glb");

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate.string();
        }
    }
    return {};
}

TEST_SUITE("Handle") {
    TEST_CASE("default handle is invalid") {
        firefly::Handle<TestResource> h;
        CHECK_FALSE(h.is_valid());
        CHECK(h.id() == 0);
    }

    TEST_CASE("handle with id is valid") {
        firefly::Handle<TestResource> h(1);
        CHECK(h.is_valid());
        CHECK(h.id() == 1);
    }

    TEST_CASE("handle equality") {
        firefly::Handle<TestResource> a(1);
        firefly::Handle<TestResource> b(1);
        firefly::Handle<TestResource> c(2);
        CHECK(a == b);
        CHECK(a != c);
    }
}

TEST_SUITE("Resource") {
    TEST_CASE("ref counting") {
        TestResource res;
        CHECK(res.ref_count() == 0);
        res.add_ref();
        CHECK(res.ref_count() == 1);
        res.add_ref();
        CHECK(res.ref_count() == 2);
        res.release();
        CHECK(res.ref_count() == 1);
    }

    TEST_CASE("type name") {
        TestResource res;
        CHECK(res.type_name() == "TestResource");
    }
}

TEST_SUITE("ResourceManager") {
    TEST_CASE("register loader and load") {
        firefly::ResourceManager mgr;
        mgr.register_loader_for(
            std::type_index(typeid(TestResource)),
            std::make_unique<TestLoader>());

        auto handle = mgr.load<TestResource>("test/path.dat");
        CHECK(handle.is_valid());

        auto* res = mgr.get(handle);
        CHECK(res != nullptr);
        CHECK(res->value == 42);
    }

    TEST_CASE("cache deduplication") {
        firefly::ResourceManager mgr;
        mgr.register_loader_for(
            std::type_index(typeid(TestResource)),
            std::make_unique<TestLoader>());

        auto h1 = mgr.load<TestResource>("same/path.dat");
        auto h2 = mgr.load<TestResource>("same/path.dat");
        CHECK(h1 == h2);
        CHECK(mgr.cache_size() == 1);
    }

    TEST_CASE("different paths create different resources") {
        firefly::ResourceManager mgr;
        mgr.register_loader_for(
            std::type_index(typeid(TestResource)),
            std::make_unique<TestLoader>());

        auto h1 = mgr.load<TestResource>("path/a.dat");
        auto h2 = mgr.load<TestResource>("path/b.dat");
        CHECK(h1 != h2);
        CHECK(mgr.cache_size() == 2);
    }

    TEST_CASE("unload resource") {
        firefly::ResourceManager mgr;
        mgr.register_loader_for(
            std::type_index(typeid(TestResource)),
            std::make_unique<TestLoader>());

        auto handle = mgr.load<TestResource>("test.dat");
        CHECK(mgr.cache_size() == 1);
        mgr.unload(handle.id());
        CHECK(mgr.cache_size() == 0);
    }

    TEST_CASE("load without loader returns invalid") {
        firefly::ResourceManager mgr;
        auto handle = mgr.load<TestResource>("no_loader.dat");
        CHECK_FALSE(handle.is_valid());
    }

    TEST_CASE("reload after unload with same path") {
        firefly::ResourceManager mgr;
        int loadCount = 0;
        mgr.register_loader_for(
            std::type_index(typeid(TestResource)),
            std::make_unique<CountingLoader>(&loadCount));

        auto first = mgr.load<TestResource>("assets/reload.dat");
        auto secondRef = mgr.load<TestResource>("assets/reload.dat");
        CHECK(first.is_valid());
        CHECK(first == secondRef);
        CHECK(loadCount == 1);

        mgr.unload(first.id());
        CHECK(mgr.cache_size() == 1);
        mgr.unload(secondRef.id());
        CHECK(mgr.cache_size() == 0);

        auto reloaded = mgr.load<TestResource>("assets/reload.dat");
        CHECK(reloaded.is_valid());
        CHECK(reloaded != first);
        CHECK(loadCount == 2);
    }
}

TEST_SUITE("TextureImporter") {
    TEST_CASE("ImageData type name") {
        firefly::ImageData img;
        CHECK(img.type_name() == "ImageData");
        CHECK(img.width == 0);
        CHECK(img.height == 0);
    }

    TEST_CASE("load nonexistent file returns null") {
        firefly::TextureImporter importer;
        auto result = importer.load("nonexistent_image.png");
        CHECK(result == nullptr);
    }
}

TEST_SUITE("ModelImporter") {
    TEST_CASE("ModelData type name") {
        firefly::ModelData model;
        CHECK(model.type_name() == "ModelData");
        CHECK(model.total_vertex_count() == 0);
        CHECK(model.total_index_count() == 0);
    }

    TEST_CASE("load nonexistent model returns null") {
        firefly::ModelImporter importer;
        auto result = importer.load("nonexistent_model.obj");
        CHECK(result == nullptr);
    }

    TEST_CASE("load bundled cube model") {
        firefly::ModelImporter importer;
        const auto modelPath = find_model_asset_path();
        REQUIRE_FALSE(modelPath.empty());

        auto resource = importer.load(modelPath);
        REQUIRE(resource != nullptr);

        auto* model = dynamic_cast<firefly::ModelData*>(resource.get());
        REQUIRE(model != nullptr);
        CHECK(model->meshes.size() >= 1);
        CHECK(model->total_vertex_count() > 0);
        CHECK(model->total_index_count() > 0);

        const auto flattened = firefly::flatten_model(*model);
        CHECK(flattened.vertices.size() == model->total_vertex_count());
        CHECK(flattened.indices.size() == model->total_index_count());
        CHECK(flattened.indices.size() % 3 == 0);
        REQUIRE_FALSE(flattened.vertices.empty());
        CHECK(flattened.vertices.front().color.a > 0.0f);
    }

    TEST_CASE("load sponza pbr glb when present") {
        const auto sponzaPath = find_sponza_asset_path();
        if (sponzaPath.empty()) {
            INFO("Sponza asset not found, skipped: assets/models/sponza-png.glb");
            return;
        }

        firefly::ModelImporter importer;
        auto resource = importer.load(sponzaPath);
        REQUIRE(resource != nullptr);

        auto* model = dynamic_cast<firefly::ModelData*>(resource.get());
        REQUIRE(model != nullptr);
        CHECK(model->meshes.size() > 1);
        CHECK(model->total_vertex_count() > 1000);
        CHECK(model->total_index_count() > 3000);

        const auto flattened = firefly::flatten_model(*model);
        CHECK(flattened.vertices.size() == model->total_vertex_count());
        CHECK(flattened.indices.size() == model->total_index_count());
        REQUIRE_FALSE(flattened.vertices.empty());

        firefly::f32 minLuminance = 1.0f;
        firefly::f32 maxLuminance = 0.0f;
        firefly::f32 minRoughness = 1.0f;
        firefly::f32 maxRoughness = 0.0f;
        for (size_t i = 0; i < flattened.vertices.size(); i += 257) {
            const auto& c = flattened.vertices[i].color;
            const firefly::f32 luminance = c.r * 0.2126f + c.g * 0.7152f + c.b * 0.0722f;
            minLuminance = std::min(minLuminance, luminance);
            maxLuminance = std::max(maxLuminance, luminance);
            minRoughness = std::min(minRoughness, flattened.vertices[i].roughness);
            maxRoughness = std::max(maxRoughness, flattened.vertices[i].roughness);
        }
        CHECK(maxLuminance - minLuminance > 0.05f);
        CHECK(minRoughness >= 0.045f);
        CHECK(maxRoughness <= 1.0f);
        CHECK(maxRoughness >= minRoughness);
    }
}
