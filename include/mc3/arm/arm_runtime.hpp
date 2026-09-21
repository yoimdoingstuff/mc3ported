#pragma once

#include "mc3/loader/elf32.hpp"
#include "mc3/loader/imports.hpp"
#include "mc3/loader/memory.hpp"
#include "mc3/arm/jni_bridge.hpp"
#include "mc3/arm/libc_bridge.hpp"
#include "mc3/arm/dlfcn_bridge.hpp"
#include "mc3/arm/asset_bridge.hpp"
#include "mc3/arm/pthread_bridge.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mc3::arm {

class GuestCpu;

struct RuntimeInfo {
    std::string backend_name;
    bool available = false;
    bool is_arm32_userspace = false;
};

RuntimeInfo query_runtime();

class ArmRuntime {
public:
    ~ArmRuntime();

    bool initialize();

    bool configure_assets(
        const std::filesystem::path& filesystem_assets_root,
        const std::filesystem::path& apk_path,
        const std::filesystem::path& obb_root
    );

    bool configure_filesystem(
        const std::filesystem::path& obb_root,
        const std::filesystem::path& app_root,
        const std::filesystem::path& external_root,
        const std::filesystem::path& data_root
    );

    bool load_library(const std::filesystem::path& path);

    bool call_symbol(
        const std::string& symbol,
        std::span<const std::uint32_t> arguments = {}
    );

    bool call_entrypoint(const std::string& symbol);

    // Advance the real 1.1.7g game loop once. The symbol and call order are
    // taken from the public MC3 Vita loader rather than guessed.
    bool step_game();

    // Query the game's own gameplay-state export after a frame or bootstrap.
    bool gameplay_active() const;

    bool call_registered_native(
        std::string_view name,
        std::string_view signature,
        std::span<const std::uint32_t> arguments = {}
    );

    bool bootstrap_jni();

    const RuntimeInfo& info() const { return info_; }
    const loader::GuestMemory& memory() const { return memory_; }
    const loader::ElfImage* image() const { return image_ ? &*image_ : nullptr; }
    std::uint32_t load_bias() const { return load_bias_; }

private:
    RuntimeInfo info_{};
    std::filesystem::path loaded_library_;
    loader::GuestMemory memory_;
    std::optional<loader::ElfImage> image_;
    std::uint32_t load_bias_ = 0;
    std::unique_ptr<GuestCpu> cpu_;
    std::unique_ptr<loader::GuestImportTable> imports_;
    std::unique_ptr<JniBridge> jni_;
    AndroidFsBridge filesystem_;
    LibcBridge libc_;
    DlfcnBridge dlfcn_;
    AssetBridge assets_;
    PthreadBridge pthread_;
};

} // namespace mc3::arm
