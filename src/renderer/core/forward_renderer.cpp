module;
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <numbers>
#include <sstream>
#include <vector>

module firefly.renderer.forward_renderer;

namespace firefly {

namespace {

static constexpr const char* ForwardShaderPath = "assets/shaders/pbr_forward.wgsl";

static constexpr const char* ForwardShadowShaderPath = "assets/shaders/pbr_shadow.wgsl";

static constexpr const char* ForwardTonemapShaderPath = "assets/shaders/pbr_tonemap.wgsl";

static auto read_text_file(const std::filesystem::path& path) -> Result<String> {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return Result<String>::error("Cannot open file: " + path.string());
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return Result<String>::ok(ss.str());
}

static auto load_shader_source(const char* relativePath) -> Result<String> {
    std::vector<std::filesystem::path> candidates;
    candidates.emplace_back(relativePath);

#ifdef FIREFLY_SOURCE_DIR
    candidates.emplace_back(std::filesystem::path(FIREFLY_SOURCE_DIR) / relativePath);
#endif

    candidates.emplace_back(std::filesystem::path("..") / relativePath);
    candidates.emplace_back(std::filesystem::path("..") / ".." / relativePath);
    candidates.emplace_back(std::filesystem::path("..") / ".." / ".." / relativePath);

    String searched;
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return read_text_file(candidate);
        }
        if (!searched.empty()) {
            searched += "; ";
        }
        searched += candidate.string();
    }
    return Result<String>::error("Shader file not found. searched: " + searched);
}

struct alignas(16) GlobalUniformData {
    glm::mat4 viewProjection{1.0f};
    glm::mat4 lightViewProjectionNear{1.0f};
    glm::mat4 lightViewProjectionFar{1.0f};
    glm::vec4 cameraPosition{0.0f, 0.0f, 3.0f, 1.0f};
    std::array<glm::vec4, ForwardMaxDirectionalLights> directionalDirectionIntensity{};
    std::array<glm::vec4, ForwardMaxDirectionalLights> directionalColor{};
    std::array<glm::vec4, ForwardMaxPointLights> pointPositionRange{};
    std::array<glm::vec4, ForwardMaxPointLights> pointColorIntensity{};
    glm::vec4 lightCounts{0.0f, 0.0f, 0.03f, 0.0f};
    glm::vec4 shadowParams{0.0008f, 0.0035f, 1.0f, 0.0f};
    glm::vec4 shadowCascadeParams{8.0f, 1.0f, 1.25f, 0.0f};
};

struct alignas(16) ObjectUniformData {
    glm::mat4 model{1.0f};
    glm::mat4 normalMatrix{1.0f};
    glm::vec4 baseColor{1.0f, 0.766f, 0.336f, 1.0f};
    glm::vec4 emissive{0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 material{1.0f, 0.25f, 1.0f, 0.0f};
    glm::vec4 grid{1.0f, 1.0f, 2.3f, 0.0f};
};

static auto safe_normalize(const glm::vec3& v, const glm::vec3& fallback) -> glm::vec3 {
    const auto len2 = glm::dot(v, v);
    if (len2 <= 1e-6f) {
        return fallback;
    }
    return glm::normalize(v);
}

static auto compute_light_view_projection(
    const glm::vec3& lightDirection,
    f32 sceneExtent,
    f32 lightDistance,
    f32 nearPlane,
    f32 farPlane,
    f32 shadowResolution) -> glm::mat4 {
    const auto direction = safe_normalize(lightDirection, glm::vec3(-0.4f, -1.0f, -0.3f));
    const glm::vec3 sceneCenter{0.0f, 0.0f, 0.0f};

    const auto eye = sceneCenter - direction * lightDistance;
    const auto upHint = std::abs(direction.y) > 0.98f
        ? glm::vec3(0.0f, 0.0f, 1.0f)
        : glm::vec3(0.0f, 1.0f, 0.0f);

    const auto view = glm::lookAt(eye, sceneCenter, upHint);
    const auto proj = glm::ortho(
        -sceneExtent, sceneExtent,
        -sceneExtent, sceneExtent,
        nearPlane, farPlane);
    const f32 safeResolution = std::max(shadowResolution, 1.0f);
    const f32 texelWorldSize = (2.0f * sceneExtent) / safeResolution;
    const auto centerLightSpace = view * glm::vec4(sceneCenter, 1.0f);
    const f32 snappedX = std::floor(centerLightSpace.x / texelWorldSize) * texelWorldSize;
    const f32 snappedY = std::floor(centerLightSpace.y / texelWorldSize) * texelWorldSize;
    const auto snapOffset = glm::vec3(
        snappedX - centerLightSpace.x,
        snappedY - centerLightSpace.y,
        0.0f);
    const auto snapMatrix = glm::translate(glm::mat4(1.0f), snapOffset);
    const auto snappedView = snapMatrix * view;
    return proj * snappedView;
}

} // namespace

