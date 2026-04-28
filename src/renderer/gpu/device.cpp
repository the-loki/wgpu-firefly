module;
#define WGPU_SHARED_LIBRARY
#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>
#include <string>
#include <memory>
#include <cstring>
#include <vector>
#include <fstream>
#include <filesystem>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

module firefly.renderer.device;

namespace firefly {

static WGPUStringView toSV(const char* s) {
    return {s, s ? strlen(s) : 0};
}

static WGPUStringView toSV(const String& s) {
    return {s.c_str(), s.size()};
}

static auto to_wgpu_texture_format(TextureFormat format) -> WGPUTextureFormat {
    switch (format) {
        case TextureFormat::RGBA8Unorm:
            return WGPUTextureFormat_RGBA8Unorm;
        case TextureFormat::RGBA16Float:
            return WGPUTextureFormat_RGBA16Float;
        case TextureFormat::BGRA8Unorm:
            return WGPUTextureFormat_BGRA8Unorm;
        case TextureFormat::Depth24PlusStencil8:
            return WGPUTextureFormat_Depth24PlusStencil8;
        case TextureFormat::Depth32Float:
            return WGPUTextureFormat_Depth32Float;
        default:
            return WGPUTextureFormat_BGRA8Unorm;
    }
}

static auto to_wgpu_texture_usage(TextureUsage usage) -> WGPUTextureUsage {
    u32 result = 0;
    if (has_usage(usage, TextureUsage::CopySrc)) {
        result |= WGPUTextureUsage_CopySrc;
    }
    if (has_usage(usage, TextureUsage::CopyDst)) {
        result |= WGPUTextureUsage_CopyDst;
    }
    if (has_usage(usage, TextureUsage::TextureBinding)) {
        result |= WGPUTextureUsage_TextureBinding;
    }
    if (has_usage(usage, TextureUsage::StorageBinding)) {
        result |= WGPUTextureUsage_StorageBinding;
    }
    if (has_usage(usage, TextureUsage::RenderAttachment)) {
        result |= WGPUTextureUsage_RenderAttachment;
    }
    return static_cast<WGPUTextureUsage>(result);
}

static auto to_wgpu_address_mode(AddressMode mode) -> WGPUAddressMode {
    switch (mode) {
        case AddressMode::ClampToEdge:
            return WGPUAddressMode_ClampToEdge;
        case AddressMode::Repeat:
            return WGPUAddressMode_Repeat;
        case AddressMode::MirrorRepeat:
            return WGPUAddressMode_MirrorRepeat;
        default:
            return WGPUAddressMode_ClampToEdge;
    }
}

static auto to_wgpu_filter_mode(FilterMode mode) -> WGPUFilterMode {
    switch (mode) {
        case FilterMode::Nearest:
            return WGPUFilterMode_Nearest;
        case FilterMode::Linear:
            return WGPUFilterMode_Linear;
        default:
            return WGPUFilterMode_Linear;
    }
}

static auto to_wgpu_mipmap_filter_mode(FilterMode mode) -> WGPUMipmapFilterMode {
    switch (mode) {
        case FilterMode::Nearest:
            return WGPUMipmapFilterMode_Nearest;
        case FilterMode::Linear:
            return WGPUMipmapFilterMode_Linear;
        default:
            return WGPUMipmapFilterMode_Nearest;
    }
}

static auto to_wgpu_compare_function(CompareFunction compare) -> WGPUCompareFunction {
    switch (compare) {
        case CompareFunction::Never:
            return WGPUCompareFunction_Never;
        case CompareFunction::Less:
            return WGPUCompareFunction_Less;
        case CompareFunction::LessEqual:
            return WGPUCompareFunction_LessEqual;
        case CompareFunction::Greater:
            return WGPUCompareFunction_Greater;
        case CompareFunction::GreaterEqual:
            return WGPUCompareFunction_GreaterEqual;
        case CompareFunction::Equal:
            return WGPUCompareFunction_Equal;
        case CompareFunction::NotEqual:
            return WGPUCompareFunction_NotEqual;
        case CompareFunction::Always:
            return WGPUCompareFunction_Always;
        default:
            return WGPUCompareFunction_LessEqual;
    }
}

static auto to_wgpu_vertex_format(VertexFormat format) -> WGPUVertexFormat {
    switch (format) {
        case VertexFormat::Float32:
            return WGPUVertexFormat_Float32;
        case VertexFormat::Float32x2:
            return WGPUVertexFormat_Float32x2;
        case VertexFormat::Float32x3:
            return WGPUVertexFormat_Float32x3;
        case VertexFormat::Float32x4:
            return WGPUVertexFormat_Float32x4;
        case VertexFormat::Uint32:
            return WGPUVertexFormat_Uint32;
        default:
            return WGPUVertexFormat_Float32x3;
    }
}

static auto to_wgpu_vertex_step_mode(VertexStepMode mode) -> WGPUVertexStepMode {
    switch (mode) {
        case VertexStepMode::Vertex:
            return WGPUVertexStepMode_Vertex;
        case VertexStepMode::Instance:
            return WGPUVertexStepMode_Instance;
        default:
            return WGPUVertexStepMode_Vertex;
    }
}

static auto align_to(u32 value, u32 alignment) -> u32 {
    if (alignment == 0) {
        return value;
    }
    const auto remainder = value % alignment;
    if (remainder == 0) {
        return value;
    }
    return value + (alignment - remainder);
}

static auto save_bmp_from_bgra(
    const String& path,
    u32 width,
    u32 height,
    const std::vector<u8>& bgraData) -> Result<void> {
    if (path.empty()) {
        return Result<void>::error("Screenshot path is empty");
    }
    if (width == 0 || height == 0) {
        return Result<void>::error("Screenshot size is invalid");
    }
    const u64 requiredBytes = static_cast<u64>(width) * static_cast<u64>(height) * 4u;
    if (bgraData.size() < requiredBytes) {
        return Result<void>::error("Screenshot data size is invalid");
    }

    const std::filesystem::path targetPath(path);
    if (targetPath.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(targetPath.parent_path(), ec);
        if (ec) {
            return Result<void>::error("Failed to create screenshot directory: " + ec.message());
        }
    }

    const u32 outputRowBytes = align_to(width * 3u, 4u);
    const u32 pixelDataBytes = outputRowBytes * height;
    const u32 headerBytes = 14u + 40u;
    const u32 fileSize = headerBytes + pixelDataBytes;

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return Result<void>::error("Failed to open screenshot file: " + path);
    }

