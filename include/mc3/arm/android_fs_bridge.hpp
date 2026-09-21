#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

namespace mc3::arm {

class AndroidFsBridge {
public:
    bool configure(
        const std::filesystem::path& obb_root,
        const std::filesystem::path& app_root,
        const std::filesystem::path& external_root,
        const std::filesystem::path& data_root
    );

    std::optional<std::filesystem::path> resolve(
        std::string_view guest_path
    ) const;

    bool is_read_only_path(
        std::string_view guest_path
    ) const;

private:
    std::filesystem::path obb_root_;
    std::filesystem::path app_root_;
    std::filesystem::path external_root_;
    std::filesystem::path data_root_;
};

} // namespace mc3::arm
