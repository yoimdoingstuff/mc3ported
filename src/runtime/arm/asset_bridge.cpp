#include "mc3/arm/asset_bridge.hpp"

#include "mc3/arm/guest_cpu.hpp"
#include "mc3/log.hpp"

#include <miniz_zip.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <optional>
#include <limits>
#include <string>

namespace mc3::arm {
namespace {

constexpr std::string_view kPackage =
    "com.gameloft.android.ANMP.GloftM3HM";

std::string normalize_asset_name(std::string_view name) {
    std::string result(name);
    std::replace(result.begin(), result.end(), '\\', '/');

    while (result.starts_with("./")) {
        result.erase(0, 2);
    }

    while (result.starts_with('/')) {
        result.erase(0, 1);
    }

    std::string safe;
    std::string component;

    for (const char character : result) {
        if (character == '/') {
            if (component == ".." || component.empty()) {
                if (component == "..") {
                    return {};
                }
            } else if (component != ".") {
                if (!safe.empty()) safe.push_back('/');
                safe += component;
            }
            component.clear();
        } else {
            component.push_back(character);
        }
    }

    if (component == "..") {
        return {};
    }

    if (!component.empty() && component != ".") {
        if (!safe.empty()) safe.push_back('/');
        safe += component;
    }

    return safe;
}

bool read_file(
    const std::filesystem::path& path,
    std::vector<std::uint8_t>& output
) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);

    if (ec || size == 0) {
        return false;
    }

    if (size > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max())) {
        return false;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    output.resize(static_cast<std::size_t>(size));
    file.read(
        reinterpret_cast<char*>(output.data()),
        static_cast<std::streamsize>(output.size())
    );

    return file.good() || file.eof();
}

} // namespace

bool AssetBridge::configure(
    loader::GuestMemory& memory,
    const std::filesystem::path& filesystem_assets_root,
    const std::filesystem::path& apk_path,
    const std::filesystem::path& obb_root
) {
    memory_ = &memory;
    filesystem_assets_root_ = filesystem_assets_root;
    apk_path_ = apk_path;
    obb_root_ = obb_root;
    return true;
}

bool AssetBridge::handles(std::string_view symbol) const {
    static constexpr std::string_view names[] = {
        "AAssetManager_fromJava",
        "AAssetManager_open",
        "AAsset_read",
        "AAsset_seek",
        "AAsset_getRemainingLength",
        "AAsset_getLength",
        "AAsset_close",
        "AAssetManager_openDir",
        "AAssetDir_close"
    };

    for (const auto name : names) {
        if (symbol == name) {
            return true;
        }
    }

    return false;
}

std::optional<std::string> AssetBridge::read_guest_string(
    std::uint32_t address
) const {
    if (!memory_ || address == 0 || !memory_->contains(address)) {
        return std::nullopt;
    }

    const auto* bytes = memory_->data(address);
    if (!bytes) {
        return std::nullopt;
    }

    constexpr std::size_t kMax = 8192;

    for (std::size_t i = 0; i < kMax; ++i) {
        const auto current =
            address + static_cast<std::uint32_t>(i);

        if (!memory_->contains(current)) {
            return std::nullopt;
        }

        if (bytes[i] == 0) {
            return std::string(
                reinterpret_cast<const char*>(bytes),
                i
            );
        }
    }

    return std::nullopt;
}

std::optional<std::vector<std::uint8_t>> AssetBridge::load_from_zip(
    const std::filesystem::path& archive,
    const std::string& entry
) const {
    std::error_code ec;
    if (archive.empty() ||
        !std::filesystem::is_regular_file(archive, ec)) {
        return std::nullopt;
    }

    mz_zip_archive zip{};
    mz_zip_zero_struct(&zip);

    const auto archive_name = archive.u8string();
    if (!mz_zip_reader_init_file(
            &zip,
            reinterpret_cast<const char*>(archive_name.c_str()),
            0
        )) {
        return std::nullopt;
    }

    std::optional<std::vector<std::uint8_t>> result;

    const int index =
        mz_zip_reader_locate_file(
            &zip,
            entry.c_str(),
            nullptr,
            MZ_ZIP_FLAG_CASE_SENSITIVE
        );

    if (index >= 0) {
        mz_zip_archive_file_stat stat{};
        if (mz_zip_reader_file_stat(
                &zip,
                static_cast<mz_uint>(index),
                &stat
            ) &&
            stat.m_uncomp_size <=
                static_cast<mz_uint64>(
                    std::numeric_limits<std::size_t>::max()
                )) {
            result.emplace(
                static_cast<std::size_t>(stat.m_uncomp_size)
            );

            if (!mz_zip_reader_extract_to_mem(
                    &zip,
                    static_cast<mz_uint>(index),
                    result->data(),
                    result->size(),
                    0
                )) {
                result.reset();
            }
        }
    }

    mz_zip_reader_end(&zip);
    return result;
}