auto ForwardRenderer::initialize(RenderDevice& device, TextureFormat colorFormat) -> Result<void> {
    shutdown();
    m_device = &device;

    const auto forwardShaderResult = load_shader_source(ForwardShaderPath);
    if (forwardShaderResult.is_error()) {
        return Result<void>::error("Failed to load forward shader: " + forwardShaderResult.error());
    }
    const auto shadowShaderResult = load_shader_source(ForwardShadowShaderPath);
    if (shadowShaderResult.is_error()) {
        return Result<void>::error("Failed to load forward shadow shader: " + shadowShaderResult.error());
    }
    const auto tonemapShaderResult = load_shader_source(ForwardTonemapShaderPath);
    if (tonemapShaderResult.is_error()) {
        return Result<void>::error("Failed to load forward tonemap shader: " + tonemapShaderResult.error());
    }

    ShaderDesc shaderDesc;
    shaderDesc.label = "ForwardShader";
    shaderDesc.wgslCode = forwardShaderResult.value();
    m_shader = device.create_shader(shaderDesc);
    if (!m_shader || !m_shader->native_handle()) {
        return Result<void>::error("Failed to create forward shader");
    }

    PipelineDesc pipelineDesc;
    pipelineDesc.label = "ForwardPipeline";
    pipelineDesc.vertexShader = m_shader.get();
    pipelineDesc.fragmentShader = m_shader.get();
    pipelineDesc.colorFormat = TextureFormat::RGBA16Float;
    pipelineDesc.depthTest = true;
    pipelineDesc.depthFormat = TextureFormat::Depth32Float;
    pipelineDesc.cullMode = CullMode::Back;

    VertexBufferLayoutDesc layout;
    layout.arrayStride = static_cast<u32>(sizeof(Vertex));
    layout.stepMode = VertexStepMode::Vertex;
    layout.attributes = {
        {.location = 0, .offset = static_cast<u32>(offsetof(Vertex, position)),
         .format = VertexFormat::Float32x3},
        {.location = 1, .offset = static_cast<u32>(offsetof(Vertex, normal)),
         .format = VertexFormat::Float32x3},
        {.location = 2, .offset = static_cast<u32>(offsetof(Vertex, texCoord)),
         .format = VertexFormat::Float32x2},
    };
    pipelineDesc.vertexBuffers = {layout};

    m_pipeline = device.create_pipeline(pipelineDesc);
    if (!m_pipeline || !m_pipeline->native_handle()) {
        return Result<void>::error("Failed to create forward render pipeline");
    }

    ShaderDesc shadowShaderDesc;
    shadowShaderDesc.label = "ForwardShadowShader";
    shadowShaderDesc.wgslCode = shadowShaderResult.value();
    m_shadowShader = device.create_shader(shadowShaderDesc);
    if (!m_shadowShader || !m_shadowShader->native_handle()) {
        return Result<void>::error("Failed to create forward shadow shader");
    }

    PipelineDesc shadowPipelineDesc;
    shadowPipelineDesc.label = "ForwardShadowPipeline";
    shadowPipelineDesc.vertexShader = m_shadowShader.get();
    shadowPipelineDesc.enableFragment = false;
    shadowPipelineDesc.colorFormat = TextureFormat::RGBA8Unorm;
    shadowPipelineDesc.depthTest = true;
    shadowPipelineDesc.depthFormat = TextureFormat::Depth32Float;
    shadowPipelineDesc.cullMode = CullMode::Back;
    shadowPipelineDesc.vertexBuffers = {layout};
    m_shadowPipeline = device.create_pipeline(shadowPipelineDesc);
    if (!m_shadowPipeline || !m_shadowPipeline->native_handle()) {
        return Result<void>::error("Failed to create forward shadow pipeline");
    }

    SamplerDesc shadowSamplerDesc;
    shadowSamplerDesc.addressModeU = AddressMode::ClampToEdge;
    shadowSamplerDesc.addressModeV = AddressMode::ClampToEdge;
    shadowSamplerDesc.addressModeW = AddressMode::ClampToEdge;
    shadowSamplerDesc.magFilter = FilterMode::Linear;
    shadowSamplerDesc.minFilter = FilterMode::Linear;
    shadowSamplerDesc.mipmapFilter = FilterMode::Nearest;
    shadowSamplerDesc.enableCompare = true;
    shadowSamplerDesc.compareFunction = CompareFunction::LessEqual;
    m_shadowSampler = device.create_sampler(shadowSamplerDesc);
    if (!m_shadowSampler || !m_shadowSampler->native_handle()) {
        return Result<void>::error("Failed to create forward shadow comparison sampler");
    }

    ShaderDesc postShaderDesc;
    postShaderDesc.label = "ForwardTonemapShader";
    postShaderDesc.wgslCode = tonemapShaderResult.value();
    m_postShader = device.create_shader(postShaderDesc);
    if (!m_postShader || !m_postShader->native_handle()) {
        return Result<void>::error("Failed to create forward tonemap shader");
    }

    PipelineDesc postPipelineDesc;
    postPipelineDesc.label = "ForwardTonemapPipeline";
    postPipelineDesc.vertexShader = m_postShader.get();
    postPipelineDesc.fragmentShader = m_postShader.get();
    postPipelineDesc.colorFormat = colorFormat;
    postPipelineDesc.depthTest = false;
    postPipelineDesc.cullMode = CullMode::None;
    m_postPipeline = device.create_pipeline(postPipelineDesc);
    if (!m_postPipeline || !m_postPipeline->native_handle()) {
        return Result<void>::error("Failed to create forward tonemap pipeline");
    }

    SamplerDesc tonemapSamplerDesc;
    tonemapSamplerDesc.addressModeU = AddressMode::ClampToEdge;
    tonemapSamplerDesc.addressModeV = AddressMode::ClampToEdge;
    tonemapSamplerDesc.addressModeW = AddressMode::ClampToEdge;
    tonemapSamplerDesc.magFilter = FilterMode::Linear;
    tonemapSamplerDesc.minFilter = FilterMode::Linear;
    tonemapSamplerDesc.mipmapFilter = FilterMode::Nearest;
    m_tonemapSampler = device.create_sampler(tonemapSamplerDesc);
    if (!m_tonemapSampler || !m_tonemapSampler->native_handle()) {
        return Result<void>::error("Failed to create forward tonemap sampler");
    }

    BufferDesc globalDesc;
    globalDesc.label = "ForwardGlobalUniform";
    globalDesc.size = sizeof(GlobalUniformData);
    globalDesc.usage = BufferUsage::Uniform;
    m_globalUniformBuffer = device.create_buffer(globalDesc);
    if (!m_globalUniformBuffer || !m_globalUniformBuffer->native_handle()) {
        return Result<void>::error("Failed to create forward global uniform buffer");
    }

    for (u32 cascadeIndex = 0; cascadeIndex < ForwardShadowCascadeCount; ++cascadeIndex) {
        BufferDesc shadowGlobalDesc;
        shadowGlobalDesc.label = "ForwardShadowGlobalUniform";
        shadowGlobalDesc.size = sizeof(GlobalUniformData);
        shadowGlobalDesc.usage = BufferUsage::Uniform;
        m_shadowGlobalUniformBuffers[cascadeIndex] = device.create_buffer(shadowGlobalDesc);
        if (!m_shadowGlobalUniformBuffers[cascadeIndex] ||
            !m_shadowGlobalUniformBuffers[cascadeIndex]->native_handle()) {
            return Result<void>::error("Failed to create forward shadow global uniform buffer");
        }
    }

    BufferDesc objectDesc;
    objectDesc.label = "ForwardObjectUniform";
    objectDesc.size = sizeof(ObjectUniformData);
    objectDesc.usage = BufferUsage::Uniform;
    m_objectUniformBuffer = device.create_buffer(objectDesc);
    if (!m_objectUniformBuffer || !m_objectUniformBuffer->native_handle()) {
        return Result<void>::error("Failed to create forward object uniform buffer");
    }

    auto meshResult = build_default_sphere_mesh();
    if (meshResult.is_error()) {
        return meshResult;
    }

    m_viewProjection = glm::mat4(1.0f);
    m_lightViewProjection = glm::mat4(1.0f);
    m_lightViewProjectionCascades = {glm::mat4(1.0f), glm::mat4(1.0f)};
    m_cameraPosition = glm::vec4(0.0f, 0.0f, 3.0f, 1.0f);
    m_directionalDirectionIntensity.fill(glm::vec4(0.0f));
    m_directionalColor.fill(glm::vec4(0.0f));
    m_pointPositionRange.fill(glm::vec4(0.0f));
    m_pointColorIntensity.fill(glm::vec4(0.0f));
    m_directionalLightCount = 0;
    m_pointLightCount = 0;
    m_ambientIntensity = 0.03f;
    set_light(ForwardDirectionalLight{});
    m_modelMatrix = glm::mat4(1.0f);
    m_normalMatrix = glm::mat4(1.0f);
    m_baseColor = glm::vec4(1.0f, 0.766f, 0.336f, 1.0f);
    m_emissive = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    m_materialParams = glm::vec4(1.0f, 0.25f, 1.0f, 0.0f);
    m_gridParams = glm::vec4(1.0f, 1.0f, 2.3f, 0.0f);
    m_shadowParams = glm::vec4(0.0008f, 0.0035f, 1.0f, 0.0f);
    m_shadowCascadeParams = glm::vec4(8.0f, 1.0f, 1.25f, 0.0f);
    m_primaryLightDirection = glm::vec3(-0.4f, -1.0f, -0.3f);
    m_shadowMapResolution = 1024;
    m_instanceCount = 1;
    invalidate_frame_graph();
    upload_global_uniforms();
    upload_object_uniforms();
    return Result<void>::ok();
}

