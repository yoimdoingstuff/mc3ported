#include "mc3/arm/android_fs_bridge.hpp"

#include <algorithm>
#include <string>

namespace mc3::arm {
namespace {

constexpr std::string_view kPackage =
    "com.gameloft.android.ANMP.GloftM3HM";

std::string normalize(std::string_view input) {
    std::string value(input);

    std::replace(value.begin(), value.end(), '\\', '/');

    while (value.size() >= 2 &&
           value[0] == '.' &&
           value[1] == '/') {
        value.erase(0, 2);
    }

    if (value.starts_with("file://")) {
        value.erase(0, 7);
    }

    return value;
}

bool starts_with_path(
    std::string_view value,
    std::string_view prefix
) {
    return value.starts_with(prefix);
}

std::optional<std::filesystem::path> safe_join(
    const std::filesystem::path& root,
    std::string_view relative
) {
    if (root.empty() || relative.empty()) {
        return std::nullopt;
    }

    const std::filesystem::path candidate(relative);
    if (candidate.is_absolute() || candidate.has_root_name() || candidate.has_root_directory()) {
        return std::nullopt;
    }

    for (const auto& component : candidate) {
        if (component == "..") {
            return std::nullopt;
        }
    }

    return (root / candidate).lexically_normal();
}

} // namespace

bool AndroidFsBridge::configure(
    const std::filesystem::path& obb_root,
    const std::filesystem::path& app_root,
    const std::filesystem::path& external_root,
    const std::filesystem::path& data_root
) {
    obb_root_ = obb_root;
    app_root_ = app_root;
    external_root_ = external_root;
    data_root_ = data_root;

    std::error_code ec;
    if (!app_root_.empty()) {
        std::filesystem::create_directories(app_root_, ec);
        if (ec) {
            return false;
        }
    }

    ec.clear();
    if (!external_root_.empty()) {
        std::filesystem::create_directories(external_root_, ec);
        if (ec) {
            return false;
        }
    }

    ec.clear();
    if (!data_root_.empty()) {
        std::filesystem::create_directories(data_root_, ec);
        if (ec) {
            return false;
        }
    }

    return true;
}

std::optional<std::filesystem::path> AndroidFsBridge::resolve(
    std::string_view guest_path
) const {
    if (guest_path.empty()) {
        return std::nullopt;
    }

    const auto path = normalize(guest_path);

    const std::string app_prefix =
        "/data/data/" + std::string(kPackage) + "/";

    const std::string external_prefix =
        "/sdcard/Android/data/" + std::string(kPackage) + "/files/";

    const std::string external_prefix_dot =
        "sdcard/Android/data/" + std::string(kPackage) + "/files/";

    const std::string obb_prefix =
        "/sdcard/Android/obb/" + std::string(kPackage) + "/";

    const std::string obb_prefix_dot =
        "sdcard/Android/obb/" + std::string(kPackage) + "/";

    if (starts_with_path(path, app_prefix)) {
        return safe_join(app_root_, path.substr(app_prefix.size()));
    }

    if (starts_with_path(path, external_prefix)) {
        return safe_join(external_root_, path.substr(external_prefix.size()));
    }

    if (starts_with_path(path, external_prefix_dot)) {
        return safe_join(external_root_, path.substr(external_prefix_dot.size()));
    }

    if (starts_with_path(path, obb_prefix)) {
        return safe_join(obb_root_, path.substr(obb_prefix.size()));
    }

    if (starts_with_path(path, obb_prefix_dot)) {
        return safe_join(obb_root_, path.substr(obb_prefix_dot.size()));
    }

    if (path == "main.1120.com.gameloft.android.ANMP.GloftM3HM.obb" ||
        path == "patch.11428.com.gameloft.android.ANMP.GloftM3HM.obb" ||
        path == "/main.1120.com.gameloft.android.ANMP.GloftM3HM.obb" ||
        path == "/patch.11428.com.gameloft.android.ANMP.GloftM3HM.obb") {
        return obb_root_ / std::filesystem::path(path).filename();
    }

    if (path == "sdcard" || path == "/sdcard") {
        return external_root_.parent_path();
    }

    if (!path.starts_with('/')) {
        return safe_join(data_root_, path);
    }

    return std::nullopt;
}

bool AndroidFsBridge::is_read_only_path(
    std::string_view guest_path
) const {
    const auto path = normalize(guest_path);
    return
        path.starts_with("/sdcard/Android/obb/") ||
        path.starts_with("sdcard/Android/obb/");
}

} // namespace mc3::arm
