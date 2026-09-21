#include "mc3/arm/arm_runtime.hpp"
#include "mc3/config.hpp"
#include "mc3/game_data.hpp"
#include "mc3/log.hpp"

#include <filesystem>

int main(int argc, char** argv) {
    mc3::log_info("MC3 Ported runtime bootstrap");
    mc3::log_info("M1: game-data discovery + ARM runtime interface");

    const auto config = mc3::load_config(argc, argv);
    if (!config) {
        return 2;
    }

    std::error_code ec;
    std::filesystem::create_directories(config->runtime_root, ec);
    if (ec) {
        mc3::log_error(
            "Failed to create runtime directory: " +
            config->runtime_root.string() +
            " (" + ec.message() + ")"
        );
        return 3;
    }

    std::filesystem::create_directories(config->save_root, ec);
    if (ec) {
        mc3::log_error(
            "Failed to create save directory: " +
            config->save_root.string() +
            " (" + ec.message() + ")"
        );
        return 3;
    }

    const auto data = mc3::inspect_game_data(
        config->game_data,
        config->runtime_root / "apk-cache"
    );

    if (!data || !mc3::validate_game_data(*data)) {
        mc3::log_error("MC3 game-data validation failed.");
        return 4;
    }

    mc3::arm::ArmRuntime arm_runtime;

    const auto obb_root =
        data->looks_like_obb_root
            ? data->obb_root
            : std::filesystem::path{};

    if (!arm_runtime.configure_filesystem(
            obb_root,
            config->save_root / "appdata",
            config->save_root / "external",
            config->runtime_root / "data"
        )) {
        mc3::log_error("Android filesystem configuration failed.");
        return 5;
    }
    if (!arm_runtime.initialize()) {
        mc3::log_error("ARM runtime initialization failed.");
        return 5;
    }

    if (data->armv7_library &&
        !arm_runtime.load_library(data->armv7_library->path)) {
        mc3::log_error(
            "MC3 ARM library could not be loaded into the guest runtime."
        );
        return 6;
    }

    if (!arm_runtime.bootstrap_jni()) {
        mc3::log_error("MC3 JNI bootstrap failed.");
        return 7;
    }

    mc3::log_info(
        "Bootstrap finished. No Android installation was required."
    );
    return 0;
}