void ForwardRenderer::shutdown() {
    if (m_frameGraph) {
        m_frameGraph->clear();
    }
    m_frameGraph.reset();
    m_frameGraphWidth = 0;
    m_frameGraphHeight = 0;
    m_frameGraphDirty = true;
    m_indexCount = 0;
    m_instanceCount = 1;
    m_objectUniformBuffer.reset();
    for (auto& shadowBuffer : m_shadowGlobalUniformBuffers) {
        shadowBuffer.reset();
    }
    m_globalUniformBuffer.reset();
    m_tonemapSampler.reset();
    m_shadowSampler.reset();
    m_indexBuffer.reset();
    m_vertexBuffer.reset();
    m_postPipeline.reset();
    m_postShader.reset();
    m_shadowPipeline.reset();
    m_shadowShader.reset();
    m_pipeline.reset();
    m_shader.reset();
    m_device = nullptr;
}

auto ForwardRenderer::is_ready() const -> bool {
    for (const auto& shadowBuffer : m_shadowGlobalUniformBuffers) {
        if (!shadowBuffer || !shadowBuffer->native_handle()) {
            return false;
        }
    }
    return m_device &&
        m_pipeline && m_pipeline->native_handle() &&
        m_shadowPipeline && m_shadowPipeline->native_handle() &&
        m_postPipeline && m_postPipeline->native_handle() &&
        m_tonemapSampler && m_tonemapSampler->native_handle() &&
        m_shadowSampler && m_shadowSampler->native_handle() &&
        m_vertexBuffer && m_vertexBuffer->native_handle() &&
        m_indexBuffer && m_indexBuffer->native_handle() &&
        m_globalUniformBuffer && m_globalUniformBuffer->native_handle() &&
        m_objectUniformBuffer && m_objectUniformBuffer->native_handle() &&
        m_indexCount > 0;
}

auto ForwardRenderer::recommended_depth_desc() const -> TextureDesc {
    TextureDesc desc;
    desc.label = "ForwardDepth";
    desc.width = 0;
    desc.height = 0;
    desc.format = TextureFormat::Depth32Float;
    desc.usage = TextureUsage::RenderAttachment;
    return desc;
}

