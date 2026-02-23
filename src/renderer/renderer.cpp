module;
#include <memory>
#include <glm/glm.hpp>

module firefly.renderer.renderer;

namespace firefly {

Renderer::Renderer() = default;
Renderer::~Renderer() { shutdown(); }

auto Renderer::initialize(void* nativeWindow, const RendererConfig& config) -> Result<void> {
    m_device = std::make_unique<RenderDevice>();
    DeviceConfig devConfig;
    devConfig.enableValidation = config.enableValidation;
    devConfig.enableVSync = config.enableVSync;
    devConfig.maxFramesInFlight = config.maxFramesInFlight;
    return m_device->initialize(nativeWindow, devConfig);
}

void Renderer::shutdown() {
    if (m_device) {
        m_device->shutdown();
        m_device.reset();
    }
}

void Renderer::begin_frame() {
    if (m_device) m_device->begin_frame();
}

void Renderer::end_frame() {
    if (m_device) m_device->end_frame();
}

void Renderer::present() {
    if (m_device) m_device->present();
}

void Renderer::set_camera(const Camera& camera) {
    m_viewProjection = camera.view_projection();
}

void Renderer::set_clear_color(f32 r, f32 g, f32 b, f32 a) {
    m_clearColor = glm::vec4(r, g, b, a);
}

} // namespace firefly
