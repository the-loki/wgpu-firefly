module;
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <mutex>
#include <string>
#include <vector>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>

#include <assimp/cimport.h>
#include <assimp/GltfMaterial.h>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include "stb/stb_image.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

module firefly.resource.model_importer;

namespace firefly {

namespace {

#ifdef _WIN32
using AssimpImportFileFn = const aiScene* (*)(const char*, unsigned int);
using AssimpReleaseImportFn = void (*)(const aiScene*);
using AssimpGetErrorStringFn = const char* (*)();
using AssimpGetMaterialColorFn =
    aiReturn (*)(const aiMaterial*, const char*, unsigned int, unsigned int, aiColor4D*);
using AssimpGetMaterialTextureCountFn = unsigned int (*)(const aiMaterial*, aiTextureType);
using AssimpGetMaterialTextureFn = aiReturn (*)(
    const aiMaterial*,
    aiTextureType,
    unsigned int,
    aiString*,
    aiTextureMapping*,
    unsigned int*,
    ai_real*,
    aiTextureOp*,
    aiTextureMapMode*,
    unsigned int*);
using AssimpGetMaterialFloatArrayFn = aiReturn (*)(
    const aiMaterial*,
    const char*,
    unsigned int,
    unsigned int,
    ai_real*,
    unsigned int*);

struct DecodedImage {
    u32 width = 0;
    u32 height = 0;
    std::vector<u8> rgba;

    [[nodiscard]] auto valid() const -> bool {
        return width > 0 && height > 0 && !rgba.empty();
    }
};

struct MaterialSampleData {
    glm::vec4 baseColorFactor{1.0f};
    glm::vec3 emissiveFactor{0.0f};
    f32 metallicFactor = 1.0f;
    f32 roughnessFactor = 1.0f;
    f32 aoFactor = 1.0f;
    f32 normalScale = 1.0f;
    DecodedImage baseColorTexture{};
    DecodedImage emissiveTexture{};
    DecodedImage metallicRoughnessTexture{};
    DecodedImage metallicTexture{};
    DecodedImage roughnessTexture{};
    DecodedImage aoTexture{};
    DecodedImage normalTexture{};
};

struct AssimpRuntime {
    HMODULE module = nullptr;
    AssimpImportFileFn importFile = nullptr;
    AssimpReleaseImportFn releaseImport = nullptr;
    AssimpGetErrorStringFn getErrorString = nullptr;
    AssimpGetMaterialColorFn getMaterialColor = nullptr;
    AssimpGetMaterialTextureCountFn getMaterialTextureCount = nullptr;
    AssimpGetMaterialTextureFn getMaterialTexture = nullptr;
    AssimpGetMaterialFloatArrayFn getMaterialFloatArray = nullptr;