auto ForwardRenderer::recommended_hdr_desc() const -> TextureDesc {
    TextureDesc desc;
    desc.label = "ForwardHDRColor";
    desc.width = 0;
    desc.height = 0;
    desc.format = TextureFormat::RGBA16Float;
    desc.usage = TextureUsage::RenderAttachment | TextureUsage::TextureBinding;
    return desc;
}

auto ForwardRenderer::recommended_shadow_depth_desc() const -> TextureDesc {
    TextureDesc desc;
    desc.label = "ForwardShadowDepth";
    desc.width = 1024;
    desc.height = 1024;
    desc.format = TextureFormat::Depth32Float;
    desc.usage = TextureUsage::RenderAttachment | TextureUsage::TextureBinding;
    return desc;
}

void ForwardRenderer::set_view_params(const ForwardViewParams& params) {
    m_viewProjection = params.viewProjection;
    m_cameraPosition = glm::vec4(params.cameraWorldPosition, 1.0f);
    upload_global_uniforms();
}

void ForwardRenderer::set_light(const ForwardDirectionalLight& light) {
    set_directional_lights({light});
}

void ForwardRenderer::set_directional_lights(const std::vector<ForwardDirectionalLight>& lights) {
    m_directionalDirectionIntensity.fill(glm::vec4(0.0f));
    m_directionalColor.fill(glm::vec4(0.0f));
    m_directionalLightCount = std::min<u32>(
        static_cast<u32>(lights.size()),
        ForwardMaxDirectionalLights);

    if (m_directionalLightCount == 0) {
        m_ambientIntensity = 0.01f;
        m_primaryLightDirection = glm::vec3(-0.4f, -1.0f, -0.3f);
        m_lightViewProjection = glm::mat4(1.0f);
        m_lightViewProjectionCascades = {glm::mat4(1.0f), glm::mat4(1.0f)};
        upload_global_uniforms();
        return;
    }

    for (u32 i = 0; i < m_directionalLightCount; ++i) {
        const auto& light = lights[i];
        const auto direction = safe_normalize(light.direction, glm::vec3(0.0f, -1.0f, 0.0f));
        m_directionalDirectionIntensity[i] = glm::vec4(direction, std::max(light.intensity, 0.0f));
        m_directionalColor[i] = glm::vec4(glm::max(light.color, glm::vec3(0.0f)), 1.0f);
        if (i == 0) {
            m_primaryLightDirection = direction;
        }
    }

    m_ambientIntensity = std::max(lights[0].ambientIntensity, 0.0f);
    update_light_cascade_matrices(m_primaryLightDirection);
    upload_global_uniforms();
}

void ForwardRenderer::set_point_lights(const std::vector<ForwardPointLight>& lights) {
    m_pointPositionRange.fill(glm::vec4(0.0f));
    m_pointColorIntensity.fill(glm::vec4(0.0f));
    m_pointLightCount = std::min<u32>(
        static_cast<u32>(lights.size()),
        ForwardMaxPointLights);

    for (u32 i = 0; i < m_pointLightCount; ++i) {
        const auto& light = lights[i];
        m_pointPositionRange[i] = glm::vec4(light.position, std::max(light.range, 0.001f));
        m_pointColorIntensity[i] = glm::vec4(
            glm::max(light.color, glm::vec3(0.0f)),
            std::max(light.intensity, 0.0f));
    }
    upload_global_uniforms();
}

void ForwardRenderer::set_object(const ForwardObjectParams& object) {
    m_modelMatrix = object.model;
    m_normalMatrix = glm::inverseTranspose(object.model);
    m_baseColor = object.material.baseColor;
    m_emissive = glm::vec4(object.material.emissive, 1.0f);
    m_materialParams = glm::vec4(
        std::clamp(object.material.metallic, 0.0f, 1.0f),
        std::clamp(object.material.roughness, 0.045f, 1.0f),
        std::clamp(object.material.ao, 0.0f, 1.0f),
        0.0f);
    upload_object_uniforms();
}

void ForwardRenderer::set_grid_params(const ForwardGridParams& params) {
    const u32 columns = std::max(1u, params.columns);
    const u32 rows = std::max(1u, params.rows);
    const f32 spacing = std::max(params.spacing, 0.1f);
    const bool enabled = params.enabled && (columns * rows > 1u);
    m_gridParams = glm::vec4(
        static_cast<f32>(columns),
        static_cast<f32>(rows),
        spacing,
        enabled ? 1.0f : 0.0f);
    m_instanceCount = enabled ? (columns * rows) : 1u;
    upload_object_uniforms();
}

void ForwardRenderer::update_light_cascade_matrices(const glm::vec3& lightDirection) {
    const auto direction = safe_normalize(lightDirection, glm::vec3(-0.4f, -1.0f, -0.3f));
    const f32 splitDistance = std::max(m_shadowCascadeParams.x, 0.5f);
    const f32 nearExtent = std::max(4.0f, splitDistance * 0.95f);
    const f32 farExtent = std::max(nearExtent + 4.0f, splitDistance * 2.2f);
    const f32 nearDistance = std::max(8.0f, nearExtent + 4.0f);
    const f32 farDistance = std::max(nearDistance + 6.0f, farExtent + 6.0f);
    const f32 farPlaneNear = std::max(nearDistance + 10.0f, 28.0f);
    const f32 farPlaneFar = std::max(farDistance + 28.0f, 60.0f);
    const f32 resolution = static_cast<f32>(std::max(1u, m_shadowMapResolution));

    m_lightViewProjectionCascades[0] = compute_light_view_projection(
        direction, nearExtent, nearDistance, 1.0f, farPlaneNear, resolution);
    m_lightViewProjectionCascades[1] = compute_light_view_projection(
        direction, farExtent, farDistance, 1.0f, farPlaneFar, resolution);
    m_lightViewProjection = m_lightViewProjectionCascades[0];
}