std::optional<std::vector<std::uint8_t>> AssetBridge::load_asset(
    std::string_view filename
) const {
    const auto normalized = normalize_asset_name(filename);
    if (normalized.empty()) {
        return std::nullopt;
    }

    const auto filesystem_path =
        filesystem_assets_root_ / normalized;

    std::error_code ec;
    if (std::filesystem::is_regular_file(
            filesystem_path,
            ec
        )) {
        std::vector<std::uint8_t> data;
        if (read_file(filesystem_path, data)) {
            return data;
        }
    }

    const std::string apk_entry =
        "assets/" + normalized;

    if (!apk_path_.empty()) {
        if (auto data = load_from_zip(
                apk_path_,
                apk_entry
            )) {
            return data;
        }
    }

    if (!obb_root_.empty()) {
        const auto patch =
            obb_root_ /
            "patch.11428.com.gameloft.android.ANMP.GloftM3HM.obb";

        const auto main =
            obb_root_ /
            "main.1120.com.gameloft.android.ANMP.GloftM3HM.obb";

        const std::string obb_entry =
            "assets/" + normalized;

        if (auto data = load_from_zip(
                patch,
                obb_entry
            )) {
            return data;
        }

        if (auto data = load_from_zip(
                main,
                obb_entry
            )) {
            return data;
        }
    }

    return std::nullopt;
}

std::uint32_t AssetBridge::new_handle(
    Asset asset
) {
    while (assets_.contains(next_asset_handle_)) {
        ++next_asset_handle_;
    }

    const auto handle = next_asset_handle_++;
    assets_.emplace(handle, std::move(asset));
    return handle;
}

AssetBridge::Asset* AssetBridge::find_asset(
    std::uint32_t handle
) {
    const auto it = assets_.find(handle);
    if (it == assets_.end()) {
        return nullptr;
    }

    return &it->second;
}

void AssetBridge::handle(
    std::string_view symbol,
    GuestCpu& guest
) {
    const auto r0 = guest.register_value(0);
    const auto r1 = guest.register_value(1);
    const auto r2 = guest.register_value(2);
    const auto r3 = guest.register_value(3);

    if (symbol == "AAssetManager_fromJava") {
        guest.set_register(0, manager_handle_);
        return;
    }

    if (symbol == "AAssetManager_open") {
        const auto filename = read_guest_string(r1);
        if (!filename) {
            guest.set_register(0, 0);
            return;
        }

        auto data = load_asset(*filename);
        if (!data) {
            log_debug(
                "AAssetManager_open: asset not found: " +
                *filename
            );
            guest.set_register(0, 0);
            return;
        }

        guest.set_register(
            0,
            new_handle(Asset{
                .data = std::move(*data),
                .position = 0
            })
        );
        return;
    }

    if (symbol == "AAsset_read") {
        auto* asset = find_asset(r0);
        if (!asset ||
            !memory_ ||
            r2 == 0 ||
            asset->position >= asset->data.size() ||
            !memory_->contains(r1, r2)) {
            guest.set_register(0, 0);
            return;
        }

        const auto remaining =
            asset->data.size() - asset->position;

        const auto count =
            std::min<std::size_t>(
                remaining,
                r2
            );

        std::memcpy(
            memory_->data(r1),
            asset->data.data() + asset->position,
            count
        );

        asset->position += count;
        guest.set_register(
            0,
            static_cast<std::uint32_t>(count)
        );
        return;
    }

    if (symbol == "AAsset_seek") {
        auto* asset = find_asset(r0);
        if (!asset) {
            guest.set_register(0, static_cast<std::uint32_t>(-1));
            return;
        }

        const auto offset =
            static_cast<std::int32_t>(r1);
        const auto whence =
            static_cast<std::int32_t>(r2);

        std::int64_t base = 0;
        switch (whence) {
            case 0:
                base = 0;
                break;
            case 1:
                base =
                    static_cast<std::int64_t>(asset->position);
                break;
            case 2:
                base =
                    static_cast<std::int64_t>(asset->data.size());
                break;
            default:
                guest.set_register(
                    0,
                    static_cast<std::uint32_t>(-1)
                );
                return;
        }

        const auto next = base + offset;
        if (next < 0 ||
            next > static_cast<std::int64_t>(asset->data.size())) {
            guest.set_register(
                0,
                static_cast<std::uint32_t>(-1)
            );
            return;
        }

        asset->position = static_cast<std::size_t>(next);
        guest.set_register(
            0,
            static_cast<std::uint32_t>(asset->position)
        );
        return;
    }

    if (symbol == "AAsset_getRemainingLength") {
        auto* asset = find_asset(r0);
        if (!asset) {
            guest.set_register(0, static_cast<std::uint32_t>(-1));
            return;
        }

        guest.set_register(
            0,
            static_cast<std::uint32_t>(
                asset->data.size() - asset->position
            )
        );
        return;
    }

    if (symbol == "AAsset_getLength") {
        auto* asset = find_asset(r0);
        if (!asset) {
            guest.set_register(0, static_cast<std::uint32_t>(-1));
            return;
        }

        guest.set_register(
            0,
            static_cast<std::uint32_t>(asset->data.size())
        );
        return;
    }

    if (symbol == "AAsset_close") {
        assets_.erase(r0);
        guest.set_register(0, 0);
        return;
    }

    if (symbol == "AAssetManager_openDir") {
        guest.set_register(
            0,
            manager_handle_ + 1
        );
        return;
    }

    if (symbol == "AAssetDir_close") {
        guest.set_register(0, 0);
        return;
    }

    (void)r3;
    guest.set_register(0, 0);
}

} // namespace mc3::arm
