module;
#include <array>
#include <glm/glm.hpp>
#include <vector>

export module firefly.renderer.forward_renderer;

import firefly.core.types;
import firefly.renderer.buffer;
import firefly.renderer.device;
import firefly.renderer.mesh;
import firefly.renderer.pipeline;
import firefly.renderer.render_graph;
import firefly.renderer.sampler;
import firefly.renderer.shader;
import firefly.renderer.texture;

export namespace firefly {

inline constexpr u32 ForwardMaxDirectionalLights = 2;
inline constexpr u32 ForwardMaxPointLights = 8;
inline constexpr u32 ForwardShadowCascadeCount = 2;

struct ForwardViewParams {
    glm::mat4 viewProjection{1.0f};
    glm::vec3 cameraWorldPosition{0.0f, 0.0f, 3.0f};
};

struct ForwardDirectionalLight {
    glm::vec3 direction{-0.4f, -1.0f, -0.3f};
    f32 intensity = 4.0f;
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    f32 ambientIntensity = 0.03f;
};

struct ForwardPointLight {
    glm::vec3 position{0.0f, 2.0f, 0.0f};
    f32 range = 6.0f;
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    f32 intensity = 15.0f;
};

struct ForwardMaterialParams {
    glm::vec4 baseColor{1.0f, 0.766f, 0.336f, 1.0f};
    glm::vec3 emissive{0.0f, 0.0f, 0.0f};
    f32 metallic = 1.0f;
    f32 roughness = 0.25f;
    f32 ao = 1.0f;
};

struct ForwardObjectParams {
    glm::mat4 model{1.0f};
    ForwardMaterialParams material{};
};

struct ForwardGridParams {
    bool enabled = false;
    u32 columns = 1;
    u32 rows = 1;
    f32 spacing = 2.3f;
};

struct ForwardShadowSettings {
    f32 constantBias = 0.0008f;
    f32 slopeBias = 0.0035f;
    bool enablePCF = true;
    bool enableCascades = true;
    f32 cascadeSplitDistance = 8.0f;
    f32 cascadeBlendDistance = 1.25f;
    u32 shadowMapResolution = 1024;
};

struct ForwardFrameTargets {
    RenderGraphResourceHandle outputColorTarget{};
    RenderGraphResourceHandle hdrColorTarget{};
    RenderGraphResourceHandle depthTarget{};
    RenderGraphResourceHandle shadowDepthNearTarget{};
    RenderGraphResourceHandle shadowDepthFarTarget{};
    String shadowNearPassName = "ForwardShadowNear";
    String shadowFarPassName = "ForwardShadowFar";
    String mainPassName = "ForwardMain";
    String tonemapPassName = "ForwardTonemap";
};

class ForwardRenderer {
public:
    auto initialize(RenderDevice& device, TextureFormat colorFormat) -> Result<void>;
    void shutdown();

    [[nodiscard]] auto is_ready() const -> bool;
    [[nodiscard]] auto recommended_depth_desc() const -> TextureDesc;
    [[nodiscard]] auto recommended_hdr_desc() const -> TextureDesc;
    [[nodiscard]] auto recommended_shadow_depth_desc() const -> TextureDesc;

    void set_view_params(const ForwardViewParams& params);
    void set_light(const ForwardDirectionalLight& light);
    void set_directional_lights(const std::vector<ForwardDirectionalLight>& lights);
    void set_point_lights(const std::vector<ForwardPointLight>& lights);
    void set_object(const ForwardObjectParams& object);
    void set_grid_params(const ForwardGridParams& params);
    void set_shadow_settings(const ForwardShadowSettings& settings);
    auto set_mesh_data(const std::vector<Vertex>& vertices, const std::vector<u32>& indices)
        -> Result<void>;
    auto reset_mesh_to_default_sphere() -> Result<void>;
    auto render_frame(u32 width, u32 height) -> Result<void>;
    void invalidate_frame_graph();

