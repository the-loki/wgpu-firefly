module;
#include <cstdint>
#include <string>
#include <memory>
#include <unordered_map>
#include <typeindex>
#include <mutex>
#include <functional>

export module firefly.resource.manager;

import firefly.core.types;

export namespace firefly {

class Resource {
public:
    virtual ~Resource() = default;
    virtual auto type_name() const -> String = 0;
    auto ref_count() const -> u32 { return m_refCount; }
    void add_ref() { ++m_refCount; }
    void release() { if (m_refCount > 0) --m_refCount; }
private:
    u32 m_refCount = 0;
};

template<typename T>
class Handle {
public:
    Handle() = default;
    Handle(u64 id) : m_id(id) {}

    auto is_valid() const -> bool { return m_id != 0; }
    auto id() const -> u64 { return m_id; }
    auto operator==(const Handle& other) const -> bool { return m_id == other.m_id; }
    auto operator!=(const Handle& other) const -> bool { return m_id != other.m_id; }

private:
    u64 m_id = 0;
};

class LoaderBase {
public:
    virtual ~LoaderBase() = default;
    virtual auto load(const String& path) -> SharedPtr<Resource> = 0;
};

class ResourceManager {
public:
    ResourceManager() = default;
    ~ResourceManager() = default;

    template<typename T>
    auto load(const String& path) -> Handle<T> {
        std::lock_guard lock(m_mutex);
        // Check cache
        auto it = m_pathCache.find(path);
        if (it != m_pathCache.end()) {
            auto& res = m_cache[it->second];
            res->add_ref();
            return Handle<T>(it->second);
        }
        // Find loader
        auto loaderIt = m_loaders.find(std::type_index(typeid(T)));
        if (loaderIt == m_loaders.end()) return Handle<T>();
        auto resource = loaderIt->second->load(path);
        if (!resource) return Handle<T>();
        u64 id = m_nextId++;
        resource->add_ref();
        m_cache[id] = resource;
        m_pathCache[path] = id;
        return Handle<T>(id);
    }

    template<typename T>
    auto get(Handle<T> handle) -> T* {
        std::lock_guard lock(m_mutex);
        auto it = m_cache.find(handle.id());
        if (it == m_cache.end()) return nullptr;
        return dynamic_cast<T*>(it->second.get());
    }

    void unload(u64 handleId) {
        std::lock_guard lock(m_mutex);
        auto it = m_cache.find(handleId);
        if (it != m_cache.end()) {
            it->second->release();
            if (it->second->ref_count() == 0) {
                m_cache.erase(it);
            }
        }
    }

    void unload_unused() {
        std::lock_guard lock(m_mutex);
        for (auto it = m_cache.begin(); it != m_cache.end();) {
            if (it->second->ref_count() == 0) {
                it = m_cache.erase(it);
            } else {
                ++it;
            }
        }
    }

    template<typename T, typename Loader>
    void register_loader() {
        std::lock_guard lock(m_mutex);
        m_loaders[std::type_index(typeid(T))] = std::make_unique<Loader>();
    }

    void register_loader_for(std::type_index type, Ptr<LoaderBase> loader) {
        std::lock_guard lock(m_mutex);
        m_loaders[type] = std::move(loader);
    }

    auto cache_size() const -> size_t { return m_cache.size(); }

private:
    std::unordered_map<u64, SharedPtr<Resource>> m_cache;
    std::unordered_map<String, u64> m_pathCache;
    std::unordered_map<std::type_index, Ptr<LoaderBase>> m_loaders;
    std::mutex m_mutex;
    u64 m_nextId = 1;
};

} // namespace firefly
