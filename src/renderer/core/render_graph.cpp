module;
#define WGPU_SHARED_LIBRARY
#include <webgpu/webgpu.h>
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <queue>
#include <sstream>
#include <unordered_set>
#include <vector>

module firefly.renderer.render_graph;

namespace firefly {

template <typename T>
static void hash_combine(u64& seed, const T& value) {
    const auto hashed = static_cast<u64>(std::hash<T>{}(value));
    seed ^= hashed + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
}

static auto estimate_format_bytes(TextureFormat format) -> u64 {
    switch (format) {
        case TextureFormat::RGBA8Unorm:
        case TextureFormat::BGRA8Unorm:
        case TextureFormat::Depth24PlusStencil8:
        case TextureFormat::Depth32Float:
            return 4;
        case TextureFormat::RGBA16Float:
            return 8;
        default:
            return 4;
    }
}

static auto are_alias_compatible(const TextureDesc& a, const TextureDesc& b) -> bool {
    return a.width == b.width &&
        a.height == b.height &&
        a.format == b.format &&
        a.usage == b.usage;
}

auto RenderGraphPassContext::texture_view(RenderGraphResourceHandle handle) const -> void* {
    if (!resourceViews || !handle.valid() || handle.id >= resourceViewCount) {
        return nullptr;
    }
    return const_cast<void*>(resourceViews[handle.id]);
}

auto RenderGraphPassContext::has_resource(RenderGraphResourceHandle handle) const -> bool {
    return texture_view(handle) != nullptr;
}

RenderGraph::~RenderGraph() {
    clear();
}

auto RenderGraph::import_backbuffer(const String& name) -> RenderGraphResourceHandle {
    if (m_hasBackbuffer) {
        for (u32 i = 0; i < static_cast<u32>(m_resources.size()); ++i) {
            if (m_resources[i].type == ResourceType::ImportedBackbuffer) {
                return RenderGraphResourceHandle{i};
            }
        }
    }

    ResourceNode node;
    node.name = name;
    node.type = ResourceType::ImportedBackbuffer;
    m_resources.push_back(std::move(node));
    m_hasBackbuffer = true;
    m_compiled = false;
    return RenderGraphResourceHandle{static_cast<u32>(m_resources.size() - 1)};
}

auto RenderGraph::create_texture(const String& name, const TextureDesc& desc)
    -> RenderGraphResourceHandle {
    ResourceNode node;
    node.name = name;
    node.type = ResourceType::TransientTexture;
    node.textureDesc = desc;
    m_resources.push_back(std::move(node));
    m_compiled = false;
    return RenderGraphResourceHandle{static_cast<u32>(m_resources.size() - 1)};
}

auto RenderGraph::add_raster_pass(const RasterPassDesc& desc, RenderGraphExecuteFn executeFn)
    -> RenderGraphPassHandle {
    if (!executeFn) {
        return {};
    }
    if (!desc.colorTarget.valid() && !desc.depthTarget.valid()) {
        return {};
    }
    if (desc.colorTarget.valid() && !is_valid_resource(desc.colorTarget)) {
        return {};
    }
    if (desc.depthTarget.valid()) {
        if (!is_valid_resource(desc.depthTarget)) {
            return {};
        }
        if (desc.colorTarget.valid() && desc.depthTarget.id == desc.colorTarget.id) {
            return {};
        }
    }

    PassNode node;
    node.type = PassType::Raster;
    node.name = desc.name;
    node.desc = desc;
    if (desc.colorTarget.valid()) {
        node.writes.push_back(desc.colorTarget.id);
    }
    if (desc.depthTarget.valid()) {
        node.writes.push_back(desc.depthTarget.id);
    }
    node.executeFn = std::move(executeFn);
    m_passes.push_back(std::move(node));
    m_compiled = false;
    return RenderGraphPassHandle{static_cast<u32>(m_passes.size() - 1)};
}

auto RenderGraph::add_compute_pass(const ComputePassDesc& desc, RenderGraphExecuteFn executeFn)
    -> RenderGraphPassHandle {
    if (!executeFn) {
        return {};
    }

    PassNode node;
    node.type = PassType::Compute;
    node.name = desc.name;
    node.executeFn = std::move(executeFn);
    m_passes.push_back(std::move(node));
    const auto handle = RenderGraphPassHandle{static_cast<u32>(m_passes.size() - 1)};

    if (!desc.writes.empty()) {
        const auto result = add_writes(handle, desc.writes);
        if (result.is_error()) {
            m_passes.pop_back();
            return {};
        }
    }

    m_compiled = false;
    return handle;
}

auto RenderGraph::add_raster_draw_pass(const RasterDrawPassDesc& desc)
    -> RenderGraphPassHandle {
    if (!desc.pipeline || desc.vertexCount == 0 || desc.instanceCount == 0) {
        return {};
    }

    auto pass = add_raster_pass(
        desc.pass,
        [pipeline = desc.pipeline,
         vertexCount = desc.vertexCount,
         instanceCount = desc.instanceCount,
         firstVertex = desc.firstVertex,
         firstInstance = desc.firstInstance](RenderGraphPassContext& ctx) {
            ctx.command().set_pipeline(*pipeline);
            ctx.command().draw(vertexCount, instanceCount, firstVertex, firstInstance);
        }
    );
    if (!pass.valid()) {
        return {};
    }

    if (!desc.reads.empty()) {
        const auto addReadResult = add_reads(pass, desc.reads);
        if (addReadResult.is_error()) {
            m_passes.pop_back();
            m_compiled = false;
            return {};
        }
    }
    if (!desc.bindings.empty()) {
        const auto addBindingResult = add_texture_bindings(pass, desc.bindings);
        if (addBindingResult.is_error()) {
            m_passes.pop_back();
            m_compiled = false;
            return {};
        }
    }
    if (!desc.bufferBindings.empty()) {
        const auto addBufferResult = add_buffer_bindings(pass, desc.bufferBindings);
        if (addBufferResult.is_error()) {
            m_passes.pop_back();
            m_compiled = false;
            return {};
        }
    }
    if (!desc.samplerBindings.empty()) {
        const auto addSamplerResult = add_sampler_bindings(pass, desc.samplerBindings);
        if (addSamplerResult.is_error()) {
            m_passes.pop_back();
            m_compiled = false;
            return {};
        }
    }
    m_passes[pass.id].rasterPipeline = desc.pipeline;

    return pass;
}

auto RenderGraph::add_compute_dispatch_pass(const ComputeDispatchPassDesc& desc)
    -> RenderGraphPassHandle {
    if (!desc.pipeline || desc.groupCountX == 0 || desc.groupCountY == 0 || desc.groupCountZ == 0) {
        return {};
    }

    auto pass = add_compute_pass(
        desc.pass,
        [pipeline = desc.pipeline,
         groupCountX = desc.groupCountX,
         groupCountY = desc.groupCountY,
         groupCountZ = desc.groupCountZ](RenderGraphPassContext& ctx) {
            ctx.command().set_compute_pipeline(*pipeline);
            ctx.command().dispatch(groupCountX, groupCountY, groupCountZ);
        }
    );
    if (!pass.valid()) {
        return {};
    }

    if (!desc.reads.empty()) {
        const auto addReadResult = add_reads(pass, desc.reads);
        if (addReadResult.is_error()) {
            m_passes.pop_back();
            m_compiled = false;
            return {};
        }
    }
    if (!desc.bindings.empty()) {
        const auto addBindingResult = add_texture_bindings(pass, desc.bindings);
        if (addBindingResult.is_error()) {
            m_passes.pop_back();
            m_compiled = false;
            return {};
        }
    }
    if (!desc.bufferBindings.empty()) {
        const auto addBufferResult = add_buffer_bindings(pass, desc.bufferBindings);
        if (addBufferResult.is_error()) {
            m_passes.pop_back();
            m_compiled = false;
            return {};
        }
    }
    if (!desc.samplerBindings.empty()) {
        const auto addSamplerResult = add_sampler_bindings(pass, desc.samplerBindings);
        if (addSamplerResult.is_error()) {
            m_passes.pop_back();
            m_compiled = false;
            return {};
        }
    }
    m_passes[pass.id].computePipeline = desc.pipeline;

    return pass;
}

auto RenderGraph::add_texture_binding(
    RenderGraphPassHandle pass,
    const RenderGraphTextureBindingDesc& binding) -> Result<void> {
    if (!is_valid_pass(pass)) {
        return Result<void>::error("Invalid pass handle");
    }
    if (!is_valid_resource(binding.resource)) {
        return Result<void>::error("Invalid resource handle");
    }

    auto& passNode = m_passes[pass.id];
    const auto slot_used = [&](u32 group, u32 slot) {
        const auto hasTexture = std::find_if(
            passNode.bindings.begin(), passNode.bindings.end(),
            [&](const RenderGraphTextureBindingDesc& existing) {
                return existing.group == group && existing.binding == slot;
            }
        ) != passNode.bindings.end();
        const auto hasBuffer = std::find_if(
            passNode.bufferBindings.begin(), passNode.bufferBindings.end(),
            [&](const RenderGraphBufferBindingDesc& existing) {
                return existing.group == group && existing.binding == slot;
            }
        ) != passNode.bufferBindings.end();
        const auto hasSampler = std::find_if(
            passNode.samplerBindings.begin(), passNode.samplerBindings.end(),
            [&](const RenderGraphSamplerBindingDesc& existing) {
                return existing.group == group && existing.binding == slot;
            }
        ) != passNode.samplerBindings.end();
        return hasTexture || hasBuffer || hasSampler;
    };
    if (slot_used(binding.group, binding.binding)) {
        return Result<void>::error("Duplicate texture binding slot in pass");
    }

    auto useResult = Result<void>::ok();
    if (binding.access == RenderGraphBindingAccess::Read) {
        useResult = add_read(pass, binding.resource);
    } else {
        useResult = add_write(pass, binding.resource);
    }
    if (useResult.is_error()) {
        return useResult;
    }

    passNode.bindings.push_back(binding);
    m_compiled = false;
    return Result<void>::ok();
}

auto RenderGraph::add_texture_bindings(
    RenderGraphPassHandle pass,
    const std::vector<RenderGraphTextureBindingDesc>& bindings) -> Result<void> {
    for (const auto& binding : bindings) {
        const auto result = add_texture_binding(pass, binding);
        if (result.is_error()) {
            return result;
        }
    }
    return Result<void>::ok();
}

auto RenderGraph::add_buffer_binding(
    RenderGraphPassHandle pass,
    const RenderGraphBufferBindingDesc& binding) -> Result<void> {
    if (!is_valid_pass(pass)) {
        return Result<void>::error("Invalid pass handle");
    }
    if (!binding.buffer) {
        return Result<void>::error("Buffer binding requires non-null buffer");
    }

    auto& passNode = m_passes[pass.id];
    const auto slot_used = [&](u32 group, u32 slot) {
        const auto hasTexture = std::find_if(
            passNode.bindings.begin(), passNode.bindings.end(),
            [&](const RenderGraphTextureBindingDesc& existing) {
                return existing.group == group && existing.binding == slot;
            }
        ) != passNode.bindings.end();
        const auto hasBuffer = std::find_if(
            passNode.bufferBindings.begin(), passNode.bufferBindings.end(),
            [&](const RenderGraphBufferBindingDesc& existing) {
                return existing.group == group && existing.binding == slot;
            }
        ) != passNode.bufferBindings.end();
        const auto hasSampler = std::find_if(
            passNode.samplerBindings.begin(), passNode.samplerBindings.end(),
            [&](const RenderGraphSamplerBindingDesc& existing) {
                return existing.group == group && existing.binding == slot;
            }
        ) != passNode.samplerBindings.end();
        return hasTexture || hasBuffer || hasSampler;
    };
    if (slot_used(binding.group, binding.binding)) {
        return Result<void>::error("Duplicate buffer binding slot in pass");
    }

    if (binding.buffer->size() > 0 && binding.offset > binding.buffer->size()) {
        return Result<void>::error("Buffer binding offset out of range");
    }

    auto normalized = binding;
    if (normalized.size == 0) {
        if (normalized.buffer->size() > normalized.offset) {
            normalized.size = normalized.buffer->size() - normalized.offset;
        } else {
            normalized.size = WGPU_WHOLE_SIZE;
        }
    }
    if (normalized.size == 0) {
        return Result<void>::error("Buffer binding size cannot be zero");
    }
    if (normalized.size != WGPU_WHOLE_SIZE &&
        normalized.buffer->size() > 0 &&
        normalized.offset + normalized.size > normalized.buffer->size()) {
        return Result<void>::error("Buffer binding range out of buffer bounds");
    }

    passNode.bufferBindings.push_back(normalized);
    m_compiled = false;
    return Result<void>::ok();
}

auto RenderGraph::add_buffer_bindings(
    RenderGraphPassHandle pass,
    const std::vector<RenderGraphBufferBindingDesc>& bindings) -> Result<void> {
    for (const auto& binding : bindings) {
        const auto result = add_buffer_binding(pass, binding);
        if (result.is_error()) {
            return result;
        }
    }
    return Result<void>::ok();
}

auto RenderGraph::add_sampler_binding(
    RenderGraphPassHandle pass,
    const RenderGraphSamplerBindingDesc& binding) -> Result<void> {
    if (!is_valid_pass(pass)) {
        return Result<void>::error("Invalid pass handle");
    }
    if (!binding.sampler) {
        return Result<void>::error("Sampler binding requires non-null sampler");
    }

    auto& passNode = m_passes[pass.id];
    const auto slot_used = [&](u32 group, u32 slot) {
        const auto hasTexture = std::find_if(
            passNode.bindings.begin(), passNode.bindings.end(),
            [&](const RenderGraphTextureBindingDesc& existing) {
                return existing.group == group && existing.binding == slot;
            }
        ) != passNode.bindings.end();
        const auto hasBuffer = std::find_if(
            passNode.bufferBindings.begin(), passNode.bufferBindings.end(),
            [&](const RenderGraphBufferBindingDesc& existing) {
                return existing.group == group && existing.binding == slot;
            }
        ) != passNode.bufferBindings.end();
        const auto hasSampler = std::find_if(
            passNode.samplerBindings.begin(), passNode.samplerBindings.end(),
            [&](const RenderGraphSamplerBindingDesc& existing) {
                return existing.group == group && existing.binding == slot;
            }
        ) != passNode.samplerBindings.end();
        return hasTexture || hasBuffer || hasSampler;
    };
    if (slot_used(binding.group, binding.binding)) {
        return Result<void>::error("Duplicate sampler binding slot in pass");
    }

    passNode.samplerBindings.push_back(binding);
    m_compiled = false;
    return Result<void>::ok();
}

auto RenderGraph::add_sampler_bindings(
    RenderGraphPassHandle pass,
    const std::vector<RenderGraphSamplerBindingDesc>& bindings) -> Result<void> {
    for (const auto& binding : bindings) {
        const auto result = add_sampler_binding(pass, binding);
        if (result.is_error()) {
            return result;
        }
    }
    return Result<void>::ok();
}

auto RenderGraph::add_reads(
    RenderGraphPassHandle pass,
    const std::vector<RenderGraphResourceHandle>& resources) -> Result<void> {
    for (const auto& resource : resources) {
        auto result = add_read(pass, resource);
        if (result.is_error()) {
            return result;
        }
    }
    return Result<void>::ok();
}

auto RenderGraph::add_read(RenderGraphPassHandle pass, RenderGraphResourceHandle resource)
    -> Result<void> {
    if (!is_valid_pass(pass)) {
        return Result<void>::error("Invalid pass handle");
    }
    if (!is_valid_resource(resource)) {
        return Result<void>::error("Invalid resource handle");
    }

    auto& reads = m_passes[pass.id].reads;
    if (std::find(reads.begin(), reads.end(), resource.id) == reads.end()) {
        reads.push_back(resource.id);
        m_compiled = false;
    }
    return Result<void>::ok();
}

auto RenderGraph::add_writes(
    RenderGraphPassHandle pass,
    const std::vector<RenderGraphResourceHandle>& resources) -> Result<void> {
    for (const auto& resource : resources) {
        auto result = add_write(pass, resource);
        if (result.is_error()) {
            return result;
        }
    }
    return Result<void>::ok();
}

auto RenderGraph::add_write(RenderGraphPassHandle pass, RenderGraphResourceHandle resource)
    -> Result<void> {
    if (!is_valid_pass(pass)) {
        return Result<void>::error("Invalid pass handle");
    }
    if (!is_valid_resource(resource)) {
        return Result<void>::error("Invalid resource handle");
    }

    auto& writes = m_passes[pass.id].writes;
    if (std::find(writes.begin(), writes.end(), resource.id) == writes.end()) {
        writes.push_back(resource.id);
        m_compiled = false;
    }
    return Result<void>::ok();
}

auto RenderGraph::set_raster_pipeline(RenderGraphPassHandle pass, RenderPipeline* pipeline)
    -> Result<void> {
    if (!is_valid_pass(pass)) {
        return Result<void>::error("Invalid pass handle");
    }
    if (!pipeline) {
        return Result<void>::error("Raster pipeline cannot be null");
    }
    auto& passNode = m_passes[pass.id];
    if (passNode.type != PassType::Raster) {
        return Result<void>::error("Pass is not raster type");
    }
    passNode.rasterPipeline = pipeline;
    m_compiled = false;
    return Result<void>::ok();
}

auto RenderGraph::set_compute_pipeline(RenderGraphPassHandle pass, ComputePipeline* pipeline)
    -> Result<void> {
    if (!is_valid_pass(pass)) {
        return Result<void>::error("Invalid pass handle");
    }
    if (!pipeline) {
        return Result<void>::error("Compute pipeline cannot be null");
    }
    auto& passNode = m_passes[pass.id];
    if (passNode.type != PassType::Compute) {
        return Result<void>::error("Pass is not compute type");
    }
    passNode.computePipeline = pipeline;
    m_compiled = false;
    return Result<void>::ok();
}

auto RenderGraph::build_pass_binding_cache(PassNode& pass) -> Result<void> {
    release_pass_binding_cache(pass);
    pass.bindingCacheBuilt = true;

    const bool hasAnyBindings =
        !pass.bindings.empty() ||
        !pass.bufferBindings.empty() ||
        !pass.samplerBindings.empty();
    if (!hasAnyBindings) {
        return Result<void>::ok();
    }

    std::unordered_map<u32, std::vector<PassNode::CachedBindingEntry>> entriesByGroup;
    for (const auto& binding : pass.bindings) {
        PassNode::CachedBindingEntry item{};
        item.type = PassNode::BindingType::Texture;
        item.binding = binding.binding;
        item.resourceId = binding.resource.id;
        entriesByGroup[binding.group].push_back(item);
    }
    for (const auto& binding : pass.bufferBindings) {
        PassNode::CachedBindingEntry item{};
        item.type = PassNode::BindingType::Buffer;
        item.binding = binding.binding;
        item.buffer = binding.buffer;
        item.offset = binding.offset;
        item.size = binding.size;
        entriesByGroup[binding.group].push_back(item);
    }
    for (const auto& binding : pass.samplerBindings) {
        PassNode::CachedBindingEntry item{};
        item.type = PassNode::BindingType::Sampler;
        item.binding = binding.binding;
        item.sampler = binding.sampler;
        entriesByGroup[binding.group].push_back(item);
    }

    std::vector<u32> groups;
    groups.reserve(entriesByGroup.size());
    for (const auto& pair : entriesByGroup) {
        groups.push_back(pair.first);
    }
    std::sort(groups.begin(), groups.end());

    for (const auto groupId : groups) {
        auto& entries = entriesByGroup[groupId];
        std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.binding < rhs.binding;
        });

