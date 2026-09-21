#include "mc3/config.hpp"

#include "mc3/log.hpp"

#include <cstdlib>
#include <string>

namespace mc3 {
namespace {

std::filesystem::path env_path(const char* name) {
    if (const char* value = std::getenv(name); value && *value) {
        return std::filesystem::path(value);
    }
    return {};
}

void print_usage() {
    log_info(
        "Usage: mc3 --game-data <directory-or-apk> "
        "[--runtime-root <directory>] "
        "[--save-root <directory>] [--obb-root <directory>]"
    );
    log_info(
        "The game-data input may be an extracted APK directory or a "
        "Modern Combat 3 .apk containing lib/armeabi-v7a/libModernCombat3.so."
    );
}

} // namespace

std::filesystem::path default_runtime_root() {
    return std::filesystem::current_path() / "runtime";
}

std::filesystem::path default_save_root() {
    return std::filesystem::current_path() / "saves";
}

std::optional<Config> load_config(int argc, char** argv) {
    Config config{
        .game_data = env_path("MC3_GAME_DATA"),
        .obb_root = env_path("MC3_OBB_ROOT"),
        .runtime_root = env_path("MC3_RUNTIME_ROOT"),
        .save_root = env_path("MC3_SAVE_ROOT")
    };

    if (config.runtime_root.empty()) {
        config.runtime_root = default_runtime_root();
    }

    if (config.save_root.empty()) {
        config.save_root = default_save_root();
    }

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];

        auto consume_path =
            [&](const char* flag, std::filesystem::path& destination) -> bool {
                if (argument != flag) {
                    return false;
                }
                if (i + 1 >= argc) {
                    log_error(std::string("Missing value for ") + flag);
                    return true;
                }
                destination = argv[++i];
                return true;
            };

        if (consume_path("--game-data", config.game_data)) continue;
        if (consume_path("--runtime-root", config.runtime_root)) continue;
        if (consume_path("--save-root", config.save_root)) continue;
        if (consume_path("--obb-root", config.obb_root)) continue;

        if (argument == "--help" || argument == "-h") {
            print_usage();
            return std::nullopt;
        }

        log_error(std::string("Unknown argument: ") + argument);
        print_usage();
        return std::nullopt;
    }

    if (config.game_data.empty()) {
        log_error("No MC3 game-data directory or APK was provided.");
        print_usage();
        return std::nullopt;
    }

    return config;
}

} // namespace mc3