    auto write_u16 = [&file](u16 value) {
        const char bytes[2] = {
            static_cast<char>(value & 0xFFu),
            static_cast<char>((value >> 8u) & 0xFFu),
        };
        file.write(bytes, 2);
    };
    auto write_u32 = [&file](u32 value) {
        const char bytes[4] = {
            static_cast<char>(value & 0xFFu),
            static_cast<char>((value >> 8u) & 0xFFu),
            static_cast<char>((value >> 16u) & 0xFFu),
            static_cast<char>((value >> 24u) & 0xFFu),
        };
        file.write(bytes, 4);
    };
    auto write_i32 = [&write_u32](i32 value) {
        write_u32(static_cast<u32>(value));
    };

    // BITMAPFILEHEADER
    write_u16(0x4D42u); // "BM"
    write_u32(fileSize);
    write_u16(0u);
    write_u16(0u);
    write_u32(headerBytes);

    // BITMAPINFOHEADER
    write_u32(40u);
    write_i32(static_cast<i32>(width));
    write_i32(static_cast<i32>(height)); // bottom-up
    write_u16(1u);
    write_u16(24u);
    write_u32(0u); // BI_RGB
    write_u32(pixelDataBytes);
    write_i32(2835); // 72 DPI
    write_i32(2835);
    write_u32(0u);
    write_u32(0u);

