#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

import firefly.resource.manager;
import firefly.resource.texture_importer;
import firefly.core.types;

#include <string>
#include <memory>
#include <typeindex>

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
        auto res = std::make_shared<TestResource>();
        res->value = 42;
        return res;
    }
};

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