        PassNode::CachedBindingGroup group{};
        group.group = groupId;
        group.entries = std::move(entries);
        for (const auto& entry : group.entries) {
            if (entry.type == PassNode::BindingType::Texture) {
                group.needsTextureView = true;
                break;
            }
        }
        pass.cachedBindingGroups.push_back(std::move(group));
    }

    return Result<void>::ok();
}

void RenderGraph::release_pass_binding_cache(PassNode& pass) {
    for (auto& group : pass.cachedBindingGroups) {
        if (group.staticBindGroup) {
            wgpuBindGroupRelease(static_cast<WGPUBindGroup>(group.staticBindGroup));
            group.staticBindGroup = nullptr;
        }
        if (group.dynamicBindGroup) {
            wgpuBindGroupRelease(static_cast<WGPUBindGroup>(group.dynamicBindGroup));
            group.dynamicBindGroup = nullptr;
        }
        group.dynamicSignatureValid = false;
        if (group.layout) {
            wgpuBindGroupLayoutRelease(static_cast<WGPUBindGroupLayout>(group.layout));
            group.layout = nullptr;
        }
    }
    pass.cachedBindingGroups.clear();
    pass.bindingCacheBuilt = false;
}

auto RenderGraph::compile() -> Result<void> {
    for (auto& pass : m_passes) {
        release_pass_binding_cache(pass);
    }
    m_compiledOrder.clear();
    if (m_passes.empty()) {
        m_compiled = true;
        return Result<void>::ok();
    }

    std::vector<std::unordered_set<u32>> edges(m_passes.size());
    std::vector<std::unordered_set<u32>> reverseEdges(m_passes.size());
    std::vector<u32> indegree(m_passes.size(), 0);
    std::vector<i32> lastWriter(m_resources.size(), -1);

    for (u32 passIndex = 0; passIndex < static_cast<u32>(m_passes.size()); ++passIndex) {
        const auto& pass = m_passes[passIndex];
        if (pass.writes.empty()) {
            return Result<void>::error("Each pass must declare at least one write resource");
        }

        std::unordered_set<u32> writeSet;
        for (const auto writeResource : pass.writes) {
            if (writeResource >= m_resources.size()) {
                return Result<void>::error("Pass write references out-of-range resource");
            }

            const auto& resource = m_resources[writeResource];
            if (resource.type == ResourceType::ImportedBackbuffer &&
                pass.type == PassType::Compute) {
                return Result<void>::error(
                    "Compute pass cannot write imported backbuffer resource");
            }
            if (resource.type == ResourceType::TransientTexture) {
                if (pass.type == PassType::Raster &&
                    !has_usage(resource.textureDesc.usage, TextureUsage::RenderAttachment)) {
                    return Result<void>::error(
                        "Raster pass writes require TextureUsage::RenderAttachment");
                }
                if (pass.type == PassType::Compute &&
                    !has_usage(resource.textureDesc.usage, TextureUsage::StorageBinding)) {
                    return Result<void>::error(
                        "Compute pass writes require TextureUsage::StorageBinding");
                }
            }
            writeSet.insert(writeResource);
        }

        for (const auto& binding : pass.bindings) {
            if (binding.resource.id >= m_resources.size()) {
                return Result<void>::error("Texture binding references out-of-range resource");
            }
            const auto& resource = m_resources[binding.resource.id];
            if (resource.type == ResourceType::ImportedBackbuffer) {
                return Result<void>::error(
                    "Imported backbuffer cannot be used as shader texture binding");
            }
            if (binding.access == RenderGraphBindingAccess::Read &&
                !has_usage(resource.textureDesc.usage, TextureUsage::TextureBinding)) {
                return Result<void>::error(
                    "Texture read binding requires TextureUsage::TextureBinding");
            }
            if (binding.access == RenderGraphBindingAccess::Write &&
                !has_usage(resource.textureDesc.usage, TextureUsage::StorageBinding)) {
                return Result<void>::error(
                    "Texture write binding requires TextureUsage::StorageBinding");
            }
        }

        for (const auto readResource : pass.reads) {
            if (writeSet.contains(readResource)) {
                return Result<void>::error(
                    "A pass cannot read and write the same resource in a single pass");
            }
            if (readResource >= m_resources.size()) {
                return Result<void>::error("Pass read references out-of-range resource");
            }

            const auto writer = lastWriter[readResource];
            if (writer >= 0) {
                if (edges[static_cast<u32>(writer)].insert(passIndex).second) {
                    ++indegree[passIndex];
                    reverseEdges[passIndex].insert(static_cast<u32>(writer));
                }
            } else if (m_resources[readResource].type != ResourceType::ImportedBackbuffer) {
                return Result<void>::error(
                    "Transient resource is read before being produced by any pass");
            }
        }

        for (const auto writeResource : pass.writes) {
            const auto previousWriter = lastWriter[writeResource];
            if (previousWriter >= 0) {
                if (edges[static_cast<u32>(previousWriter)].insert(passIndex).second) {
                    ++indegree[passIndex];
                    reverseEdges[passIndex].insert(static_cast<u32>(previousWriter));
                }
            }
            lastWriter[writeResource] = static_cast<i32>(passIndex);
        }
    }

    std::priority_queue<u32, std::vector<u32>, std::greater<u32>> ready;
    for (u32 i = 0; i < static_cast<u32>(indegree.size()); ++i) {
        if (indegree[i] == 0) {
            ready.emplace(i);
        }
    }

    while (!ready.empty()) {
        const auto current = ready.top();
        ready.pop();
        m_compiledOrder.push_back(current);

        for (const auto next : edges[current]) {
            --indegree[next];
            if (indegree[next] == 0) {
                ready.emplace(next);
            }
        }
    }

    if (m_compiledOrder.size() != m_passes.size()) {
        return Result<void>::error("RenderGraph has dependency cycle");
    }

    // Dead-pass culling: keep only passes that contribute to imported outputs.
    std::vector<u32> rootPasses;
    rootPasses.reserve(m_passes.size());
    for (u32 passIndex = 0; passIndex < static_cast<u32>(m_passes.size()); ++passIndex) {
        const auto& writes = m_passes[passIndex].writes;
        for (const auto resource : writes) {
            if (resource < m_resources.size() &&
                m_resources[resource].type == ResourceType::ImportedBackbuffer) {
                rootPasses.push_back(passIndex);
                break;
            }
        }
    }

    if (!rootPasses.empty()) {
        std::vector<bool> keep(m_passes.size(), false);
        std::queue<u32> pending;
        for (const auto root : rootPasses) {
            if (!keep[root]) {
                keep[root] = true;
                pending.push(root);
            }
        }

        while (!pending.empty()) {
            const auto current = pending.front();
            pending.pop();
            for (const auto dependency : reverseEdges[current]) {
                if (!keep[dependency]) {
                    keep[dependency] = true;
                    pending.push(dependency);
                }
            }
        }

        std::vector<u32> filteredOrder;
        filteredOrder.reserve(m_compiledOrder.size());
        for (const auto passIndex : m_compiledOrder) {
            if (keep[passIndex]) {
                filteredOrder.push_back(passIndex);
            }
        }
        m_compiledOrder = std::move(filteredOrder);
    }

    for (const auto passIndex : m_compiledOrder) {
        auto cacheResult = build_pass_binding_cache(m_passes[passIndex]);
        if (cacheResult.is_error()) {
            return cacheResult;
        }
    }

    m_compiled = true;
    return Result<void>::ok();
}