    std::vector<u8> row(outputRowBytes, 0u);
    for (u32 y = 0; y < height; ++y) {
        const u32 srcY = height - 1u - y;
        const auto* src = bgraData.data() + static_cast<size_t>(srcY) * width * 4u;
        for (u32 x = 0; x < width; ++x) {
            row[x * 3u + 0u] = src[x * 4u + 0u];
            row[x * 3u + 1u] = src[x * 4u + 1u];
            row[x * 3u + 2u] = src[x * 4u + 2u];
        }
        file.write(reinterpret_cast<const char*>(row.data()), outputRowBytes);
    }

    if (!file.good()) {
        return Result<void>::error("Failed while writing screenshot file: " + path);
    }
    return Result<void>::ok();
}

RenderDevice::RenderDevice() = default;

RenderDevice::~RenderDevice() {
    shutdown();
}

auto RenderDevice::initialize(void* nativeWindow, const DeviceConfig&) -> Result<void> {
    WGPUInstanceDescriptor instanceDesc{};
    m_instance = wgpuCreateInstance(&instanceDesc);
    if (!m_instance) {
        return Result<void>::error("Failed to create WGPU instance");
    }

    // Create surface
    WGPUSurfaceSourceWindowsHWND surfaceFromWin{};
    surfaceFromWin.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
    surfaceFromWin.hinstance = GetModuleHandle(nullptr);
    surfaceFromWin.hwnd = nativeWindow;

    WGPUSurfaceDescriptor surfaceDesc{};
    surfaceDesc.nextInChain = &surfaceFromWin.chain;
    m_surface = wgpuInstanceCreateSurface(
        static_cast<WGPUInstance>(m_instance), &surfaceDesc);
    if (!m_surface) {
        return Result<void>::error("Failed to create WGPU surface");
    }

    // Request adapter
    WGPURequestAdapterOptions adapterOpts{};
    adapterOpts.compatibleSurface = static_cast<WGPUSurface>(m_surface);
    adapterOpts.powerPreference = WGPUPowerPreference_HighPerformance;

    struct AdapterData { WGPUAdapter adapter = nullptr; bool done = false; };
    AdapterData adapterData;

    WGPURequestAdapterCallbackInfo adapterCbInfo{};
    adapterCbInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    adapterCbInfo.callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter,
                                WGPUStringView, void* ud1, void*) {
        auto* data = static_cast<AdapterData*>(ud1);
        if (status == WGPURequestAdapterStatus_Success) data->adapter = adapter;
        data->done = true;
    };
    adapterCbInfo.userdata1 = &adapterData;

    wgpuInstanceRequestAdapter(
        static_cast<WGPUInstance>(m_instance), &adapterOpts, adapterCbInfo);

    while (!adapterData.done) {
        wgpuInstanceProcessEvents(static_cast<WGPUInstance>(m_instance));
    }

    m_adapter = adapterData.adapter;
    if (!m_adapter) {
        return Result<void>::error("Failed to get WGPU adapter");
    }

    // Request device
    WGPUDeviceDescriptor deviceDesc{};
    deviceDesc.label = toSV("Firefly Device");

    struct DeviceData { WGPUDevice device = nullptr; bool done = false; };
    DeviceData deviceData;

    WGPURequestDeviceCallbackInfo deviceCbInfo{};
    deviceCbInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    deviceCbInfo.callback = [](WGPURequestDeviceStatus status, WGPUDevice device,
                               WGPUStringView, void* ud1, void*) {
        auto* data = static_cast<DeviceData*>(ud1);
        if (status == WGPURequestDeviceStatus_Success) data->device = device;
        data->done = true;
    };
    deviceCbInfo.userdata1 = &deviceData;

    wgpuAdapterRequestDevice(
        static_cast<WGPUAdapter>(m_adapter), &deviceDesc, deviceCbInfo);

    while (!deviceData.done) {
        wgpuInstanceProcessEvents(static_cast<WGPUInstance>(m_instance));
    }

    m_device = deviceData.device;
    if (!m_device) {
        return Result<void>::error("Failed to get WGPU device");
    }

    m_queue = wgpuDeviceGetQueue(static_cast<WGPUDevice>(m_device));
    m_surfaceFormat = TextureFormat::BGRA8Unorm;
    m_initialized = true;
    return Result<void>::ok();
}

