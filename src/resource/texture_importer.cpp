module;
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#include <string>
#include <memory>
#include <vector>

module firefly.resource.texture_importer;

namespace firefly {

auto TextureImporter::load(const String& path) -> SharedPtr<Resource> {
    int w, h, ch;
    auto* data = stbi_load(path.c_str(), &w, &h, &ch, 4);
    if (!data) return nullptr;

    auto img = std::make_shared<ImageData>();
    img->width = static_cast<u32>(w);
    img->height = static_cast<u32>(h);
    img->channels = 4;
    img->pixels.assign(data, data + w * h * 4);
    stbi_image_free(data);
    return img;
}

} // namespace firefly
