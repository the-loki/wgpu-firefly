module;
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

export module firefly.renderer.render_graph;

import firefly.core.types;
import firefly.renderer.buffer;
import firefly.renderer.command;
import firefly.renderer.device;
import firefly.renderer.pipeline;
import firefly.renderer.sampler;
import firefly.renderer.texture;

export namespace firefly {

struct RenderGraphResourceHandle {
    static constexpr u32 invalid_id = 0xFFFFFFFFu;
    u32 id = invalid_id;

    [[nodiscard]] auto valid() const -> bool { return id != invalid_id; }
};

struct RenderGraphPassHandle {
    static constexpr u32 invalid_id = 0xFFFFFFFFu;
    u32 id = invalid_id;

    [[nodiscard]] auto valid() const -> bool { return id != invalid_id; }
};

struct RenderGraphPassContext {
    CommandBuffer& commandBuffer;
    RenderDevice& device;
    const void* const* resourceViews = nullptr;
    u32 resourceViewCount = 0;

    [[nodiscard]] auto command() -> CommandBuffer& { return commandBuffer; }
    [[nodiscard]] auto render_device() -> RenderDevice& { return device; }
    [[nodiscard]] auto texture_view(RenderGraphResourceHandle handle) const -> void*;
    [[nodiscard]] auto has_resource(RenderGraphResourceHandle handle) const -> bool;
};

using RenderGraphExecuteFn = std::function<void(RenderGraphPassContext&)>;

enum class RenderGraphBindingAccess : u8 {
    Read,
    Write,
};

struct RenderGraphTextureBindingDesc {
    u32 group = 0;
    u32 binding = 0;
    RenderGraphResourceHandle resource{};
    RenderGraphBindingAccess access = RenderGraphBindingAccess::Read;
};

struct RenderGraphBufferBindingDesc {
    u32 group = 0;
    u32 binding = 0;
    Buffer* buffer = nullptr;
    u64 offset = 0;
    u64 size = 0; // 0 means "buffer.size() - offset"
};

struct RenderGraphSamplerBindingDesc {
    u32 group = 0;
    u32 binding = 0;
    Sampler* sampler = nullptr;
};

struct RasterPassDesc {
    String name = "RasterPass";
    // Color target is optional for depth-only raster passes.
    RenderGraphResourceHandle colorTarget{};
    RenderGraphResourceHandle depthTarget{};
    bool clearColor = true;
    f32 clearR = 0.1f;
    f32 clearG = 0.1f;
    f32 clearB = 0.1f;
    f32 clearA = 1.0f;
    bool clearDepth = true;
    f32 clearDepthValue = 1.0f;
};

struct ComputePassDesc {
    String name = "ComputePass";
    std::vector<RenderGraphResourceHandle> writes;
};

struct RasterDrawPassDesc {
    RasterPassDesc pass{};
    RenderPipeline* pipeline = nullptr;
    std::vector<RenderGraphResourceHandle> reads;
    std::vector<RenderGraphTextureBindingDesc> bindings;
    std::vector<RenderGraphBufferBindingDesc> bufferBindings;
    std::vector<RenderGraphSamplerBindingDesc> samplerBindings;
    u32 vertexCount = 3;
    u32 instanceCount = 1;
    u32 firstVertex = 0;
    u32 firstInstance = 0;
};

struct ComputeDispatchPassDesc {
    ComputePassDesc pass{};
    ComputePipeline* pipeline = nullptr;
    std::vector<RenderGraphResourceHandle> reads;
    std::vector<RenderGraphTextureBindingDesc> bindings;
    std::vector<RenderGraphBufferBindingDesc> bufferBindings;
    std::vector<RenderGraphSamplerBindingDesc> samplerBindings;
    u32 groupCountX = 1;
    u32 groupCountY = 1;
    u32 groupCountZ = 1;
};

struct RenderGraphResourceLifetime {
    RenderGraphResourceHandle resource{};
    bool imported = false;
    bool used = false;
    u32 firstPass = RenderGraphPassHandle::invalid_id;
    u32 lastPass = RenderGraphPassHandle::invalid_id;
    u32 firstOrderIndex = RenderGraphPassHandle::invalid_id;
    u32 lastOrderIndex = RenderGraphPassHandle::invalid_id;
};

struct RenderGraphMemoryStats {
    u32 peakAliveTransientResources = 0;
    u64 peakEstimatedTransientBytes = 0;
};

struct RenderGraphAllocationStats {
    u32 usedTransientResources = 0;
    u32 allocatedTransientTextures = 0;
    u32 aliasedTransientResources = 0;
    u64 allocatedEstimatedTransientBytes = 0;
};

struct RenderGraphBindingCacheStats {
    u64 layoutCreates = 0;
    u64 bindGroupCreates = 0;
    u64 bindGroupReuses = 0;
    u64 staticBindGroupCreates = 0;
    u64 staticBindGroupReuses = 0;
    u64 dynamicBindGroupCreates = 0;
    u64 dynamicBindGroupReuses = 0;
};

class RenderGraph {
public:
    RenderGraph() = default;
    ~RenderGraph();

    auto import_backbuffer(const String& name = "Backbuffer") -> RenderGraphResourceHandle;
    auto create_texture(const String& name, const TextureDesc& desc) -> RenderGraphResourceHandle;

