#include "mc3/game_data.hpp"

#include "mc3/apk_archive.hpp"
#include "mc3/log.hpp"

#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>

namespace mc3 {
namespace {

constexpr const char* kMc3Library =
    "lib/armeabi-v7a/libModernCombat3.so";

std::optional<NativeLibraryInfo> find_armv7(
    const std::filesystem::path& root
) {
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
                .abi =
                    (std::string(relative).find("armeabi-v7a") != std::string::npos)
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

bool looks_like_apk(const std::filesystem::path& path) {
    return path.has_extension() &&
           path.extension() == ".apk";
}

std::filesystem::path find_obb_root(
    const std::filesystem::path& source_root
) {
    if (has_file(source_root / "main.obb") ||
        has_file(source_root / "patch.obb") ||
        has_file(
            source_root /
            "main.1120.com.gameloft.android.ANMP.GloftM3HM.obb"
        ) ||
        has_file(
            source_root /
            "patch.11428.com.gameloft.android.ANMP.GloftM3HM.obb"
        )) {
        return source_root;
    }

    return {};
}

} // namespace

std::optional<GameDataInfo> inspect_game_data(
    const std::filesystem::path& input,
    const std::filesystem::path& materialization_root
) {
    std::error_code ec;

    if (input.empty()) {
        log_error("MC3 game-data input is empty.");
        return std::nullopt;
    }

    GameDataInfo info;

    if (std::filesystem::is_regular_file(input, ec) && looks_like_apk(input)) {
        info.input_is_apk = true;
        info.source_apk = std::filesystem::weakly_canonical(input, ec);

        if (ec) {
            info.source_apk = input;
        }

        info.root = info.source_apk.value();

        const auto parent = info.source_apk.value().parent_path();
        info.obb_root = find_obb_root(parent);
        info.looks_like_obb_root = !info.obb_root.empty();

        if (!materialization_root.empty()) {
            std::filesystem::create_directories(materialization_root, ec);
            if (ec) {
                log_error(
                    "Failed to create APK materialization directory: " +
                    materialization_root.string() +
                    " (" + ec.message() + ")"
                );
                return std::nullopt;
            }

            const auto extracted =
                materialization_root / "libModernCombat3.so";

            if (!std::filesystem::exists(extracted, ec)) {
                if (!apk_contains_entry(input, kMc3Library)) {
                    log_error(
                        "MC3 ARMv7 library is missing from the APK. Expected " +
                        std::string(kMc3Library)
                    );
                    return std::nullopt;
                }

                if (!extract_apk_entry(
                        input,
                        kMc3Library,
                        extracted
                    )) {
                    return std::nullopt;
                }
            }

            auto size = std::filesystem::file_size(extracted, ec);
            if (ec || size == 0) {
                log_error(
                    "Materialized MC3 ARMv7 library has an invalid size."
                );
                return std::nullopt;
            }

            info.armv7_library = NativeLibraryInfo{
                .path = extracted,
                .abi = "armeabi-v7a",
                .size = size
            };

            info.looks_like_apk_extract = true;
            return info;
        }

        if (!apk_contains_entry(input, kMc3Library)) {
            log_error(
                "MC3 ARMv7 library is missing from the APK. Expected " +
                std::string(kMc3Library)
            );
            return std::nullopt;
        }

        log_info("APK contains the MC3 ARMv7 library.");
        info.looks_like_apk_extract = true;
        return info;
    }

    if (!std::filesystem::exists(input, ec) ||
        !std::filesystem::is_directory(input, ec)) {
        log_error(
            "Game-data path does not exist or is not a directory: " +
            input.string()
        );
        return std::nullopt;
    }

    info.root = std::filesystem::weakly_canonical(input, ec);
    if (ec) {
        info.root = input;
    }

    info.armv7_library = find_armv7(input);
    info.obb_root = find_obb_root(input);
    info.looks_like_obb_root = !info.obb_root.empty();
    info.looks_like_apk_extract =
        has_file(input / "AndroidManifest.xml") ||
        std::filesystem::exists(input / "lib", ec);

    return info;
}

bool validate_game_data(const GameDataInfo& info) {
    if (!info.armv7_library) {
        if (info.input_is_apk) {
            log_error(
                "APK is valid but the MC3 ARM library has not been materialized."
            );
        } else {
            log_error(
                "MC3 native library was not found. Expected "
                "lib/armeabi-v7a/libModernCombat3.so."
            );
        }
        return false;
    }

    if (info.armv7_library->size == 0) {
        log_error(
            "MC3 native library exists but its size could not be read."
        );
        return false;
    }

    log_info(
        "Found MC3 ARM library: " +
        info.armv7_library->path.string()
    );
    log_info("ABI: " + info.armv7_library->abi);
    log_info(
        "Native library size: " +
        std::to_string(info.armv7_library->size) +
        " bytes"
    );

    if (info.looks_like_obb_root) {
        log_info(
            "Found MC3 OBB directory: " +
            info.obb_root.string()
        );
    } else {
        log_warning(
            "No MC3 OBB directory was detected next to the supplied game data."
        );
    }

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
