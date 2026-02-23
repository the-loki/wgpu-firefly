module;
#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <glm/glm.hpp>

export module firefly.renderer.renderer;

import firefly.core.types;
import firefly.renderer.device;
import firefly.renderer.pipeline;
import firefly.renderer.command;
import firefly.renderer.material;
import firefly.renderer.mesh;
import firefly.renderer.camera;
import firefly.renderer.buffer;
import firefly.renderer.texture;
import firefly.renderer.shader;
import firefly.ecs.components;

export namespace firefly {

struct RendererConfig {
    bool enableValidation = true;
    bool enableVSync = true;
    u32 maxFramesInFlight = 2;
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    auto initialize(void* nativeWindow, const RendererConfig& config = {}) -> Result<void>;
    void shutdown();

    void begin_frame();
    void end_frame();
    void present();

    void set_camera(const Camera& camera);
    void set_clear_color(f32 r, f32 g, f32 b, f32 a = 1.0f);

    auto device() -> RenderDevice& { return *m_device; }

private:
    Ptr<RenderDevice> m_device;
    glm::vec4 m_clearColor{0.1f, 0.1f, 0.1f, 1.0f};
    glm::mat4 m_viewProjection{1.0f};
};

} // namespace firefly