auto RenderGraph::execute(RenderDevice& device) -> Result<void> {
    if (!m_compiled) {
        auto compileResult = compile();
        if (compileResult.is_error()) {
            return compileResult;
        }
    }
    if (m_passes.empty()) {
        return Result<void>::ok();
    }
    m_bindingCacheStats = {};

    auto lifetimesResult = resource_lifetimes();
    if (lifetimesResult.is_error()) {
        return Result<void>::error(lifetimesResult.error());
    }
    const auto& lifetimes = lifetimesResult.value();
    const auto aliasPlan = build_alias_plan(lifetimes);

    auto* commandBuffer = device.begin_frame();
    if (!commandBuffer) {
        return Result<void>::error("Failed to begin frame");
    }

    std::vector<Ptr<Texture>> aliasTextures(aliasPlan.slots.size());
    std::vector<const void*> resourceViews(m_resources.size(), nullptr);

    for (u32 slotIndex = 0; slotIndex < static_cast<u32>(aliasPlan.slots.size()); ++slotIndex) {
        auto desc = aliasPlan.slots[slotIndex].desc;
        if (desc.width == 0) {
            desc.width = device.surface_width();
        }
        if (desc.height == 0) {
            desc.height = device.surface_height();
        }
        if (desc.width == 0 || desc.height == 0) {
            return Result<void>::error(
                "Failed to allocate transient texture: unresolved width/height");
        }

        auto texture = device.create_texture(desc);
        if (!texture || !texture->view()) {
            return Result<void>::error("Failed to allocate aliased transient render-graph texture");
        }
        aliasTextures[slotIndex] = std::move(texture);
    }

    for (u32 resourceIndex = 0; resourceIndex < static_cast<u32>(m_resources.size()); ++resourceIndex) {
        const auto& resource = m_resources[resourceIndex];
        if (resource.type == ResourceType::ImportedBackbuffer) {
            resourceViews[resourceIndex] = device.current_texture_view();
            continue;
        }

        if (resourceIndex < aliasPlan.slotByResource.size()) {
            const auto slot = aliasPlan.slotByResource[resourceIndex];
            if (slot >= 0) {
                auto& texture = aliasTextures[static_cast<u32>(slot)];
                resourceViews[resourceIndex] = texture ? texture->view() : nullptr;
            }
        }
    }

    for (const auto passIndex : m_compiledOrder) {
        auto& pass = m_passes[passIndex];
        // WebGPU handles hazards implicitly; render-graph only enforces dependency order.
        if (pass.type == PassType::Raster) {
            void* colorTargetView = nullptr;
            if (pass.desc.colorTarget.valid()) {
                const auto colorIndex = pass.desc.colorTarget.id;
                if (colorIndex >= resourceViews.size()) {
                    return Result<void>::error("Render pass color target is out of range");
                }
                auto* rawColorView = const_cast<void*>(resourceViews[colorIndex]);
                if (!rawColorView) {
                    return Result<void>::error("Render pass color target view is unavailable");
                }
                colorTargetView = rawColorView;
            }
            void* depthTargetView = nullptr;
            if (pass.desc.depthTarget.valid()) {
                const auto depthIndex = pass.desc.depthTarget.id;
                if (depthIndex >= resourceViews.size()) {
                    return Result<void>::error("Render pass depth target is out of range");
                }
                auto* rawDepthView = const_cast<void*>(resourceViews[depthIndex]);
                if (!rawDepthView) {
                    return Result<void>::error("Render pass depth target view is unavailable");
                }
                depthTargetView = rawDepthView;
            }

            commandBuffer->begin_render_pass(
                colorTargetView, depthTargetView,
                pass.desc.clearR, pass.desc.clearG, pass.desc.clearB, pass.desc.clearA,
                pass.desc.clearColor, pass.desc.clearDepth, pass.desc.clearDepthValue
            );
        } else {
            commandBuffer->begin_compute_pass();
        }

        const auto fail_pass = [&](const char* message) -> Result<void> {
            if (pass.type == PassType::Raster) {
                commandBuffer->end_render_pass();
            } else {
                commandBuffer->end_compute_pass();
            }
            return Result<void>::error(message);
        };
        const bool canAutoBind =
            (pass.type == PassType::Raster && pass.rasterPipeline) ||
            (pass.type == PassType::Compute && pass.computePipeline);
        const bool hasAnyBindings = !pass.cachedBindingGroups.empty();
        if (hasAnyBindings && canAutoBind) {
            if (pass.type == PassType::Raster && !pass.rasterPipeline->native_handle()) {
                return fail_pass("Raster auto-binding requires valid raster pipeline handle");
            }
            if (pass.type == PassType::Compute && !pass.computePipeline->native_handle()) {
                return fail_pass("Compute auto-binding requires valid compute pipeline handle");
            }

            const auto build_entries_and_signature =
                [&](const PassNode::CachedBindingGroup& sourceGroup,
                    std::vector<WGPUBindGroupEntry>& entries,
                    u64& dynamicSignature) -> Result<void> {
                entries.clear();
                entries.reserve(sourceGroup.entries.size());
                dynamicSignature = 0;
                hash_combine(dynamicSignature, sourceGroup.group);

                for (const auto& source : sourceGroup.entries) {
                    WGPUBindGroupEntry entry{};
                    entry.binding = source.binding;
                    hash_combine(dynamicSignature, static_cast<u32>(source.type));
                    hash_combine(dynamicSignature, source.binding);

                    if (source.type == PassNode::BindingType::Texture) {
                        if (source.resourceId >= resourceViews.size()) {
                            return Result<void>::error(
                                "Texture binding references out-of-range resource");
                        }
                        auto* textureView = const_cast<void*>(resourceViews[source.resourceId]);
                        if (!textureView) {
                            return Result<void>::error(
                                "Texture binding references unavailable resource view");
                        }
                        entry.textureView = static_cast<WGPUTextureView>(textureView);
                        hash_combine(
                            dynamicSignature,
                            static_cast<u64>(reinterpret_cast<uintptr_t>(textureView)));
                    } else if (source.type == PassNode::BindingType::Buffer) {
                        if (!source.buffer || !source.buffer->native_handle()) {
                            return Result<void>::error(
                                "Buffer binding references unavailable buffer");
                        }
                        if (source.size != WGPU_WHOLE_SIZE &&
                            source.buffer->size() > 0 &&
                            source.offset + source.size > source.buffer->size()) {
                            return Result<void>::error(
                                "Buffer binding range out of bounds at execute");
                        }
                        entry.buffer = static_cast<WGPUBuffer>(source.buffer->native_handle());
                        entry.offset = source.offset;
                        entry.size = source.size;
                        hash_combine(
                            dynamicSignature,
                            static_cast<u64>(
                                reinterpret_cast<uintptr_t>(source.buffer->native_handle())));
                        hash_combine(dynamicSignature, source.offset);
                        hash_combine(dynamicSignature, source.size);
                    } else {
                        if (!source.sampler || !source.sampler->native_handle()) {
                            return Result<void>::error(
                                "Sampler binding references unavailable sampler");
                        }
                        entry.sampler = static_cast<WGPUSampler>(source.sampler->native_handle());
                        hash_combine(
                            dynamicSignature,
                            static_cast<u64>(
                                reinterpret_cast<uintptr_t>(source.sampler->native_handle())));
                    }

                    entries.push_back(entry);
                }
                return Result<void>::ok();
            };

            for (auto& group : pass.cachedBindingGroups) {
                if (!group.layout) {
                    if (pass.type == PassType::Raster) {
                        group.layout = wgpuRenderPipelineGetBindGroupLayout(
                            static_cast<WGPURenderPipeline>(pass.rasterPipeline->native_handle()),
                            group.group
                        );
                    } else {
                        group.layout = wgpuComputePipelineGetBindGroupLayout(
                            static_cast<WGPUComputePipeline>(pass.computePipeline->native_handle()),
                            group.group
                        );
                    }
                    if (!group.layout) {
                        return fail_pass("Failed to acquire bind-group layout for pass");
                    }
                    ++m_bindingCacheStats.layoutCreates;
                }

                if (!group.needsTextureView && group.staticBindGroup) {
                    ++m_bindingCacheStats.bindGroupReuses;
                    ++m_bindingCacheStats.staticBindGroupReuses;
                    commandBuffer->set_bind_group(group.group, group.staticBindGroup);
                    continue;
                }

                std::vector<WGPUBindGroupEntry> entries;
                entries.reserve(group.entries.size());
                u64 dynamicSignature = 0;
                if (const auto buildResult =
                        build_entries_and_signature(group, entries, dynamicSignature);
                    buildResult.is_error()) {
                    return fail_pass(buildResult.error().c_str());
                }

                if (group.needsTextureView &&
                    group.dynamicBindGroup &&
                    group.dynamicSignatureValid &&
                    group.dynamicSignature == dynamicSignature) {
                    ++m_bindingCacheStats.bindGroupReuses;
                    ++m_bindingCacheStats.dynamicBindGroupReuses;
                    commandBuffer->set_bind_group(group.group, group.dynamicBindGroup);
                    continue;
                }

                WGPUBindGroupDescriptor bgDesc{};
                bgDesc.layout = static_cast<WGPUBindGroupLayout>(group.layout);
                bgDesc.entryCount = static_cast<u32>(entries.size());
                bgDesc.entries = entries.data();
                auto bindGroup = wgpuDeviceCreateBindGroup(
                    static_cast<WGPUDevice>(device.wgpu_device()),
                    &bgDesc
                );
                if (!bindGroup) {
                    return fail_pass("Failed to create pass bind-group");
                }
                ++m_bindingCacheStats.bindGroupCreates;

                if (!group.needsTextureView) {
                    group.staticBindGroup = bindGroup;
                    ++m_bindingCacheStats.staticBindGroupCreates;
                    commandBuffer->set_bind_group(group.group, bindGroup);
                } else {
                    if (group.dynamicBindGroup) {
                        wgpuBindGroupRelease(static_cast<WGPUBindGroup>(group.dynamicBindGroup));
                    }
                    group.dynamicBindGroup = bindGroup;
                    group.dynamicSignature = dynamicSignature;
                    group.dynamicSignatureValid = true;
                    ++m_bindingCacheStats.dynamicBindGroupCreates;
                    commandBuffer->set_bind_group(group.group, bindGroup);
                }
            }
        }

        RenderGraphPassContext context{
            *commandBuffer,
            device,
            resourceViews.empty() ? nullptr : resourceViews.data(),
            static_cast<u32>(resourceViews.size()),
        };
        pass.executeFn(context);

        if (pass.type == PassType::Raster) {
            commandBuffer->end_render_pass();
        } else {
            commandBuffer->end_compute_pass();
        }
    }

    device.end_frame();
    device.present();
    return Result<void>::ok();
}