void ForwardRenderer::set_shadow_settings(const ForwardShadowSettings& settings) {
    m_shadowParams = glm::vec4(
        std::max(settings.constantBias, 0.0f),
        std::max(settings.slopeBias, 0.0f),
        settings.enablePCF ? 1.0f : 0.0f,
        0.0f);
    m_shadowCascadeParams = glm::vec4(
        std::max(settings.cascadeSplitDistance, 0.1f),
        settings.enableCascades ? 1.0f : 0.0f,
        std::max(settings.cascadeBlendDistance, 0.001f),
        0.0f);
    m_shadowMapResolution = std::max(settings.shadowMapResolution, 64u);
    update_light_cascade_matrices(m_primaryLightDirection);
    m_frameGraphDirty = true;
    upload_global_uniforms();
}

void ForwardRenderer::invalidate_frame_graph() {
    m_frameGraphDirty = true;
}

auto ForwardRenderer::rebuild_frame_graph(u32 width, u32 height) -> Result<void> {
    if (!is_ready()) {
        return Result<void>::error("Forward renderer is not initialized");
    }
    if (width == 0 || height == 0) {
        return Result<void>::error("Forward frame graph requires non-zero dimensions");
    }

    auto graph = SharedPtr<RenderGraph>(new RenderGraph());
    auto backbuffer = graph->import_backbuffer("Backbuffer");

    auto depthDesc = recommended_depth_desc();
    depthDesc.width = width;
    depthDesc.height = height;
    auto depthTarget = graph->create_texture("ForwardDepth", depthDesc);

    auto shadowDepthDesc = recommended_shadow_depth_desc();
    auto shadowDepthNearTarget = graph->create_texture("ForwardShadowDepthNear", shadowDepthDesc);
    auto shadowDepthFarTarget = graph->create_texture("ForwardShadowDepthFar", shadowDepthDesc);

    auto hdrDesc = recommended_hdr_desc();
    hdrDesc.width = width;
    hdrDesc.height = height;
    auto hdrTarget = graph->create_texture("ForwardHDR", hdrDesc);

    auto addPassResult = add_frame_passes(*graph, {
        .outputColorTarget = backbuffer,
        .hdrColorTarget = hdrTarget,
        .depthTarget = depthTarget,
        .shadowDepthNearTarget = shadowDepthNearTarget,
        .shadowDepthFarTarget = shadowDepthFarTarget,
        .shadowNearPassName = "ForwardShadowNear",
        .shadowFarPassName = "ForwardShadowFar",
        .mainPassName = "ForwardMain",
        .tonemapPassName = "ForwardTonemap",
    });
    if (addPassResult.is_error()) {
        return addPassResult;
    }

    auto compileResult = graph->compile();
    if (compileResult.is_error()) {
        return Result<void>::error("Failed to compile forward frame graph: " + compileResult.error());
    }

    if (m_frameGraph) {
        m_frameGraph->clear();
    }
    m_frameGraph = std::move(graph);
    m_frameGraphWidth = width;
    m_frameGraphHeight = height;
    m_frameGraphDirty = false;
    return Result<void>::ok();
}

auto ForwardRenderer::render_frame(u32 width, u32 height) -> Result<void> {
    if (!is_ready()) {
        return Result<void>::error("Forward renderer is not initialized");
    }
    if (width == 0 || height == 0) {
        return Result<void>::error("Forward renderer frame dimensions must be non-zero");
    }
    if (!m_device) {
        return Result<void>::error("Forward renderer has no render device");
    }

    if (!m_frameGraph ||
        m_frameGraphDirty ||
        m_frameGraphWidth != width ||
        m_frameGraphHeight != height) {
        auto rebuildResult = rebuild_frame_graph(width, height);
        if (rebuildResult.is_error()) {
            return rebuildResult;
        }
    }

    auto executeResult = m_frameGraph->execute(*m_device);
    if (executeResult.is_error()) {
        return Result<void>::error("Failed to execute forward frame graph: " + executeResult.error());
    }
    return Result<void>::ok();
}

auto ForwardRenderer::add_frame_passes(
    RenderGraph& graph,
    const ForwardFrameTargets& targets) -> Result<void> {
    if (!is_ready()) {
        return Result<void>::error("Forward renderer is not initialized");
    }
    if (!targets.outputColorTarget.valid()) {
        return Result<void>::error("Forward frame requires a valid output color target");
    }
    if (!targets.hdrColorTarget.valid()) {
        return Result<void>::error("Forward frame requires a valid HDR color target");
    }
    if (!targets.depthTarget.valid()) {
        return Result<void>::error("Forward frame requires a valid depth target");
    }
    if (!targets.shadowDepthNearTarget.valid()) {
        return Result<void>::error("Forward frame requires a valid near shadow depth target");
    }

    const bool useCascades = (m_shadowCascadeParams.y > 0.5f);
    if (useCascades && !targets.shadowDepthFarTarget.valid()) {
        return Result<void>::error("Forward frame requires a valid far shadow depth target");
    }

    auto nearShadowPass = add_shadow_cascade_pass(
        graph,
        targets.shadowDepthNearTarget,
        0,
        targets.shadowNearPassName);
    if (nearShadowPass.is_error()) {
        return Result<void>::error(nearShadowPass.error());
    }

    auto farShadowResource = targets.shadowDepthNearTarget;
    if (useCascades) {
        farShadowResource = targets.shadowDepthFarTarget;
        auto farShadowPass = add_shadow_cascade_pass(
            graph,
            targets.shadowDepthFarTarget,
            1,
            targets.shadowFarPassName);
        if (farShadowPass.is_error()) {
            return Result<void>::error(farShadowPass.error());
        }
    }

    auto mainPass = add_main_pass(
        graph,
        targets.hdrColorTarget,
        targets.depthTarget,
        targets.shadowDepthNearTarget,
        farShadowResource,
        targets.mainPassName);
    if (mainPass.is_error()) {
        return Result<void>::error(mainPass.error());
    }

    auto tonemapPass = add_tonemap_pass(
        graph,
        targets.hdrColorTarget,
        targets.outputColorTarget,
        targets.tonemapPassName);
    if (tonemapPass.is_error()) {
        return Result<void>::error(tonemapPass.error());
    }

    return Result<void>::ok();
}

