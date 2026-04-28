module;
#include <cstdint>
#include <string>
#include <memory>

export module firefly.renderer.device;

import firefly.core.types;
import firefly.renderer.buffer;
import firefly.renderer.texture;
import firefly.renderer.sampler;
import firefly.renderer.shader;
import firefly.renderer.pipeline;
import firefly.renderer.command;

export namespace firefly {

struct DeviceConfig {
    bool enableValidation = true;
    bool enableVSync = true;
    u32 maxFramesInFlight = 2;
};

class RenderDevice {
public:
    RenderDevice();
    ~RenderDevice();

    RenderDevice(const RenderDevice&) = delete;
    RenderDevice& operator=(const RenderDevice&) = delete;

    auto initialize(void* nativeWindow, const DeviceConfig& config = {}) -> Result<void>;
    void shutdown();

    auto create_buffer(const BufferDesc& desc) -> Ptr<Buffer>;
    auto create_texture(const TextureDesc& desc) -> Ptr<Texture>;
    auto create_sampler(const SamplerDesc& desc) -> Ptr<Sampler>;
    auto create_shader(const ShaderDesc& desc) -> Ptr<Shader>;
    auto create_pipeline(const PipelineDesc& desc) -> Ptr<RenderPipeline>;
    auto create_compute_pipeline(const ComputePipelineDesc& desc) -> Ptr<ComputePipeline>;

    auto begin_frame() -> CommandBuffer*;
    void end_frame();
    void present();
    auto request_screenshot(const String& path) -> Result<void>;
    [[nodiscard]] auto has_pending_screenshot() const -> bool;
    auto take_last_screenshot_error() -> String;

    void configure_surface(u32 width, u32 height);

    auto wgpu_device() const -> void* { return m_device; }
    auto wgpu_surface() const -> void* { return m_surface; }
    auto wgpu_queue() const -> void* { return m_queue; }

    auto surface_format() const -> TextureFormat { return m_surfaceFormat; }
    auto current_texture_view() const -> void* { return m_currentTextureView; }
    auto surface_width() const -> u32 { return m_width; }
    auto surface_height() const -> u32 { return m_height; }

private:
    void* m_instance = nullptr;
    void* m_adapter = nullptr;
    void* m_device = nullptr;
    void* m_queue = nullptr;
    void* m_surface = nullptr;
    void* m_currentTexture = nullptr;
    void* m_currentTextureView = nullptr;
    TextureFormat m_surfaceFormat = TextureFormat::BGRA8Unorm;
    Ptr<CommandBuffer> m_commandBuffer;
    u32 m_width = 0;
    u32 m_height = 0;
    bool m_initialized = false;
    String m_pendingScreenshotPath;
    String m_lastScreenshotError;
};

} // namespace firefly
