module;
#define WGPU_SHARED_LIBRARY
#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>
#include <string>
#include <memory>
#include <cstring>

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
    if (m_queue) { wgpuQueueRelease(static_cast<WGPUQueue>(m_queue)); m_queue = nullptr; }
    if (m_device) { wgpuDeviceRelease(static_cast<WGPUDevice>(m_device)); m_device = nullptr; }
    if (m_adapter) { wgpuAdapterRelease(static_cast<WGPUAdapter>(m_adapter)); m_adapter = nullptr; }
    if (m_surface) { wgpuSurfaceRelease(static_cast<WGPUSurface>(m_surface)); m_surface = nullptr; }
    if (m_instance) { wgpuInstanceRelease(static_cast<WGPUInstance>(m_instance)); m_instance = nullptr; }
    m_initialized = false;
}

void RenderDevice::configure_surface(u32 width, u32 height) {
    m_width = width;
    m_height = height;
    WGPUSurfaceConfiguration surfConfig{};
    surfConfig.device = static_cast<WGPUDevice>(m_device);
    surfConfig.format = static_cast<WGPUTextureFormat>(m_surfaceFormat);
    surfConfig.usage = WGPUTextureUsage_RenderAttachment;
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
    texDesc.format = static_cast<WGPUTextureFormat>(desc.format);
    texDesc.usage = static_cast<WGPUTextureUsage>(desc.usage);
    texDesc.mipLevelCount = 1;
    texDesc.sampleCount = 1;
    texDesc.dimension = WGPUTextureDimension_2D;
    tex->m_texture = wgpuDeviceCreateTexture(static_cast<WGPUDevice>(m_device), &texDesc);
    tex->m_width = desc.width;
    tex->m_height = desc.height;
    tex->m_format = desc.format;

    WGPUTextureViewDescriptor viewDesc{};
    viewDesc.format = static_cast<WGPUTextureFormat>(desc.format);
    viewDesc.dimension = WGPUTextureViewDimension_2D;
    viewDesc.mipLevelCount = 1;
    viewDesc.arrayLayerCount = 1;
    tex->m_view = wgpuTextureCreateView(static_cast<WGPUTexture>(tex->m_texture), &viewDesc);
    return tex;
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
    pipeDesc.vertex = vertexState;

    WGPUColorTargetState colorTarget{};
    colorTarget.format = static_cast<WGPUTextureFormat>(desc.colorFormat);
    colorTarget.writeMask = WGPUColorWriteMask_All;

    WGPUBlendState blend{};
    blend.color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blend.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blend.color.operation = WGPUBlendOperation_Add;
    blend.alpha.srcFactor = WGPUBlendFactor_One;
    blend.alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blend.alpha.operation = WGPUBlendOperation_Add;
    colorTarget.blend = &blend;

    WGPUFragmentState fragState{};
    fragState.module = static_cast<WGPUShaderModule>(
        desc.fragmentShader ? desc.fragmentShader->native_handle() :
        (desc.vertexShader ? desc.vertexShader->native_handle() : nullptr));
    fragState.entryPoint = toSV("fs_main");
    fragState.targetCount = 1;
    fragState.targets = &colorTarget;
    pipeDesc.fragment = &fragState;

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
        depthStencil.format = static_cast<WGPUTextureFormat>(desc.depthFormat);
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

auto RenderDevice::begin_frame() -> CommandBuffer* {
    if (!m_initialized) return nullptr;

    WGPUSurfaceTexture surfTex{};
    wgpuSurfaceGetCurrentTexture(static_cast<WGPUSurface>(m_surface), &surfTex);
    if (surfTex.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
        surfTex.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
        return nullptr;
    }

    m_currentTextureView = wgpuTextureCreateView(surfTex.texture, nullptr);

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
    auto cmdBuf = m_commandBuffer->finish();
    wgpuQueueSubmit(static_cast<WGPUQueue>(m_queue), 1,
                    reinterpret_cast<WGPUCommandBuffer*>(&cmdBuf));
    wgpuCommandBufferRelease(static_cast<WGPUCommandBuffer>(cmdBuf));
    m_commandBuffer.reset();

    if (m_currentTextureView) {
        wgpuTextureViewRelease(static_cast<WGPUTextureView>(m_currentTextureView));
        m_currentTextureView = nullptr;
    }
}

void RenderDevice::present() {
    if (m_surface) {
        wgpuSurfacePresent(static_cast<WGPUSurface>(m_surface));
    }
}

} // namespace firefly
