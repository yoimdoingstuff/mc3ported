#pragma once

#include "mc3/loader/memory.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mc3::arm {

class GuestCpu;

class AssetBridge {
public:
    bool configure(
        loader::GuestMemory& memory,
        const std::filesystem::path& filesystem_assets_root,
        const std::filesystem::path& apk_path,
        const std::filesystem::path& obb_root
    );

    bool handles(std::string_view symbol) const;

    void handle(
        std::string_view symbol,
        GuestCpu& guest
    );

private:
    struct Asset {
        std::vector<std::uint8_t> data;
        std::size_t position = 0;
    };

    struct AssetDirectory {
        std::vector<std::string> entries;
        std::size_t position = 0;
    };

    std::optional<std::string> read_guest_string(
        std::uint32_t address
    ) const;

    std::optional<std::vector<std::uint8_t>> load_from_zip(
        const std::filesystem::path& archive,
        const std::string& entry
    ) const;

    std::vector<std::string> list_zip_directory(
        const std::filesystem::path& archive,
        const std::string& directory
    ) const;

    std::optional<std::vector<std::uint8_t>> load_asset(
        std::string_view filename
    ) const;

    std::uint32_t new_handle(
        Asset asset
    );

    Asset* find_asset(
        std::uint32_t handle
    );

    std::uint32_t new_directory_handle(
        AssetDirectory directory
    );

    AssetDirectory* find_directory(
        std::uint32_t handle
    );

    std::uint32_t manager_handle_ = 0x41535354u;
    std::uint32_t next_asset_handle_ = 0x41530001u;
    std::uint32_t next_directory_handle_ = 0x44495201u;

    loader::GuestMemory* memory_ = nullptr;
    std::filesystem::path filesystem_assets_root_;
    std::filesystem::path apk_path_;
    std::filesystem::path obb_root_;

    std::unordered_map<std::uint32_t, Asset> assets_;
    std::unordered_map<std::uint32_t, AssetDirectory> directories_;
};

} // namespace mc3::arm