auto ForwardRenderer::add_main_pass(
    RenderGraph& graph,
    RenderGraphResourceHandle colorTarget,
    RenderGraphResourceHandle depthTarget,
    RenderGraphResourceHandle shadowDepthTarget,
    const String& passName) -> Result<RenderGraphPassHandle> {
    return add_main_pass(
        graph,
        colorTarget,
        depthTarget,
        shadowDepthTarget,
        shadowDepthTarget,
        passName);
}

auto ForwardRenderer::add_main_pass(
    RenderGraph& graph,
    RenderGraphResourceHandle colorTarget,
    RenderGraphResourceHandle depthTarget,
    RenderGraphResourceHandle shadowDepthNearTarget,
    RenderGraphResourceHandle shadowDepthFarTarget,
    const String& passName) -> Result<RenderGraphPassHandle> {
    if (!is_ready()) {
        return Result<RenderGraphPassHandle>::error("Forward renderer is not initialized");
    }
    if (!colorTarget.valid()) {
        return Result<RenderGraphPassHandle>::error("Forward pass requires a valid color target");
    }
    if (!depthTarget.valid()) {
        return Result<RenderGraphPassHandle>::error("Forward pass requires a valid depth target");
    }
    if (!shadowDepthNearTarget.valid()) {
        return Result<RenderGraphPassHandle>::error(
            "Forward pass requires a valid near-cascade shadow depth target");
    }
    if (!shadowDepthFarTarget.valid()) {
        return Result<RenderGraphPassHandle>::error(
            "Forward pass requires a valid far-cascade shadow depth target");
    }

    RasterPassDesc passDesc;
    passDesc.name = passName;
    passDesc.colorTarget = colorTarget;
    passDesc.depthTarget = depthTarget;
    passDesc.clearColor = true;
    passDesc.clearR = 0.04f;
    passDesc.clearG = 0.05f;
    passDesc.clearB = 0.08f;
    passDesc.clearA = 1.0f;
    passDesc.clearDepth = true;
    passDesc.clearDepthValue = 1.0f;

    auto pass = graph.add_raster_pass(
        passDesc,
        [this](RenderGraphPassContext& ctx) {
            auto& command = ctx.command();
            command.set_pipeline(*m_pipeline);
            command.set_vertex_buffer(0, *m_vertexBuffer);
            command.set_index_buffer(*m_indexBuffer);
            command.draw_indexed(m_indexCount, m_instanceCount);
        });
    if (!pass.valid()) {
        return Result<RenderGraphPassHandle>::error("Failed to add forward main raster pass");
    }

    auto setPipeline = graph.set_raster_pipeline(pass, m_pipeline.get());
    if (setPipeline.is_error()) {
        return Result<RenderGraphPassHandle>::error(setPipeline.error());
    }

    auto globalBinding = graph.add_buffer_binding(pass, {
        .group = 0,
        .binding = 0,
        .buffer = m_globalUniformBuffer.get(),
        .offset = 0,
        .size = sizeof(GlobalUniformData),
    });
    if (globalBinding.is_error()) {
        return Result<RenderGraphPassHandle>::error(globalBinding.error());
    }

    auto objectBinding = graph.add_buffer_binding(pass, {
        .group = 1,
        .binding = 0,
        .buffer = m_objectUniformBuffer.get(),
        .offset = 0,
        .size = sizeof(ObjectUniformData),
    });
    if (objectBinding.is_error()) {
        return Result<RenderGraphPassHandle>::error(objectBinding.error());
    }

    auto shadowDepthNearBinding = graph.add_texture_binding(pass, {
        .group = 2,
        .binding = 0,
        .resource = shadowDepthNearTarget,
        .access = RenderGraphBindingAccess::Read,
    });
    if (shadowDepthNearBinding.is_error()) {
        return Result<RenderGraphPassHandle>::error(shadowDepthNearBinding.error());
    }

    auto shadowDepthFarBinding = graph.add_texture_binding(pass, {
        .group = 2,
        .binding = 1,
        .resource = shadowDepthFarTarget,
        .access = RenderGraphBindingAccess::Read,
    });
    if (shadowDepthFarBinding.is_error()) {
        return Result<RenderGraphPassHandle>::error(shadowDepthFarBinding.error());
    }

    auto shadowSamplerBinding = graph.add_sampler_binding(pass, {
        .group = 2,
        .binding = 2,
        .sampler = m_shadowSampler.get(),
    });
    if (shadowSamplerBinding.is_error()) {
        return Result<RenderGraphPassHandle>::error(shadowSamplerBinding.error());
    }

    return Result<RenderGraphPassHandle>::ok(pass);
}