    [[nodiscard]] auto is_ready() const -> bool {
        return module != nullptr && importFile != nullptr &&
            releaseImport != nullptr && getErrorString != nullptr;
    }
};

static auto load_assimp_runtime() -> const AssimpRuntime* {
    static AssimpRuntime runtime{};
    static std::once_flag initFlag;

    std::call_once(initFlag, []() {
        std::vector<String> dllCandidates = {
#ifdef FIREFLY_ASSIMP_DLL_PATH
            FIREFLY_ASSIMP_DLL_PATH,
#endif
            "assimp-vc143-mt.dll",
        };

        for (const auto& candidate : dllCandidates) {
            runtime.module = ::LoadLibraryA(candidate.c_str());
            if (runtime.module != nullptr) {
                break;
            }
        }
        if (runtime.module == nullptr) {
            return;
        }

        runtime.importFile = reinterpret_cast<AssimpImportFileFn>(
            ::GetProcAddress(runtime.module, "aiImportFile"));
        runtime.releaseImport = reinterpret_cast<AssimpReleaseImportFn>(
            ::GetProcAddress(runtime.module, "aiReleaseImport"));
        runtime.getErrorString = reinterpret_cast<AssimpGetErrorStringFn>(
            ::GetProcAddress(runtime.module, "aiGetErrorString"));
        runtime.getMaterialColor = reinterpret_cast<AssimpGetMaterialColorFn>(
            ::GetProcAddress(runtime.module, "aiGetMaterialColor"));
        runtime.getMaterialTextureCount = reinterpret_cast<AssimpGetMaterialTextureCountFn>(
            ::GetProcAddress(runtime.module, "aiGetMaterialTextureCount"));
        runtime.getMaterialTexture = reinterpret_cast<AssimpGetMaterialTextureFn>(
            ::GetProcAddress(runtime.module, "aiGetMaterialTexture"));
        runtime.getMaterialFloatArray = reinterpret_cast<AssimpGetMaterialFloatArrayFn>(
            ::GetProcAddress(runtime.module, "aiGetMaterialFloatArray"));

        if (!runtime.is_ready()) {
            ::FreeLibrary(runtime.module);
            runtime = {};
        }
    });

    return runtime.is_ready() ? &runtime : nullptr;
}
#endif

static auto safe_normalize_or_up(const glm::vec3& value) -> glm::vec3 {
    const auto len2 = glm::dot(value, value);
    if (len2 <= 1e-7f) {
        return glm::vec3(0.0f, 1.0f, 0.0f);
    }
    return glm::normalize(value);
}

#ifdef _WIN32
static auto srgb_to_linear_channel(f32 value) -> f32 {
    const f32 v = std::clamp(value, 0.0f, 1.0f);
    if (v <= 0.04045f) {
        return v / 12.92f;
    }
    return std::pow((v + 0.055f) / 1.055f, 2.4f);
}

static auto decode_image_from_memory(
    const stbi_uc* bytes,
    int sizeInBytes,
    DecodedImage& outImage) -> bool {
    if (!bytes || sizeInBytes <= 0) {
        return false;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* decoded = stbi_load_from_memory(bytes, sizeInBytes, &width, &height, &channels, 4);
    if (!decoded || width <= 0 || height <= 0) {
        if (decoded) {
            stbi_image_free(decoded);
        }
        return false;
    }

    outImage.width = static_cast<u32>(width);
    outImage.height = static_cast<u32>(height);
    outImage.rgba.assign(
        decoded,
        decoded + static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
    stbi_image_free(decoded);
    return outImage.valid();
}

static auto decode_image_from_file(const std::filesystem::path& imagePath, DecodedImage& outImage) -> bool {
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* decoded = stbi_load(imagePath.string().c_str(), &width, &height, &channels, 4);
    if (!decoded || width <= 0 || height <= 0) {
        if (decoded) {
            stbi_image_free(decoded);
        }
        return false;
    }

    outImage.width = static_cast<u32>(width);
    outImage.height = static_cast<u32>(height);
    outImage.rgba.assign(
        decoded,
        decoded + static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
    stbi_image_free(decoded);
    return outImage.valid();
}

static auto decode_embedded_texture(const aiTexture* texture, DecodedImage& outImage) -> bool {
    if (!texture) {
        return false;
    }

    if (texture->mHeight == 0) {
        const auto* bytes = reinterpret_cast<const stbi_uc*>(texture->pcData);
        return decode_image_from_memory(bytes, static_cast<int>(texture->mWidth), outImage);
    }

    if (texture->mWidth == 0 || texture->mHeight == 0 || !texture->pcData) {
        return false;
    }

    const auto width = static_cast<u32>(texture->mWidth);
    const auto height = static_cast<u32>(texture->mHeight);
    outImage.width = width;
    outImage.height = height;
    outImage.rgba.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
    for (u32 y = 0; y < height; ++y) {
        for (u32 x = 0; x < width; ++x) {
            const auto texelIndex = static_cast<size_t>(y) * static_cast<size_t>(width) + x;
            const auto& texel = texture->pcData[texelIndex];
            const auto outIndex = texelIndex * 4u;
            outImage.rgba[outIndex + 0u] = texel.r;
            outImage.rgba[outIndex + 1u] = texel.g;
            outImage.rgba[outIndex + 2u] = texel.b;
            outImage.rgba[outIndex + 3u] = texel.a;
        }
    }
    return outImage.valid();
}

static auto sample_base_color_texture(const DecodedImage& image, const glm::vec2& uv) -> glm::vec4 {
    if (!image.valid()) {
        return glm::vec4(1.0f);
    }

    const auto wrap01 = [](f32 value) -> f32 {
        const f32 wrapped = value - std::floor(value);
        return wrapped < 0.0f ? wrapped + 1.0f : wrapped;
    };

    const f32 u = wrap01(uv.x);
    const f32 v = wrap01(uv.y);
    const u32 x = static_cast<u32>(u * static_cast<f32>(image.width)) % image.width;
    const u32 y = static_cast<u32>(v * static_cast<f32>(image.height)) % image.height;
    const size_t index =
        (static_cast<size_t>(y) * static_cast<size_t>(image.width) + static_cast<size_t>(x)) * 4u;

    const auto to_linear_color = [&](size_t channelOffset) -> f32 {
        return srgb_to_linear_channel(static_cast<f32>(image.rgba[index + channelOffset]) / 255.0f);
    };
    return glm::vec4(
        to_linear_color(0u),
        to_linear_color(1u),
        to_linear_color(2u),
        std::clamp(static_cast<f32>(image.rgba[index + 3u]) / 255.0f, 0.0f, 1.0f));
}

static auto sample_linear_rgba_texture(const DecodedImage& image, const glm::vec2& uv) -> glm::vec4 {
    if (!image.valid()) {
        return glm::vec4(1.0f);
    }

    const auto wrap01 = [](f32 value) -> f32 {
        const f32 wrapped = value - std::floor(value);
        return wrapped < 0.0f ? wrapped + 1.0f : wrapped;
    };

    const f32 u = wrap01(uv.x);
    const f32 v = wrap01(uv.y);
    const u32 x = static_cast<u32>(u * static_cast<f32>(image.width)) % image.width;
    const u32 y = static_cast<u32>(v * static_cast<f32>(image.height)) % image.height;
    const size_t index =
        (static_cast<size_t>(y) * static_cast<size_t>(image.width) + static_cast<size_t>(x)) * 4u;

    const f32 r = static_cast<f32>(image.rgba[index + 0u]) / 255.0f;
    const f32 g = static_cast<f32>(image.rgba[index + 1u]) / 255.0f;
    const f32 b = static_cast<f32>(image.rgba[index + 2u]) / 255.0f;
    const f32 a = static_cast<f32>(image.rgba[index + 3u]) / 255.0f;
    return glm::vec4(
        std::clamp(r, 0.0f, 1.0f),
        std::clamp(g, 0.0f, 1.0f),
        std::clamp(b, 0.0f, 1.0f),
        std::clamp(a, 0.0f, 1.0f));
}

static auto sample_emissive_texture(const DecodedImage& image, const glm::vec2& uv) -> glm::vec3 {
    const auto srgb = sample_base_color_texture(image, uv);
    return glm::vec3(srgb.r, srgb.g, srgb.b);
}

static auto sample_normal_texture(const DecodedImage& image, const glm::vec2& uv) -> glm::vec3 {
    const auto normalSample = sample_linear_rgba_texture(image, uv);
    glm::vec3 tangentNormal = glm::vec3(normalSample) * 2.0f - glm::vec3(1.0f);
    const auto len2 = glm::dot(tangentNormal, tangentNormal);
    if (len2 <= 1e-7f) {
        return glm::vec3(0.0f, 0.0f, 1.0f);
    }
    return glm::normalize(tangentNormal);
}

static auto transform_tangent_normal_to_model_space(
    const glm::vec3& baseNormal,
    const glm::vec3& tangent,
    const glm::vec3& bitangent,
    glm::vec3 tangentNormal,
    f32 normalScale) -> glm::vec3 {
    const auto n = safe_normalize_or_up(baseNormal);
    glm::vec3 t = tangent - n * glm::dot(n, tangent);
    const auto tangentLen2 = glm::dot(t, t);
    if (tangentLen2 <= 1e-7f) {
        return n;
    }
    t = glm::normalize(t);

    glm::vec3 b = bitangent - n * glm::dot(n, bitangent) - t * glm::dot(t, bitangent);
    const auto bitangentLen2 = glm::dot(b, b);
    if (bitangentLen2 <= 1e-7f) {
        b = glm::cross(n, t);
    } else {
        b = glm::normalize(b);
    }

    if (glm::dot(glm::cross(n, t), b) < 0.0f) {
        b = -b;
    }

    tangentNormal.x *= normalScale;
    tangentNormal.y *= normalScale;
    tangentNormal = safe_normalize_or_up(tangentNormal);

    const glm::vec3 mappedNormal = tangentNormal.x * t + tangentNormal.y * b + tangentNormal.z * n;
    return safe_normalize_or_up(mappedNormal);
}

static auto read_material_float(
    const AssimpRuntime& runtime,
    const aiMaterial* material,
    const char* key,
    unsigned int type,
    unsigned int index,
    f32 defaultValue) -> f32 {
    if (!material || !runtime.getMaterialFloatArray) {
        return defaultValue;
    }

    ai_real value = static_cast<ai_real>(defaultValue);
    unsigned int count = 1u;
    const auto result = runtime.getMaterialFloatArray(material, key, type, index, &value, &count);
    if (result != aiReturn_SUCCESS || count == 0u) {
        return defaultValue;
    }
    return static_cast<f32>(value);
}

static auto try_load_material_texture(
    const AssimpRuntime& runtime,
    const aiScene* scene,
    const aiMaterial* material,
    aiTextureType textureType,
    const std::filesystem::path& modelDirectory,
    DecodedImage& outImage) -> bool {
    if (!scene || !material || !runtime.getMaterialTextureCount || !runtime.getMaterialTexture) {
        return false;
    }
    if (runtime.getMaterialTextureCount(material, textureType) == 0u) {
        return false;
    }

    aiString texturePath{};
    const auto getTextureResult = runtime.getMaterialTexture(
        material,
        textureType,
        0,
        &texturePath,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr);
    if (getTextureResult != aiReturn_SUCCESS) {
        return false;
    }

    const String texturePathString = texturePath.C_Str();
    if (texturePathString.empty()) {
        return false;
    }

    if (texturePathString.front() == '*') {
        char* parseEnd = nullptr;
        const long textureIndex = std::strtol(texturePathString.c_str() + 1, &parseEnd, 10);
        if (parseEnd == texturePathString.c_str() + 1 || textureIndex < 0) {
            return false;
        }
        if (static_cast<unsigned long>(textureIndex) >= scene->mNumTextures) {
            return false;
        }
        return decode_embedded_texture(scene->mTextures[textureIndex], outImage);
    }

    auto resolvedPath = std::filesystem::path(texturePathString);
    if (resolvedPath.is_relative()) {
        resolvedPath = modelDirectory / resolvedPath;
    }
    if (!std::filesystem::exists(resolvedPath) && !modelDirectory.empty()) {
        resolvedPath = modelDirectory / std::filesystem::path(texturePathString).filename();
    }
    if (!std::filesystem::exists(resolvedPath)) {
        return false;
    }
    return decode_image_from_file(resolvedPath, outImage);
}

static auto load_material_sample_data(
    const AssimpRuntime& runtime,
    const aiScene* scene,
    const aiMaterial* material,
    const std::filesystem::path& modelDirectory) -> MaterialSampleData {
    MaterialSampleData data{};
    if (!material) {
        return data;
    }

    if (runtime.getMaterialColor) {
        aiColor4D color{};
        if (runtime.getMaterialColor(material, AI_MATKEY_BASE_COLOR, &color) == aiReturn_SUCCESS ||
            runtime.getMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &color) == aiReturn_SUCCESS) {
            data.baseColorFactor = glm::vec4(
                std::clamp(color.r, 0.0f, 1.0f),
                std::clamp(color.g, 0.0f, 1.0f),
                std::clamp(color.b, 0.0f, 1.0f),
                std::clamp(color.a, 0.0f, 1.0f));
        }

        aiColor4D emissive{};
        if (runtime.getMaterialColor(material, AI_MATKEY_COLOR_EMISSIVE, &emissive) == aiReturn_SUCCESS) {
            data.emissiveFactor = glm::clamp(
                glm::vec3(emissive.r, emissive.g, emissive.b),
                glm::vec3(0.0f),
                glm::vec3(32.0f));
        }
    }

    data.metallicFactor = std::clamp(
        read_material_float(runtime, material, AI_MATKEY_METALLIC_FACTOR, 1.0f),
        0.0f,
        1.0f);
    data.roughnessFactor = std::clamp(
        read_material_float(runtime, material, AI_MATKEY_ROUGHNESS_FACTOR, 1.0f),
        0.0f,
        1.0f);
    const f32 emissiveIntensity = std::max(
        0.0f,
        read_material_float(runtime, material, AI_MATKEY_EMISSIVE_INTENSITY, 1.0f));
    data.emissiveFactor *= emissiveIntensity;
#ifdef AI_MATKEY_GLTF_TEXTURE_SCALE
    data.normalScale = std::max(
        0.0f,
        read_material_float(runtime, material, AI_MATKEY_GLTF_TEXTURE_SCALE(aiTextureType_NORMALS, 0), 1.0f));
#endif
#ifdef AI_MATKEY_GLTF_TEXTURE_STRENGTH
    data.aoFactor = std::clamp(
        data.aoFactor *
            read_material_float(
                runtime,
                material,
                AI_MATKEY_GLTF_TEXTURE_STRENGTH(aiTextureType_AMBIENT_OCCLUSION, 0),
                1.0f),
        0.0f,
        1.0f);
#endif

    if (!try_load_material_texture(
            runtime,
            scene,
            material,
            aiTextureType_BASE_COLOR,
            modelDirectory,
            data.baseColorTexture)) {
        (void)try_load_material_texture(
            runtime,
            scene,
            material,
            aiTextureType_DIFFUSE,
            modelDirectory,
            data.baseColorTexture);
    }

    (void)try_load_material_texture(
        runtime,
        scene,
        material,
            aiTextureType_EMISSIVE,
            modelDirectory,
            data.emissiveTexture);
    (void)try_load_material_texture(
        runtime,
        scene,
        material,
        aiTextureType_GLTF_METALLIC_ROUGHNESS,
        modelDirectory,
        data.metallicRoughnessTexture);
    (void)try_load_material_texture(
        runtime,
        scene,
        material,
        aiTextureType_METALNESS,
        modelDirectory,
        data.metallicTexture);
    (void)try_load_material_texture(
        runtime,
        scene,
        material,
        aiTextureType_DIFFUSE_ROUGHNESS,
        modelDirectory,
        data.roughnessTexture);
    if (!try_load_material_texture(
            runtime,
            scene,
            material,
            aiTextureType_AMBIENT_OCCLUSION,
            modelDirectory,
            data.aoTexture)) {
        (void)try_load_material_texture(
            runtime,
            scene,
            material,
            aiTextureType_LIGHTMAP,
            modelDirectory,
            data.aoTexture);
    }
    if (!try_load_material_texture(
            runtime,
            scene,
            material,
            aiTextureType_NORMALS,
            modelDirectory,
            data.normalTexture)) {
        (void)try_load_material_texture(
            runtime,
            scene,
            material,
            aiTextureType_HEIGHT,
            modelDirectory,
            data.normalTexture);
    }
    return data;
}
#endif

} // namespace

auto ModelData::total_vertex_count() const -> u32 {
    u64 total = 0;
    for (const auto& mesh : meshes) {
        total += static_cast<u64>(mesh.vertices.size());
    }
    return static_cast<u32>(std::min<u64>(total, std::numeric_limits<u32>::max()));
}

auto ModelData::total_index_count() const -> u32 {
    u64 total = 0;
    for (const auto& mesh : meshes) {
        total += static_cast<u64>(mesh.indices.size());
    }
    return static_cast<u32>(std::min<u64>(total, std::numeric_limits<u32>::max()));
}

auto flatten_model(const ModelData& model) -> FlattenedModelData {
    FlattenedModelData flattened{};
    flattened.vertices.reserve(model.total_vertex_count());
    flattened.indices.reserve(model.total_index_count());

    u64 vertexOffset = 0;
    for (const auto& mesh : model.meshes) {
        flattened.vertices.insert(
            flattened.vertices.end(),
            mesh.vertices.begin(),
            mesh.vertices.end());

        for (const auto index : mesh.indices) {
            const auto shifted = vertexOffset + static_cast<u64>(index);
            if (shifted > std::numeric_limits<u32>::max()) {
                continue;
            }
            flattened.indices.push_back(static_cast<u32>(shifted));
        }

        vertexOffset += static_cast<u64>(mesh.vertices.size());
    }
    return flattened;
}

auto ModelImporter::load(const String& path) -> SharedPtr<Resource> {
#ifdef _WIN32
    const auto* runtime = load_assimp_runtime();
    if (runtime == nullptr) {
        return nullptr;
    }

    constexpr unsigned int postProcessFlags =
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_ImproveCacheLocality |
        aiProcess_PreTransformVertices;

    const aiScene* scene = runtime->importFile(path.c_str(), postProcessFlags);
    if (scene == nullptr || !scene->HasMeshes()) {
        return nullptr;
    }

    const std::filesystem::path modelDirectory = std::filesystem::path(path).parent_path();
    std::vector<MaterialSampleData> materialSamples;
    if (scene->HasMaterials()) {
        materialSamples.reserve(scene->mNumMaterials);
        for (unsigned int materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex) {
            const aiMaterial* material = scene->mMaterials[materialIndex];
            materialSamples.push_back(
                load_material_sample_data(*runtime, scene, material, modelDirectory));
        }
    }

    auto model = std::make_shared<ModelData>();
    bool hasBounds = false;

    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        const aiMesh* mesh = scene->mMeshes[meshIndex];
        if (mesh == nullptr || !mesh->HasPositions() || mesh->mNumVertices == 0) {
            continue;
        }
        const bool hasTexCoords = mesh->HasTextureCoords(0);
        const bool hasTangents = mesh->HasTangentsAndBitangents();
        const MaterialSampleData* materialSample = nullptr;
        if (mesh->mMaterialIndex < materialSamples.size()) {
            materialSample = &materialSamples[mesh->mMaterialIndex];
        }

        ModelMeshData output{};
        if (mesh->mName.length > 0) {
            output.name = mesh->mName.C_Str();
        }

        output.vertices.reserve(mesh->mNumVertices);
        for (unsigned int vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
            const auto& pos = mesh->mVertices[vertexIndex];
            ModelVertex vertex{};
            vertex.position = glm::vec3(pos.x, pos.y, pos.z);

            if (mesh->HasNormals()) {
                const auto& normal = mesh->mNormals[vertexIndex];
                vertex.normal = safe_normalize_or_up(glm::vec3(normal.x, normal.y, normal.z));
            }

            if (hasTexCoords) {
                const auto& uv = mesh->mTextureCoords[0][vertexIndex];
                vertex.texCoord = glm::vec2(uv.x, uv.y);
            }
            if (materialSample) {
                if (hasTexCoords && hasTangents && materialSample->normalTexture.valid()) {
                    const auto normalSample = sample_normal_texture(materialSample->normalTexture, vertex.texCoord);
                    const auto& tangent = mesh->mTangents[vertexIndex];
                    const auto& bitangent = mesh->mBitangents[vertexIndex];
                    vertex.normal = transform_tangent_normal_to_model_space(
                        vertex.normal,
                        glm::vec3(tangent.x, tangent.y, tangent.z),
                        glm::vec3(bitangent.x, bitangent.y, bitangent.z),
                        normalSample,
                        materialSample->normalScale);
                }

                auto color = materialSample->baseColorFactor;
                if (hasTexCoords && materialSample->baseColorTexture.valid()) {
                    color *= sample_base_color_texture(materialSample->baseColorTexture, vertex.texCoord);
                }
                vertex.color = glm::clamp(color, glm::vec4(0.0f), glm::vec4(1.0f));

                glm::vec3 emissive = materialSample->emissiveFactor;
                if (hasTexCoords && materialSample->emissiveTexture.valid()) {
                    emissive *= sample_emissive_texture(materialSample->emissiveTexture, vertex.texCoord);
                }
                vertex.emissive = glm::clamp(emissive, glm::vec3(0.0f), glm::vec3(32.0f));

                firefly::f32 metallic = materialSample->metallicFactor;
                if (hasTexCoords && materialSample->metallicRoughnessTexture.valid()) {
                    const auto metallicRoughnessSample = sample_linear_rgba_texture(
                        materialSample->metallicRoughnessTexture,
                        vertex.texCoord);
                    metallic *= metallicRoughnessSample.b;
                } else if (hasTexCoords && materialSample->metallicTexture.valid()) {
                    const auto metallicSample =
                        sample_linear_rgba_texture(materialSample->metallicTexture, vertex.texCoord);
                    metallic *= metallicSample.b;
                }
                vertex.metallic = std::clamp(metallic, 0.0f, 1.0f);

                firefly::f32 roughness = materialSample->roughnessFactor;
                if (hasTexCoords && materialSample->metallicRoughnessTexture.valid()) {
                    const auto metallicRoughnessSample = sample_linear_rgba_texture(
                        materialSample->metallicRoughnessTexture,
                        vertex.texCoord);
                    roughness *= metallicRoughnessSample.g;
                } else if (hasTexCoords && materialSample->roughnessTexture.valid()) {
                    const auto roughnessSample =
                        sample_linear_rgba_texture(materialSample->roughnessTexture, vertex.texCoord);
                    roughness *= roughnessSample.g;
                }
                vertex.roughness = std::clamp(roughness, 0.045f, 1.0f);

                firefly::f32 ao = materialSample->aoFactor;
                if (hasTexCoords && materialSample->aoTexture.valid()) {
                    const auto aoSample = sample_linear_rgba_texture(materialSample->aoTexture, vertex.texCoord);
                    ao *= aoSample.r;
                }
                vertex.ao = std::clamp(ao, 0.0f, 1.0f);
            }

            output.vertices.push_back(vertex);

            if (!hasBounds) {
                model->boundingMin = vertex.position;
                model->boundingMax = vertex.position;
                hasBounds = true;
            } else {
                model->boundingMin = glm::min(model->boundingMin, vertex.position);
                model->boundingMax = glm::max(model->boundingMax, vertex.position);
            }
        }

        output.indices.reserve(static_cast<size_t>(mesh->mNumFaces) * 3u);
        for (unsigned int faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
            const auto& face = mesh->mFaces[faceIndex];
            if (face.mNumIndices != 3) {
                continue;
            }
            output.indices.push_back(static_cast<u32>(face.mIndices[0]));
            output.indices.push_back(static_cast<u32>(face.mIndices[1]));
            output.indices.push_back(static_cast<u32>(face.mIndices[2]));
        }

        if (!output.vertices.empty() && !output.indices.empty()) {
            model->meshes.push_back(std::move(output));
        }
    }

    runtime->releaseImport(scene);

    if (model->meshes.empty()) {
        return nullptr;
    }

    if (!hasBounds) {
        model->boundingMin = glm::vec3(0.0f);
        model->boundingMax = glm::vec3(0.0f);
    }

    return model;
#else
    (void)path;
    return nullptr;
#endif
}

} // namespace firefly