auto RenderGraph::resource_lifetimes() -> Result<std::vector<RenderGraphResourceLifetime>> {
    if (!m_compiled) {
        auto compileResult = compile();
        if (compileResult.is_error()) {
            return Result<std::vector<RenderGraphResourceLifetime>>::error(compileResult.error());
        }
    }

    std::vector<RenderGraphResourceLifetime> lifetimes(m_resources.size());
    for (u32 resourceIndex = 0; resourceIndex < static_cast<u32>(m_resources.size()); ++resourceIndex) {
        auto& item = lifetimes[resourceIndex];
        item.resource = RenderGraphResourceHandle{resourceIndex};
        item.imported = (m_resources[resourceIndex].type == ResourceType::ImportedBackbuffer);
    }

    for (u32 orderIndex = 0; orderIndex < static_cast<u32>(m_compiledOrder.size()); ++orderIndex) {
        const auto passIndex = m_compiledOrder[orderIndex];
        const auto& pass = m_passes[passIndex];

        auto mark_use = [&](u32 resourceIndex) {
            auto& item = lifetimes[resourceIndex];
            if (!item.used) {
                item.used = true;
                item.firstPass = passIndex;
                item.lastPass = passIndex;
                item.firstOrderIndex = orderIndex;
                item.lastOrderIndex = orderIndex;
            } else {
                item.lastPass = passIndex;
                item.lastOrderIndex = orderIndex;
            }
        };

        for (const auto readResource : pass.reads) {
            mark_use(readResource);
        }
        for (const auto writeResource : pass.writes) {
            mark_use(writeResource);
        }
    }

    return Result<std::vector<RenderGraphResourceLifetime>>::ok(std::move(lifetimes));
}