void RenderDevice::shutdown() {
    if (m_currentTextureView) {
        wgpuTextureViewRelease(static_cast<WGPUTextureView>(m_currentTextureView));
        m_currentTextureView = nullptr;
    }
    if (m_currentTexture) {
        wgpuTextureRelease(static_cast<WGPUTexture>(m_currentTexture));
        m_currentTexture = nullptr;
    }
    if (m_queue) { wgpuQueueRelease(static_cast<WGPUQueue>(m_queue)); m_queue = nullptr; }
    if (m_device) { wgpuDeviceRelease(static_cast<WGPUDevice>(m_device)); m_device = nullptr; }
    if (m_adapter) { wgpuAdapterRelease(static_cast<WGPUAdapter>(m_adapter)); m_adapter = nullptr; }
    if (m_surface) { wgpuSurfaceRelease(static_cast<WGPUSurface>(m_surface)); m_surface = nullptr; }
    if (m_instance) { wgpuInstanceRelease(static_cast<WGPUInstance>(m_instance)); m_instance = nullptr; }
    m_initialized = false;
    m_pendingScreenshotPath.clear();
    m_lastScreenshotError.clear();
}

void RenderDevice::configure_surface(u32 width, u32 height) {
    m_width = width;
    m_height = height;
    WGPUSurfaceConfiguration surfConfig{};
    surfConfig.device = static_cast<WGPUDevice>(m_device);
    surfConfig.format = to_wgpu_texture_format(m_surfaceFormat);
    surfConfig.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
    surfConfig.width = width;
    surfConfig.height = height;
    surfConfig.presentMode = WGPUPresentMode_Fifo;
    surfConfig.alphaMode = WGPUCompositeAlphaMode_Auto;
    wgpuSurfaceConfigure(static_cast<WGPUSurface>(m_surface), &surfConfig);
}

auto RenderDevice::create_buffer(const BufferDesc& desc) -> Ptr<Buffer> {
    auto buf = std::make_unique<Buffer>();
    WGPUBufferDescriptor bufDesc{};
    bufDesc.label = toSV(desc.label);
    bufDesc.size = desc.size;
    bufDesc.usage = static_cast<WGPUBufferUsage>(desc.usage) | WGPUBufferUsage_CopyDst;
    bufDesc.mappedAtCreation = desc.mappedAtCreation;
    buf->m_buffer = wgpuDeviceCreateBuffer(static_cast<WGPUDevice>(m_device), &bufDesc);
    buf->m_device = m_device;
    buf->m_queue = m_queue;
    buf->m_size = desc.size;
    buf->m_usage = desc.usage;
    return buf;
}

auto RenderDevice::create_texture(const TextureDesc& desc) -> Ptr<Texture> {
    auto tex = std::make_unique<Texture>();
    WGPUTextureDescriptor texDesc{};
    texDesc.label = toSV(desc.label);
    texDesc.size = {desc.width, desc.height, 1};
    texDesc.format = to_wgpu_texture_format(desc.format);
    texDesc.usage = to_wgpu_texture_usage(desc.usage);
    texDesc.mipLevelCount = 1;
    texDesc.sampleCount = 1;
    texDesc.dimension = WGPUTextureDimension_2D;
    tex->m_texture = wgpuDeviceCreateTexture(static_cast<WGPUDevice>(m_device), &texDesc);
    tex->m_width = desc.width;
    tex->m_height = desc.height;
    tex->m_format = desc.format;

    WGPUTextureViewDescriptor viewDesc{};
    viewDesc.format = to_wgpu_texture_format(desc.format);
    viewDesc.dimension = WGPUTextureViewDimension_2D;
    viewDesc.mipLevelCount = 1;
    viewDesc.arrayLayerCount = 1;
    tex->m_view = wgpuTextureCreateView(static_cast<WGPUTexture>(tex->m_texture), &viewDesc);
    return tex;
}

