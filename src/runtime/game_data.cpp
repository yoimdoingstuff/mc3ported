#include "mc3/game_data.hpp"

#include "mc3/log.hpp"

#include <algorithm>
#include <string>

namespace mc3 {

namespace {

std::optional<NativeLibraryInfo> find_armv7(const std::filesystem::path& root) {
    static constexpr const char* names[] = {
        "lib/armeabi-v7a/libModernCombat3.so",
        "lib/armeabi/libModernCombat3.so",
        "libModernCombat3.so"
    };

    for (const char* relative : names) {
        const auto path = root / relative;
        std::error_code ec;
        if (std::filesystem::is_regular_file(path, ec)) {
            const auto size = std::filesystem::file_size(path, ec);
            return NativeLibraryInfo{
                .path = path,
                .abi = (std::string(relative).find("armeabi-v7a") != std::string::npos)
                    ? "armeabi-v7a"
                    : "unknown-arm32",
                .size = ec ? 0 : size
            };
        }
    }

    return std::nullopt;
}

bool has_file(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}

} // namespace

std::optional<GameDataInfo> inspect_game_data(const std::filesystem::path& root) {
    std::error_code ec;

    if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec)) {
        log_error("Game-data path does not exist or is not a directory: " + root.string());
        return std::nullopt;
    }

    GameDataInfo info;
    info.root = std::filesystem::weakly_canonical(root, ec);
    if (ec) {
        info.root = root;
    }

    info.armv7_library = find_armv7(root);
    info.looks_like_apk_extract =
        std::filesystem::exists(root / "AndroidManifest.xml", ec) ||
        std::filesystem::exists(root / "lib", ec);

    info.looks_like_obb_root =
        has_file(root / "main.obb") ||
        has_file(root / "patch.obb");

    return info;
}

bool validate_game_data(const GameDataInfo& info) {
    if (!info.armv7_library) {
        log_error(
            "MC3 native library was not found. Expected "
            "lib/armeabi-v7a/libModernCombat3.so in extracted game data."
        );
        return false;
    }

    if (info.armv7_library->size == 0) {
        log_error("MC3 native library exists but its size could not be read.");
        return false;
    }

    log_info("Found MC3 ARM library: " + info.armv7_library->path.string());
    log_info("ABI: " + info.armv7_library->abi);
    log_info("Native library size: " + std::to_string(info.armv7_library->size) + " bytes");
    return true;
}

std::vector<std::filesystem::path> find_candidate_libraries(
    const std::filesystem::path& root
) {
    std::vector<std::filesystem::path> results;
    std::error_code ec;

    if (!std::filesystem::exists(root, ec)) {
        return results;
    }

    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(root, ec)) {
        if (ec) {
            break;
        }

        if (!entry.is_regular_file(ec)) {
            continue;
        }

        if (entry.path().filename() == "libModernCombat3.so") {
            results.push_back(entry.path());
        }
    }

    std::sort(results.begin(), results.end());
    return results;
}

} // namespace mc3
