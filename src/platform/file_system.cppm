module;
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

export module firefly.platform.file_system;

import firefly.core.types;

export namespace firefly {

class FileSystem {
public:
    static auto read_file(const String& path) -> Result<std::vector<u8>>;
    static auto read_text(const String& path) -> Result<String>;
    static auto write_file(const String& path, const void* data, size_t size) -> Result<void>;

    static auto exists(const String& path) -> bool;
    static auto is_file(const String& path) -> bool;
    static auto is_directory(const String& path) -> bool;

    static auto resolve_asset_path(const String& relativePath) -> String;
    static void set_asset_root(const String& root);

private:
    static inline String s_assetRoot = "assets/";
};

} // namespace firefly