auto RenderDevice::create_sampler(const SamplerDesc& desc) -> Ptr<Sampler> {
    auto sampler = std::make_unique<Sampler>();
    WGPUSamplerDescriptor samplerDesc{};
    samplerDesc.addressModeU = to_wgpu_address_mode(desc.addressModeU);
    samplerDesc.addressModeV = to_wgpu_address_mode(desc.addressModeV);
    samplerDesc.addressModeW = to_wgpu_address_mode(desc.addressModeW);
    samplerDesc.magFilter = to_wgpu_filter_mode(desc.magFilter);
    samplerDesc.minFilter = to_wgpu_filter_mode(desc.minFilter);
    samplerDesc.mipmapFilter = to_wgpu_mipmap_filter_mode(desc.mipmapFilter);
    samplerDesc.compare = desc.enableCompare
        ? to_wgpu_compare_function(desc.compareFunction)
        : WGPUCompareFunction_Undefined;
    samplerDesc.maxAnisotropy = 1;
    sampler->m_sampler = wgpuDeviceCreateSampler(
        static_cast<WGPUDevice>(m_device), &samplerDesc);
    return sampler;
}

auto RenderDevice::create_shader(const ShaderDesc& desc) -> Ptr<Shader> {
    auto shader = std::make_unique<Shader>();
    WGPUShaderSourceWGSL wgslDesc{};
    wgslDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgslDesc.code = toSV(desc.wgslCode);
    WGPUShaderModuleDescriptor moduleDesc{};
    moduleDesc.label = toSV(desc.label);
    moduleDesc.nextInChain = &wgslDesc.chain;
    shader->m_module = wgpuDeviceCreateShaderModule(
        static_cast<WGPUDevice>(m_device), &moduleDesc);
    return shader;
}

auto RenderDevice::create_pipeline(const PipelineDesc& desc) -> Ptr<RenderPipeline> {
    auto pipeline = std::make_unique<RenderPipeline>();

    WGPURenderPipelineDescriptor pipeDesc{};
    pipeDesc.label = toSV(desc.label);

    WGPUVertexState vertexState{};
    vertexState.module = static_cast<WGPUShaderModule>(
        desc.vertexShader ? desc.vertexShader->native_handle() : nullptr);
    vertexState.entryPoint = toSV("vs_main");

    std::vector<std::vector<WGPUVertexAttribute>> vertexAttributeStorage;
    std::vector<WGPUVertexBufferLayout> vertexBufferLayouts;
    if (!desc.vertexBuffers.empty()) {
        vertexAttributeStorage.resize(desc.vertexBuffers.size());
        vertexBufferLayouts.resize(desc.vertexBuffers.size());

        for (u32 bufferIndex = 0; bufferIndex < static_cast<u32>(desc.vertexBuffers.size()); ++bufferIndex) {
            const auto& srcLayout = desc.vertexBuffers[bufferIndex];
            auto& dstAttributes = vertexAttributeStorage[bufferIndex];
            dstAttributes.resize(srcLayout.attributes.size());

            for (u32 attrIndex = 0; attrIndex < static_cast<u32>(srcLayout.attributes.size()); ++attrIndex) {
                const auto& srcAttribute = srcLayout.attributes[attrIndex];
                auto& dstAttribute = dstAttributes[attrIndex];
                dstAttribute.shaderLocation = srcAttribute.location;
                dstAttribute.offset = srcAttribute.offset;
                dstAttribute.format = to_wgpu_vertex_format(srcAttribute.format);
            }

            auto& dstLayout = vertexBufferLayouts[bufferIndex];
            dstLayout.arrayStride = srcLayout.arrayStride;
            dstLayout.stepMode = to_wgpu_vertex_step_mode(srcLayout.stepMode);
            dstLayout.attributeCount = static_cast<u32>(dstAttributes.size());
            dstLayout.attributes = dstAttributes.data();
        }

        vertexState.bufferCount = static_cast<u32>(vertexBufferLayouts.size());
        vertexState.buffers = vertexBufferLayouts.data();
    }
    pipeDesc.vertex = vertexState;

    WGPUColorTargetState colorTarget{};
    WGPUBlendState blend{};
    WGPUFragmentState fragState{};
    if (desc.enableFragment) {
        colorTarget.format = to_wgpu_texture_format(desc.colorFormat);
        colorTarget.writeMask = WGPUColorWriteMask_All;

        blend.color.srcFactor = WGPUBlendFactor_SrcAlpha;
        blend.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
        blend.color.operation = WGPUBlendOperation_Add;
        blend.alpha.srcFactor = WGPUBlendFactor_One;
        blend.alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
        blend.alpha.operation = WGPUBlendOperation_Add;
        colorTarget.blend = &blend;

        fragState.module = static_cast<WGPUShaderModule>(
            desc.fragmentShader ? desc.fragmentShader->native_handle() :
            (desc.vertexShader ? desc.vertexShader->native_handle() : nullptr));
        fragState.entryPoint = toSV("fs_main");
        fragState.targetCount = 1;
        fragState.targets = &colorTarget;
        pipeDesc.fragment = &fragState;
    } else {
        pipeDesc.fragment = nullptr;
    }

    // Map our topology enum to WGPU (TriangleList=0 -> WGPUPrimitiveTopology_TriangleList=4)
    static const WGPUPrimitiveTopology topoMap[] = {
        WGPUPrimitiveTopology_TriangleList,
        WGPUPrimitiveTopology_TriangleStrip,
        WGPUPrimitiveTopology_LineList,
        WGPUPrimitiveTopology_PointList,
    };
    pipeDesc.primitive.topology = topoMap[static_cast<u32>(desc.topology)];
    pipeDesc.primitive.cullMode = static_cast<WGPUCullMode>(desc.cullMode);

    WGPUDepthStencilState depthStencil{};
    if (desc.depthTest) {
        depthStencil.format = to_wgpu_texture_format(desc.depthFormat);
        depthStencil.depthWriteEnabled = WGPUOptionalBool_True;
        depthStencil.depthCompare = WGPUCompareFunction_Less;
        pipeDesc.depthStencil = &depthStencil;
    }

    pipeDesc.multisample.count = 1;
    pipeDesc.multisample.mask = 0xFFFFFFFF;

    pipeline->m_pipeline = wgpuDeviceCreateRenderPipeline(
        static_cast<WGPUDevice>(m_device), &pipeDesc);
    return pipeline;
}