    auto add_frame_passes(RenderGraph& graph, const ForwardFrameTargets& targets)
        -> Result<void>;

private:
    auto add_main_pass(
        RenderGraph& graph,
        RenderGraphResourceHandle colorTarget,
        RenderGraphResourceHandle depthTarget,
        RenderGraphResourceHandle shadowDepthTarget,
        const String& passName = "ForwardMainPass") -> Result<RenderGraphPassHandle>;
    auto add_main_pass(
        RenderGraph& graph,
        RenderGraphResourceHandle colorTarget,
        RenderGraphResourceHandle depthTarget,
        RenderGraphResourceHandle shadowDepthNearTarget,
        RenderGraphResourceHandle shadowDepthFarTarget,
        const String& passName = "ForwardMainPass") -> Result<RenderGraphPassHandle>;
    auto add_shadow_pass(
        RenderGraph& graph,
        RenderGraphResourceHandle depthTarget,
        const String& passName = "ForwardShadowPass") -> Result<RenderGraphPassHandle>;
    auto add_shadow_cascade_pass(
        RenderGraph& graph,
        RenderGraphResourceHandle depthTarget,
        u32 cascadeIndex,
        const String& passName = "ForwardShadowCascadePass") -> Result<RenderGraphPassHandle>;
    auto add_tonemap_pass(
        RenderGraph& graph,
        RenderGraphResourceHandle hdrInput,
        RenderGraphResourceHandle outputTarget,
        const String& passName = "ForwardTonemapPass") -> Result<RenderGraphPassHandle>;

    auto build_default_sphere_mesh() -> Result<void>;
    auto upload_mesh_to_gpu(
        const std::vector<Vertex>& vertices,
        const std::vector<u32>& indices,
        StringView labelPrefix) -> Result<void>;
    void update_light_cascade_matrices(const glm::vec3& lightDirection);
    auto rebuild_frame_graph(u32 width, u32 height) -> Result<void>;
    void upload_global_uniforms();
    void upload_shadow_uniforms(u32 cascadeIndex);
    void upload_object_uniforms();

    RenderDevice* m_device = nullptr;
    Ptr<Shader> m_shader;
    Ptr<RenderPipeline> m_pipeline;
    Ptr<Shader> m_shadowShader;
    Ptr<RenderPipeline> m_shadowPipeline;
    Ptr<Shader> m_postShader;
    Ptr<RenderPipeline> m_postPipeline;
    Ptr<Sampler> m_tonemapSampler;
    Ptr<Sampler> m_shadowSampler;
    Ptr<Buffer> m_vertexBuffer;
    Ptr<Buffer> m_indexBuffer;
    Ptr<Buffer> m_globalUniformBuffer;
    std::array<Ptr<Buffer>, ForwardShadowCascadeCount> m_shadowGlobalUniformBuffers{};
    Ptr<Buffer> m_objectUniformBuffer;
    u32 m_indexCount = 0;
    u32 m_instanceCount = 1;

    glm::mat4 m_viewProjection{1.0f};
    glm::mat4 m_lightViewProjection{1.0f};
    std::array<glm::mat4, ForwardShadowCascadeCount> m_lightViewProjectionCascades{
        glm::mat4(1.0f), glm::mat4(1.0f)};
    glm::vec4 m_cameraPosition{0.0f, 0.0f, 3.0f, 1.0f};
    std::array<glm::vec4, ForwardMaxDirectionalLights> m_directionalDirectionIntensity{};
    std::array<glm::vec4, ForwardMaxDirectionalLights> m_directionalColor{};
    std::array<glm::vec4, ForwardMaxPointLights> m_pointPositionRange{};
    std::array<glm::vec4, ForwardMaxPointLights> m_pointColorIntensity{};
    u32 m_directionalLightCount = 0;
    u32 m_pointLightCount = 0;
    f32 m_ambientIntensity = 0.03f;

    glm::mat4 m_modelMatrix{1.0f};
    glm::mat4 m_normalMatrix{1.0f};
    glm::vec4 m_baseColor{1.0f, 0.766f, 0.336f, 1.0f};
    glm::vec4 m_emissive{0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 m_materialParams{1.0f, 0.25f, 1.0f, 0.0f};
    glm::vec4 m_gridParams{1.0f, 1.0f, 2.3f, 0.0f};
    glm::vec4 m_shadowParams{0.0008f, 0.0035f, 1.0f, 0.0f};
    glm::vec4 m_shadowCascadeParams{8.0f, 1.0f, 1.25f, 0.0f};
    glm::vec3 m_primaryLightDirection{-0.4f, -1.0f, -0.3f};
    u32 m_shadowMapResolution = 1024;
    SharedPtr<RenderGraph> m_frameGraph;
    u32 m_frameGraphWidth = 0;
    u32 m_frameGraphHeight = 0;
    bool m_frameGraphDirty = true;
};

} // namespace firefly
