module;
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

module firefly.platform.file_system;

namespace firefly {

auto FileSystem::read_file(const String& path) -> Result<std::vector<u8>> {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return Result<std::vector<u8>>::error("Cannot open file: " + path);
    }
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<u8> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return Result<std::vector<u8>>::error("Failed to read file: " + path);
    }
    return Result<std::vector<u8>>::ok(std::move(buffer));
}

auto FileSystem::read_text(const String& path) -> Result<String> {
    std::ifstream file(path);
    if (!file.is_open()) {
        return Result<String>::error("Cannot open file: " + path);
    }
    String content((std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>());
    return Result<String>::ok(std::move(content));
}

auto FileSystem::write_file(const String& path, const void* data, size_t size) -> Result<void> {
    auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return Result<void>::error("Cannot create file: " + path);
    }
    file.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
    return Result<void>::ok();
}

auto FileSystem::exists(const String& path) -> bool {
    return std::filesystem::exists(path);
}

auto FileSystem::is_file(const String& path) -> bool {
    return std::filesystem::is_regular_file(path);
}

auto FileSystem::is_directory(const String& path) -> bool {
    return std::filesystem::is_directory(path);
}

auto FileSystem::resolve_asset_path(const String& relativePath) -> String {
    return s_assetRoot + relativePath;
}

void FileSystem::set_asset_root(const String& root) {
    s_assetRoot = root;
    if (!s_assetRoot.empty() && s_assetRoot.back() != '/' && s_assetRoot.back() != '\\') {
        s_assetRoot += '/';
    }
}

} // namespace firefly
