#include "mc3/arm/arm_runtime.hpp"
#include "mc3/game_data.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
int main() {
    const auto temp = std::filesystem::temp_directory_path() / "mc3ported-runtime-test";
    std::error_code ec;
    std::filesystem::remove_all(temp, ec);
    std::filesystem::create_directories(temp / "lib" / "armeabi-v7a", ec);
    assert(!ec);
    {
        std::ofstream library(temp / "lib" / "armeabi-v7a" / "libModernCombat3.so", std::ios::binary);
        library << "placeholder";
    }
    const auto info = mc3::inspect_game_data(temp);
    assert(info.has_value());
    assert(info->armv7_library.has_value());
    assert(info->armv7_library->abi == "armeabi-v7a");
    assert(mc3::validate_game_data(*info));
    const auto runtime = mc3::arm::query_runtime();
    assert(!runtime.backend_name.empty());
    std::filesystem::remove_all(temp, ec);
    return 0;
}