auto RenderDevice::create_compute_pipeline(const ComputePipelineDesc& desc) -> Ptr<ComputePipeline> {
    auto pipeline = std::make_unique<ComputePipeline>();

    WGPUComputePipelineDescriptor pipeDesc{};
    pipeDesc.label = toSV(desc.label);
    pipeDesc.compute.module = static_cast<WGPUShaderModule>(
        desc.computeShader ? desc.computeShader->native_handle() : nullptr);
    pipeDesc.compute.entryPoint = toSV(desc.entryPoint);
    pipeline->m_pipeline = wgpuDeviceCreateComputePipeline(
        static_cast<WGPUDevice>(m_device), &pipeDesc);
    return pipeline;
}

auto RenderDevice::begin_frame() -> CommandBuffer* {
    if (!m_initialized) return nullptr;

    if (m_currentTextureView) {
        wgpuTextureViewRelease(static_cast<WGPUTextureView>(m_currentTextureView));
        m_currentTextureView = nullptr;
    }
    if (m_currentTexture) {
        wgpuTextureRelease(static_cast<WGPUTexture>(m_currentTexture));
        m_currentTexture = nullptr;
    }

    WGPUSurfaceTexture surfTex{};
    wgpuSurfaceGetCurrentTexture(static_cast<WGPUSurface>(m_surface), &surfTex);
    if (surfTex.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
        surfTex.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
        if (surfTex.texture) {
            wgpuTextureRelease(surfTex.texture);
        }
        return nullptr;
    }

    m_currentTexture = surfTex.texture;
    m_currentTextureView = wgpuTextureCreateView(static_cast<WGPUTexture>(m_currentTexture), nullptr);

    m_commandBuffer = std::make_unique<CommandBuffer>();
    WGPUCommandEncoderDescriptor encDesc{};
    encDesc.label = toSV("Frame Encoder");
    m_commandBuffer->m_encoder = wgpuDeviceCreateCommandEncoder(
        static_cast<WGPUDevice>(m_device), &encDesc);
    m_commandBuffer->m_device = m_device;
    return m_commandBuffer.get();
}

