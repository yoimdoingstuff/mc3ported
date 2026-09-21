#pragma once

#include <filesystem>
#include <string_view>

namespace mc3 {

bool extract_apk_entry(
    const std::filesystem::path& apk_path,
    std::string_view entry_name,
    const std::filesystem::path& destination
);

bool apk_contains_entry(
    const std::filesystem::path& apk_path,
    std::string_view entry_name
);

} // namespace mc3