    auto add_raster_pass(const RasterPassDesc& desc, RenderGraphExecuteFn executeFn)
        -> RenderGraphPassHandle;
    auto add_compute_pass(const ComputePassDesc& desc, RenderGraphExecuteFn executeFn)
        -> RenderGraphPassHandle;
    auto add_raster_draw_pass(const RasterDrawPassDesc& desc) -> RenderGraphPassHandle;
    auto add_compute_dispatch_pass(const ComputeDispatchPassDesc& desc) -> RenderGraphPassHandle;
    auto add_texture_binding(RenderGraphPassHandle pass, const RenderGraphTextureBindingDesc& binding)
        -> Result<void>;
    auto add_texture_bindings(
        RenderGraphPassHandle pass,
        const std::vector<RenderGraphTextureBindingDesc>& bindings) -> Result<void>;
    auto add_buffer_binding(RenderGraphPassHandle pass, const RenderGraphBufferBindingDesc& binding)
        -> Result<void>;
    auto add_buffer_bindings(
        RenderGraphPassHandle pass,
        const std::vector<RenderGraphBufferBindingDesc>& bindings) -> Result<void>;
    auto add_sampler_binding(RenderGraphPassHandle pass, const RenderGraphSamplerBindingDesc& binding)
        -> Result<void>;
    auto add_sampler_bindings(
        RenderGraphPassHandle pass,
        const std::vector<RenderGraphSamplerBindingDesc>& bindings) -> Result<void>;
    auto add_reads(RenderGraphPassHandle pass, const std::vector<RenderGraphResourceHandle>& resources)
        -> Result<void>;
    auto add_read(RenderGraphPassHandle pass, RenderGraphResourceHandle resource) -> Result<void>;
    auto add_writes(RenderGraphPassHandle pass, const std::vector<RenderGraphResourceHandle>& resources)
        -> Result<void>;
    auto add_write(RenderGraphPassHandle pass, RenderGraphResourceHandle resource) -> Result<void>;
    auto set_raster_pipeline(RenderGraphPassHandle pass, RenderPipeline* pipeline) -> Result<void>;
    auto set_compute_pipeline(RenderGraphPassHandle pass, ComputePipeline* pipeline) -> Result<void>;

    auto compile() -> Result<void>;
    auto execute(RenderDevice& device) -> Result<void>;
    auto resource_lifetimes() -> Result<std::vector<RenderGraphResourceLifetime>>;
    auto memory_stats() -> Result<RenderGraphMemoryStats>;
    auto allocation_stats() -> Result<RenderGraphAllocationStats>;
    [[nodiscard]] auto binding_cache_stats() const -> RenderGraphBindingCacheStats;
    auto debug_dump_text() -> Result<String>;

    void clear();

    [[nodiscard]] auto pass_count() const -> u32;
    [[nodiscard]] auto resource_count() const -> u32;
    [[nodiscard]] auto compiled_order() const -> const std::vector<u32>&;

private:
    enum class PassType : u8 {
        Raster,
        Compute,
    };

    enum class ResourceType : u8 {
        ImportedBackbuffer,
        TransientTexture,
    };

    struct ResourceNode {
        String name;
        ResourceType type = ResourceType::TransientTexture;
        TextureDesc textureDesc{};
    };

    struct PassNode {
        enum class BindingType : u8 {
            Texture,
            Buffer,
            Sampler,
        };

        struct CachedBindingEntry {
            BindingType type = BindingType::Texture;
            u32 binding = 0;
            u32 resourceId = RenderGraphResourceHandle::invalid_id;
            Buffer* buffer = nullptr;
            u64 offset = 0;
            u64 size = 0;
            Sampler* sampler = nullptr;
        };

        struct CachedBindingGroup {
            u32 group = 0;
            bool needsTextureView = false;
            std::vector<CachedBindingEntry> entries;
            void* layout = nullptr;
            void* staticBindGroup = nullptr;
            void* dynamicBindGroup = nullptr;
            u64 dynamicSignature = 0;
            bool dynamicSignatureValid = false;
        };

        PassType type = PassType::Raster;
        String name;
        RasterPassDesc desc{};
        std::vector<u32> reads;
        std::vector<u32> writes;
        std::vector<RenderGraphTextureBindingDesc> bindings;
        std::vector<RenderGraphBufferBindingDesc> bufferBindings;
        std::vector<RenderGraphSamplerBindingDesc> samplerBindings;
        RenderPipeline* rasterPipeline = nullptr;
        ComputePipeline* computePipeline = nullptr;
        RenderGraphExecuteFn executeFn;
        std::vector<CachedBindingGroup> cachedBindingGroups;
        bool bindingCacheBuilt = false;
    };

    struct AliasSlot {
        TextureDesc desc{};
        std::vector<u32> resources;
    };

    struct AliasPlan {
        std::vector<i32> slotByResource;
        std::vector<AliasSlot> slots;
        u32 usedTransientResources = 0;
    };

    [[nodiscard]] auto is_valid_resource(RenderGraphResourceHandle handle) const -> bool;
    [[nodiscard]] auto is_valid_pass(RenderGraphPassHandle handle) const -> bool;
    [[nodiscard]] auto build_alias_plan(const std::vector<RenderGraphResourceLifetime>& lifetimes) const
        -> AliasPlan;
    auto build_pass_binding_cache(PassNode& pass) -> Result<void>;
    void release_pass_binding_cache(PassNode& pass);

    std::vector<ResourceNode> m_resources;
    std::vector<PassNode> m_passes;
    std::vector<u32> m_compiledOrder;
    RenderGraphBindingCacheStats m_bindingCacheStats{};
    bool m_compiled = false;
    bool m_hasBackbuffer = false;
};

} // namespace firefly