auto RenderGraph::memory_stats() -> Result<RenderGraphMemoryStats> {
    auto lifetimeResult = resource_lifetimes();
    if (lifetimeResult.is_error()) {
        return Result<RenderGraphMemoryStats>::error(lifetimeResult.error());
    }
    const auto& lifetimes = lifetimeResult.value();

    RenderGraphMemoryStats stats{};
    if (m_compiledOrder.empty()) {
        return Result<RenderGraphMemoryStats>::ok(stats);
    }

    for (u32 orderIndex = 0; orderIndex < static_cast<u32>(m_compiledOrder.size()); ++orderIndex) {
        u32 aliveCount = 0;
        u64 aliveBytes = 0;
        for (u32 resourceIndex = 0; resourceIndex < static_cast<u32>(m_resources.size()); ++resourceIndex) {
            const auto& resource = m_resources[resourceIndex];
            const auto& lifetime = lifetimes[resourceIndex];
            if (resource.type != ResourceType::TransientTexture || !lifetime.used) {
                continue;
            }
            if (orderIndex < lifetime.firstOrderIndex || orderIndex > lifetime.lastOrderIndex) {
                continue;
            }
            ++aliveCount;
            aliveBytes += static_cast<u64>(resource.textureDesc.width) *
                static_cast<u64>(resource.textureDesc.height) *
                estimate_format_bytes(resource.textureDesc.format);
        }

        stats.peakAliveTransientResources =
            std::max(stats.peakAliveTransientResources, aliveCount);
        stats.peakEstimatedTransientBytes =
            std::max(stats.peakEstimatedTransientBytes, aliveBytes);
    }

    return Result<RenderGraphMemoryStats>::ok(stats);
}