auto ForwardRenderer::add_shadow_pass(
    RenderGraph& graph,
    RenderGraphResourceHandle depthTarget,
    const String& passName) -> Result<RenderGraphPassHandle> {
    return add_shadow_cascade_pass(graph, depthTarget, 0, passName);
}

auto ForwardRenderer::add_shadow_cascade_pass(
    RenderGraph& graph,
    RenderGraphResourceHandle depthTarget,
    u32 cascadeIndex,
    const String& passName) -> Result<RenderGraphPassHandle> {
    if (!is_ready()) {
        return Result<RenderGraphPassHandle>::error("Forward shadow pass requires initialized renderer");
    }
    if (!depthTarget.valid()) {
        return Result<RenderGraphPassHandle>::error("Forward shadow pass requires valid depth target");
    }
    if (cascadeIndex >= ForwardShadowCascadeCount) {
        return Result<RenderGraphPassHandle>::error("Forward shadow pass cascade index is out of range");
    }

    RasterPassDesc passDesc;
    passDesc.name = passName;
    passDesc.clearColor = false;
    passDesc.depthTarget = depthTarget;
    passDesc.clearDepth = true;
    passDesc.clearDepthValue = 1.0f;

    auto pass = graph.add_raster_pass(
        passDesc,
        [this](RenderGraphPassContext& ctx) {
            auto& command = ctx.command();
            command.set_pipeline(*m_shadowPipeline);
            command.set_vertex_buffer(0, *m_vertexBuffer);
            command.set_index_buffer(*m_indexBuffer);
            command.draw_indexed(m_indexCount, m_instanceCount);
        });
    if (!pass.valid()) {
        return Result<RenderGraphPassHandle>::error("Failed to add forward shadow pass");
    }

    auto setPipeline = graph.set_raster_pipeline(pass, m_shadowPipeline.get());
    if (setPipeline.is_error()) {
        return Result<RenderGraphPassHandle>::error(setPipeline.error());
    }

    auto globalBinding = graph.add_buffer_binding(pass, {
        .group = 0,
        .binding = 0,
        .buffer = m_shadowGlobalUniformBuffers[cascadeIndex].get(),
        .offset = 0,
        .size = sizeof(GlobalUniformData),
    });
    if (globalBinding.is_error()) {
        return Result<RenderGraphPassHandle>::error(globalBinding.error());
    }

    auto objectBinding = graph.add_buffer_binding(pass, {
        .group = 1,
        .binding = 0,
        .buffer = m_objectUniformBuffer.get(),
        .offset = 0,
        .size = sizeof(ObjectUniformData),
    });
    if (objectBinding.is_error()) {
        return Result<RenderGraphPassHandle>::error(objectBinding.error());
    }

    return Result<RenderGraphPassHandle>::ok(pass);
}

auto ForwardRenderer::add_tonemap_pass(
    RenderGraph& graph,
    RenderGraphResourceHandle hdrInput,
    RenderGraphResourceHandle outputTarget,
    const String& passName) -> Result<RenderGraphPassHandle> {
    if (!is_ready()) {
        return Result<RenderGraphPassHandle>::error("Forward tonemap pass requires initialized renderer");
    }
    if (!hdrInput.valid()) {
        return Result<RenderGraphPassHandle>::error("Forward tonemap pass requires valid HDR input");
    }
    if (!outputTarget.valid()) {
        return Result<RenderGraphPassHandle>::error("Forward tonemap pass requires valid output target");
    }

    RasterPassDesc passDesc;
    passDesc.name = passName;
    passDesc.colorTarget = outputTarget;
    passDesc.clearColor = true;
    passDesc.clearR = 0.0f;
    passDesc.clearG = 0.0f;
    passDesc.clearB = 0.0f;
    passDesc.clearA = 1.0f;
    passDesc.clearDepth = false;

    auto pass = graph.add_raster_pass(
        passDesc,
        [this](RenderGraphPassContext& ctx) {
            auto& command = ctx.command();
            command.set_pipeline(*m_postPipeline);
            command.draw(3);
        });
    if (!pass.valid()) {
        return Result<RenderGraphPassHandle>::error("Failed to add forward tonemap pass");
    }

    auto setPipeline = graph.set_raster_pipeline(pass, m_postPipeline.get());
    if (setPipeline.is_error()) {
        return Result<RenderGraphPassHandle>::error(setPipeline.error());
    }

    auto texBinding = graph.add_texture_binding(pass, {
        .group = 0,
        .binding = 0,
        .resource = hdrInput,
        .access = RenderGraphBindingAccess::Read,
    });
    if (texBinding.is_error()) {
        return Result<RenderGraphPassHandle>::error(texBinding.error());
    }

    auto samplerBinding = graph.add_sampler_binding(pass, {
        .group = 0,
        .binding = 1,
        .sampler = m_tonemapSampler.get(),
    });
    if (samplerBinding.is_error()) {
        return Result<RenderGraphPassHandle>::error(samplerBinding.error());
    }

    return Result<RenderGraphPassHandle>::ok(pass);
}