void RenderDevice::end_frame() {
    if (!m_commandBuffer) return;
    auto cmdBuf = static_cast<WGPUCommandBuffer>(m_commandBuffer->finish());
    wgpuQueueSubmit(static_cast<WGPUQueue>(m_queue), 1, &cmdBuf);
    wgpuCommandBufferRelease(cmdBuf);

    if (!m_pendingScreenshotPath.empty()) {
        if (!m_currentTexture || !m_device || !m_instance) {
            m_lastScreenshotError = "Screenshot failed: current frame is unavailable";
            m_pendingScreenshotPath.clear();
        } else {
            const u32 width = m_width;
            const u32 height = m_height;
            if (width == 0 || height == 0) {
                m_lastScreenshotError = "Screenshot failed: surface size is zero";
                m_pendingScreenshotPath.clear();
            } else {
                const u32 bytesPerPixel = 4u;
                const u32 unpaddedRowBytes = width * bytesPerPixel;
                const u32 paddedRowBytes = align_to(unpaddedRowBytes, 256u);
                const u64 bufferSize = static_cast<u64>(paddedRowBytes) * static_cast<u64>(height);

                WGPUBufferDescriptor readbackDesc{};
                readbackDesc.label = toSV("ScreenshotReadbackBuffer");
                readbackDesc.size = bufferSize;
                readbackDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
                auto readbackBuffer = wgpuDeviceCreateBuffer(
                    static_cast<WGPUDevice>(m_device), &readbackDesc);
                if (!readbackBuffer) {
                    m_lastScreenshotError = "Screenshot failed: cannot create readback buffer";
                    m_pendingScreenshotPath.clear();
                } else {
                    WGPUCommandEncoderDescriptor encoderDesc{};
                    encoderDesc.label = toSV("ScreenshotEncoder");
                    auto screenshotEncoder = wgpuDeviceCreateCommandEncoder(
                        static_cast<WGPUDevice>(m_device), &encoderDesc);
                    if (!screenshotEncoder) {
                        m_lastScreenshotError = "Screenshot failed: cannot create command encoder";
                        wgpuBufferRelease(readbackBuffer);
                        m_pendingScreenshotPath.clear();
                        m_commandBuffer.reset();
                        return;
                    }

                    WGPUTexelCopyTextureInfo source{};
                    source.texture = static_cast<WGPUTexture>(m_currentTexture);
                    source.mipLevel = 0;
                    source.origin = {0, 0, 0};
                    source.aspect = WGPUTextureAspect_All;

                    WGPUTexelCopyBufferInfo destination{};
                    destination.buffer = readbackBuffer;
                    destination.layout.offset = 0;
                    destination.layout.bytesPerRow = paddedRowBytes;
                    destination.layout.rowsPerImage = height;

                    WGPUExtent3D copySize{};
                    copySize.width = width;
                    copySize.height = height;
                    copySize.depthOrArrayLayers = 1;

                    wgpuCommandEncoderCopyTextureToBuffer(
                        screenshotEncoder, &source, &destination, &copySize);
                    auto screenshotCmd = wgpuCommandEncoderFinish(screenshotEncoder, nullptr);
                    wgpuCommandEncoderRelease(screenshotEncoder);
                    if (!screenshotCmd) {
                        m_lastScreenshotError = "Screenshot failed: cannot finish command buffer";
                        wgpuBufferRelease(readbackBuffer);
                        m_pendingScreenshotPath.clear();
                        m_commandBuffer.reset();
                        return;
                    }
                    wgpuQueueSubmit(static_cast<WGPUQueue>(m_queue), 1, &screenshotCmd);
                    wgpuCommandBufferRelease(screenshotCmd);

                    struct MapState {
                        bool done = false;
                        WGPUMapAsyncStatus status = WGPUMapAsyncStatus_Unknown;
                        String message;
                    };
                    MapState mapState{};

                    WGPUBufferMapCallbackInfo mapCbInfo{};
                    mapCbInfo.mode = WGPUCallbackMode_AllowProcessEvents;
                    mapCbInfo.callback = [](WGPUMapAsyncStatus status, WGPUStringView message, void* ud1, void*) {
                        auto* state = static_cast<MapState*>(ud1);
                        state->status = status;
                        if (message.data && message.length > 0) {
                            state->message.assign(message.data, message.length);
                        }
                        state->done = true;
                    };
                    mapCbInfo.userdata1 = &mapState;
                    wgpuBufferMapAsync(readbackBuffer, WGPUMapMode_Read, 0, bufferSize, mapCbInfo);

                    constexpr int maxPollIterations = 20000;
                    int pollCount = 0;
                    while (!mapState.done && pollCount < maxPollIterations) {
                        wgpuDevicePoll(static_cast<WGPUDevice>(m_device), false, nullptr);
                        wgpuInstanceProcessEvents(static_cast<WGPUInstance>(m_instance));
                        ++pollCount;
                    }

                    if (!mapState.done) {
                        m_lastScreenshotError = "Screenshot failed: map callback timed out";
                    } else if (mapState.status != WGPUMapAsyncStatus_Success) {
                        m_lastScreenshotError = "Screenshot failed: map_async status=" +
                            std::to_string(static_cast<int>(mapState.status));
                        if (!mapState.message.empty()) {
                            m_lastScreenshotError += " message=" + mapState.message;
                        }
                    } else {
                        const auto* mapped = static_cast<const u8*>(
                            wgpuBufferGetConstMappedRange(readbackBuffer, 0, bufferSize));
                        if (!mapped) {
                            m_lastScreenshotError = "Screenshot failed: mapped range is null";
                        } else {
                            std::vector<u8> pixels(static_cast<size_t>(width) * height * bytesPerPixel);
                            for (u32 y = 0; y < height; ++y) {
                                const auto* srcRow = mapped + static_cast<size_t>(y) * paddedRowBytes;
                                auto* dstRow = pixels.data() + static_cast<size_t>(y) * unpaddedRowBytes;
                                std::memcpy(dstRow, srcRow, unpaddedRowBytes);
                            }
                            const auto saveResult = save_bmp_from_bgra(
                                m_pendingScreenshotPath, width, height, pixels);
                            if (saveResult.is_error()) {
                                m_lastScreenshotError = saveResult.error();
                            } else {
                                m_lastScreenshotError.clear();
                            }
                        }
                    }

                    if (wgpuBufferGetMapState(readbackBuffer) == WGPUBufferMapState_Mapped) {
                        wgpuBufferUnmap(readbackBuffer);
                    }
                    wgpuBufferRelease(readbackBuffer);
                    m_pendingScreenshotPath.clear();
                }
            }
        }
    }

    m_commandBuffer.reset();
}

void RenderDevice::present() {
    if (m_surface) {
        wgpuSurfacePresent(static_cast<WGPUSurface>(m_surface));
    }
    if (m_currentTextureView) {
        wgpuTextureViewRelease(static_cast<WGPUTextureView>(m_currentTextureView));
        m_currentTextureView = nullptr;
    }
    if (m_currentTexture) {
        wgpuTextureRelease(static_cast<WGPUTexture>(m_currentTexture));
        m_currentTexture = nullptr;
    }
}

auto RenderDevice::request_screenshot(const String& path) -> Result<void> {
    if (path.empty()) {
        return Result<void>::error("Screenshot path cannot be empty");
    }
    if (!m_initialized) {
        return Result<void>::error("RenderDevice is not initialized");
    }
    if (!m_pendingScreenshotPath.empty()) {
        return Result<void>::error("A screenshot request is already pending");
    }
    m_pendingScreenshotPath = path;
    return Result<void>::ok();
}

auto RenderDevice::has_pending_screenshot() const -> bool {
    return !m_pendingScreenshotPath.empty();
}

auto RenderDevice::take_last_screenshot_error() -> String {
    auto err = m_lastScreenshotError;
    m_lastScreenshotError.clear();
    return err;
}

} // namespace firefly
