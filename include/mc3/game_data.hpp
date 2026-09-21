#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace mc3 {

struct NativeLibraryInfo {
    std::filesystem::path path;
    std::string abi;
    std::uintmax_t size = 0;
};

struct GameDataInfo {
    std::filesystem::path root;
    std::optional<std::filesystem::path> source_apk;
    std::optional<NativeLibraryInfo> armv7_library;
    std::filesystem::path obb_root;
    bool input_is_apk = false;
    bool looks_like_apk_extract = false;
    bool looks_like_obb_root = false;
};

std::optional<GameDataInfo> inspect_game_data(
    const std::filesystem::path& input,
    const std::filesystem::path& materialization_root = {}
);

bool validate_game_data(const GameDataInfo& info);

std::vector<std::filesystem::path> find_candidate_libraries(
    const std::filesystem::path& root
);

} // namespace mc3