auto ForwardRenderer::build_default_sphere_mesh() -> Result<void> {
    constexpr u32 xSegments = 64;
    constexpr u32 ySegments = 32;

    std::vector<Vertex> vertices;
    vertices.reserve(static_cast<size_t>(xSegments + 1) * static_cast<size_t>(ySegments + 1));

    std::vector<u32> indices;
    indices.reserve(static_cast<size_t>(xSegments) * static_cast<size_t>(ySegments) * 6u);

    for (u32 y = 0; y <= ySegments; ++y) {
        for (u32 x = 0; x <= xSegments; ++x) {
            const f32 xRatio = static_cast<f32>(x) / static_cast<f32>(xSegments);
            const f32 yRatio = static_cast<f32>(y) / static_cast<f32>(ySegments);

            const f32 xPos = std::cos(xRatio * std::numbers::pi_v<f32> * 2.0f) *
                std::sin(yRatio * std::numbers::pi_v<f32>);
            const f32 yPos = std::cos(yRatio * std::numbers::pi_v<f32>);
            const f32 zPos = std::sin(xRatio * std::numbers::pi_v<f32> * 2.0f) *
                std::sin(yRatio * std::numbers::pi_v<f32>);

            Vertex v{};
            v.position = glm::vec3(xPos, yPos, zPos);
            v.normal = glm::normalize(v.position);
            v.texCoord = glm::vec2(xRatio, yRatio);
            v.color = glm::vec4(1.0f);
            vertices.push_back(v);
        }
    }

    for (u32 y = 0; y < ySegments; ++y) {
        for (u32 x = 0; x < xSegments; ++x) {
            const u32 i0 = y * (xSegments + 1u) + x;
            const u32 i1 = i0 + 1u;
            const u32 i2 = (y + 1u) * (xSegments + 1u) + x;
            const u32 i3 = i2 + 1u;

            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);
            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }

    m_indexCount = static_cast<u32>(indices.size());
    if (m_indexCount == 0 || vertices.empty()) {
        return Result<void>::error("Generated forward sphere mesh is empty");
    }

    BufferDesc vertexDesc;
    vertexDesc.label = "ForwardSphereVertexBuffer";
    vertexDesc.size = static_cast<u64>(vertices.size()) * sizeof(Vertex);
    vertexDesc.usage = BufferUsage::Vertex;
    m_vertexBuffer = m_device->create_buffer(vertexDesc);
    if (!m_vertexBuffer || !m_vertexBuffer->native_handle()) {
        return Result<void>::error("Failed to create forward vertex buffer");
    }
    m_vertexBuffer->write(vertices.data(), vertexDesc.size);

    BufferDesc indexDesc;
    indexDesc.label = "ForwardSphereIndexBuffer";
    indexDesc.size = static_cast<u64>(indices.size()) * sizeof(u32);
    indexDesc.usage = BufferUsage::Index;
    m_indexBuffer = m_device->create_buffer(indexDesc);
    if (!m_indexBuffer || !m_indexBuffer->native_handle()) {
        return Result<void>::error("Failed to create forward index buffer");
    }
    m_indexBuffer->write(indices.data(), indexDesc.size);

    return Result<void>::ok();
}

void ForwardRenderer::upload_global_uniforms() {
    if (!m_globalUniformBuffer || !m_globalUniformBuffer->native_handle()) {
        return;
    }
    GlobalUniformData data{};
    data.viewProjection = m_viewProjection;
    data.lightViewProjectionNear = m_lightViewProjectionCascades[0];
    data.lightViewProjectionFar = m_lightViewProjectionCascades[1];
    data.cameraPosition = m_cameraPosition;
    data.directionalDirectionIntensity = m_directionalDirectionIntensity;
    data.directionalColor = m_directionalColor;
    data.pointPositionRange = m_pointPositionRange;
    data.pointColorIntensity = m_pointColorIntensity;
    data.lightCounts = glm::vec4(
        static_cast<f32>(m_directionalLightCount),
        static_cast<f32>(m_pointLightCount),
        m_ambientIntensity,
        0.0f);
    data.shadowParams = m_shadowParams;
    data.shadowCascadeParams = m_shadowCascadeParams;
    m_globalUniformBuffer->write(&data, sizeof(GlobalUniformData), 0);
    for (u32 cascadeIndex = 0; cascadeIndex < ForwardShadowCascadeCount; ++cascadeIndex) {
        upload_shadow_uniforms(cascadeIndex);
    }
}

void ForwardRenderer::upload_shadow_uniforms(u32 cascadeIndex) {
    if (cascadeIndex >= ForwardShadowCascadeCount) {
        return;
    }
    auto& buffer = m_shadowGlobalUniformBuffers[cascadeIndex];
    if (!buffer || !buffer->native_handle()) {
        return;
    }

    GlobalUniformData data{};
    data.viewProjection = m_viewProjection;
    const auto& lightMatrix = m_lightViewProjectionCascades[cascadeIndex];
    data.lightViewProjectionNear = lightMatrix;
    data.lightViewProjectionFar = lightMatrix;
    data.cameraPosition = m_cameraPosition;
    data.directionalDirectionIntensity = m_directionalDirectionIntensity;
    data.directionalColor = m_directionalColor;
    data.pointPositionRange = m_pointPositionRange;
    data.pointColorIntensity = m_pointColorIntensity;
    data.lightCounts = glm::vec4(
        static_cast<f32>(m_directionalLightCount),
        static_cast<f32>(m_pointLightCount),
        m_ambientIntensity,
        0.0f);
    data.shadowParams = m_shadowParams;
    data.shadowCascadeParams = m_shadowCascadeParams;
    buffer->write(&data, sizeof(GlobalUniformData), 0);
}

void ForwardRenderer::upload_object_uniforms() {
    if (!m_objectUniformBuffer || !m_objectUniformBuffer->native_handle()) {
        return;
    }
    ObjectUniformData data{};
    data.model = m_modelMatrix;
    data.normalMatrix = m_normalMatrix;
    data.baseColor = m_baseColor;
    data.emissive = m_emissive;
    data.material = m_materialParams;
    data.grid = m_gridParams;
    m_objectUniformBuffer->write(&data, sizeof(ObjectUniformData), 0);
}

} // namespace firefly

