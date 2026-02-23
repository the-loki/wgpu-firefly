module;
#include <cstdint>
#include <string>
#include <memory>
#include <vector>

export module firefly.resource.texture_importer;

import firefly.core.types;
import firefly.resource.manager;

export namespace firefly {

struct ImageData : public Resource {
    std::vector<u8> pixels;
    u32 width = 0;
    u32 height = 0;
    u32 channels = 0;

    auto type_name() const -> String override { return "ImageData"; }
};

class TextureImporter : public LoaderBase {
public:
    auto load(const String& path) -> SharedPtr<Resource> override;
};

} // namespace firefly