auto RenderGraph::allocation_stats() -> Result<RenderGraphAllocationStats> {
    auto lifetimeResult = resource_lifetimes();
    if (lifetimeResult.is_error()) {
        return Result<RenderGraphAllocationStats>::error(lifetimeResult.error());
    }
    const auto& lifetimes = lifetimeResult.value();
    const auto aliasPlan = build_alias_plan(lifetimes);

    RenderGraphAllocationStats stats{};
    stats.usedTransientResources = aliasPlan.usedTransientResources;
    stats.allocatedTransientTextures = static_cast<u32>(aliasPlan.slots.size());
    if (stats.usedTransientResources > stats.allocatedTransientTextures) {
        stats.aliasedTransientResources =
            stats.usedTransientResources - stats.allocatedTransientTextures;
    }

    for (const auto& slot : aliasPlan.slots) {
        stats.allocatedEstimatedTransientBytes +=
            static_cast<u64>(slot.desc.width) *
            static_cast<u64>(slot.desc.height) *
            estimate_format_bytes(slot.desc.format);
    }

    return Result<RenderGraphAllocationStats>::ok(stats);
}

auto RenderGraph::binding_cache_stats() const -> RenderGraphBindingCacheStats {
    return m_bindingCacheStats;
}

auto RenderGraph::debug_dump_text() -> Result<String> {
    if (!m_compiled) {
        auto compileResult = compile();
        if (compileResult.is_error()) {
            return Result<String>::error(compileResult.error());
        }
    }

    auto lifetimeResult = resource_lifetimes();
    if (lifetimeResult.is_error()) {
        return Result<String>::error(lifetimeResult.error());
    }
    const auto& lifetimes = lifetimeResult.value();

    auto memoryResult = memory_stats();
    if (memoryResult.is_error()) {
        return Result<String>::error(memoryResult.error());
    }
    const auto& memory = memoryResult.value();

    auto allocationResult = allocation_stats();
    if (allocationResult.is_error()) {
        return Result<String>::error(allocationResult.error());
    }
    const auto& allocation = allocationResult.value();

    const auto aliasPlan = build_alias_plan(lifetimes);

    std::ostringstream ss;
    ss << "RenderGraph Debug Report\n";
    ss << "resources=" << m_resources.size()
       << ", passes=" << m_passes.size()
       << ", compiledOrder=" << m_compiledOrder.size() << "\n";
    ss << "peakAliveTransientResources=" << memory.peakAliveTransientResources
       << ", peakEstimatedTransientBytes=" << memory.peakEstimatedTransientBytes << "\n";
    ss << "usedTransientResources=" << allocation.usedTransientResources
       << ", allocatedTransientTextures=" << allocation.allocatedTransientTextures
       << ", aliasedTransientResources=" << allocation.aliasedTransientResources
       << ", allocatedEstimatedTransientBytes=" << allocation.allocatedEstimatedTransientBytes << "\n";
    ss << "bindingCache.layoutCreates=" << m_bindingCacheStats.layoutCreates
       << ", bindingCache.bindGroupCreates=" << m_bindingCacheStats.bindGroupCreates
       << ", bindingCache.bindGroupReuses=" << m_bindingCacheStats.bindGroupReuses << "\n";
    ss << "bindingCache.staticCreates=" << m_bindingCacheStats.staticBindGroupCreates
       << ", bindingCache.staticReuses=" << m_bindingCacheStats.staticBindGroupReuses
       << ", bindingCache.dynamicCreates=" << m_bindingCacheStats.dynamicBindGroupCreates
       << ", bindingCache.dynamicReuses=" << m_bindingCacheStats.dynamicBindGroupReuses << "\n";

    ss << "PassOrder:\n";
    for (u32 orderIndex = 0; orderIndex < static_cast<u32>(m_compiledOrder.size()); ++orderIndex) {
        const auto passIndex = m_compiledOrder[orderIndex];
        const auto& pass = m_passes[passIndex];
        const auto bindingCount = pass.bindings.size() +
            pass.bufferBindings.size() +
            pass.samplerBindings.size();
        ss << "  [" << orderIndex << "] #" << passIndex << " "
           << (pass.type == PassType::Raster ? "Raster" : "Compute")
           << " name=\"" << pass.name << "\""
           << " reads=" << pass.reads.size()
           << " writes=" << pass.writes.size()
           << " bindings=" << bindingCount
           << "\n";
    }

    ss << "ResourceLifetimes:\n";
    for (u32 resourceIndex = 0; resourceIndex < static_cast<u32>(m_resources.size()); ++resourceIndex) {
        const auto& resource = m_resources[resourceIndex];
        const auto& life = lifetimes[resourceIndex];
        ss << "  #" << resourceIndex
           << " name=\"" << resource.name << "\""
           << " type=" << (resource.type == ResourceType::ImportedBackbuffer ? "Imported" : "Transient")
           << " used=" << (life.used ? "true" : "false");
        if (life.used) {
            ss << " firstPass=" << life.firstPass
               << " lastPass=" << life.lastPass
               << " firstOrder=" << life.firstOrderIndex
               << " lastOrder=" << life.lastOrderIndex;
        }
        ss << "\n";
    }

    ss << "AliasSlots:\n";
    for (u32 slotIndex = 0; slotIndex < static_cast<u32>(aliasPlan.slots.size()); ++slotIndex) {
        const auto& slot = aliasPlan.slots[slotIndex];
        ss << "  slot#" << slotIndex
           << " size=" << slot.desc.width << "x" << slot.desc.height
           << " resources=[";
        for (u32 i = 0; i < static_cast<u32>(slot.resources.size()); ++i) {
            if (i > 0) ss << ",";
            ss << slot.resources[i];
        }
        ss << "]\n";
    }

    return Result<String>::ok(ss.str());
}

void RenderGraph::clear() {
    for (auto& pass : m_passes) {
        release_pass_binding_cache(pass);
    }
    m_resources.clear();
    m_passes.clear();
    m_compiledOrder.clear();
    m_bindingCacheStats = {};
    m_compiled = false;
    m_hasBackbuffer = false;
}

auto RenderGraph::pass_count() const -> u32 {
    return static_cast<u32>(m_passes.size());
}

auto RenderGraph::resource_count() const -> u32 {
    return static_cast<u32>(m_resources.size());
}

auto RenderGraph::compiled_order() const -> const std::vector<u32>& {
    return m_compiledOrder;
}

auto RenderGraph::is_valid_resource(RenderGraphResourceHandle handle) const -> bool {
    return handle.valid() && handle.id < m_resources.size();
}

auto RenderGraph::is_valid_pass(RenderGraphPassHandle handle) const -> bool {
    return handle.valid() && handle.id < m_passes.size();
}

auto RenderGraph::build_alias_plan(const std::vector<RenderGraphResourceLifetime>& lifetimes) const
    -> AliasPlan {
    AliasPlan plan;
    plan.slotByResource.assign(m_resources.size(), -1);
    std::vector<u32> orderedResources;
    orderedResources.reserve(m_resources.size());

    for (u32 resourceIndex = 0; resourceIndex < static_cast<u32>(m_resources.size()); ++resourceIndex) {
        const auto& resource = m_resources[resourceIndex];
        const auto& lifetime = lifetimes[resourceIndex];
        if (resource.type != ResourceType::TransientTexture || !lifetime.used) {
            continue;
        }
        orderedResources.push_back(resourceIndex);
    }

    std::sort(orderedResources.begin(), orderedResources.end(), [&](u32 lhs, u32 rhs) {
        const auto& left = lifetimes[lhs];
        const auto& right = lifetimes[rhs];
        if (left.firstOrderIndex != right.firstOrderIndex) {
            return left.firstOrderIndex < right.firstOrderIndex;
        }
        if (left.lastOrderIndex != right.lastOrderIndex) {
            return left.lastOrderIndex < right.lastOrderIndex;
        }
        return lhs < rhs;
    });

    struct WorkingSlot {
        u32 slotIndex = 0;
        u32 lastOrderIndex = 0;
        TextureDesc desc{};
    };
    std::vector<WorkingSlot> workingSlots;
    workingSlots.reserve(orderedResources.size());

    for (const auto resourceIndex : orderedResources) {
        ++plan.usedTransientResources;
        const auto& lifetime = lifetimes[resourceIndex];
        const auto& desc = m_resources[resourceIndex].textureDesc;

        i32 bestSlot = -1;
        u32 bestLastOrder = 0;
        for (const auto& slot : workingSlots) {
            if (!are_alias_compatible(slot.desc, desc)) {
                continue;
            }
            if (slot.lastOrderIndex >= lifetime.firstOrderIndex) {
                continue;
            }
            if (bestSlot < 0 || slot.lastOrderIndex > bestLastOrder) {
                bestSlot = static_cast<i32>(slot.slotIndex);
                bestLastOrder = slot.lastOrderIndex;
            }
        }

        if (bestSlot < 0) {
            AliasSlot slotNode;
            slotNode.desc = desc;
            slotNode.resources.push_back(resourceIndex);
            plan.slots.push_back(std::move(slotNode));
            const auto newSlotIndex = static_cast<u32>(plan.slots.size() - 1);
            workingSlots.push_back(WorkingSlot{
                newSlotIndex,
                lifetime.lastOrderIndex,
                desc,
            });
            plan.slotByResource[resourceIndex] = static_cast<i32>(newSlotIndex);
        } else {
            const auto slotIndex = static_cast<u32>(bestSlot);
            plan.slots[slotIndex].resources.push_back(resourceIndex);
            plan.slotByResource[resourceIndex] = bestSlot;
            for (auto& slot : workingSlots) {
                if (slot.slotIndex == slotIndex) {
                    slot.lastOrderIndex = lifetime.lastOrderIndex;
                    break;
                }
            }
        }
    }

    return plan;
}

} // namespace firefly
