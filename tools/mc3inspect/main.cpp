#include "mc3/arm/arm_runtime.hpp"
#include "mc3/game_data.hpp"
#include <iostream>
#include <string>
namespace {
void usage() {
    std::cout << "Usage: mc3inspect <game-data-directory>\n"
              << "       mc3inspect --runtime\n";
}
}
int main(int argc, char** argv) {
    if (argc != 2) { usage(); return 2; }
    if (std::string(argv[1]) == "--runtime") {
        const auto info = mc3::arm::query_runtime();
        std::cout << "backend=" << info.backend_name << "\n"
                  << "available=" << (info.available ? "yes" : "no") << "\n"
                  << "host_arm32=" << (info.is_arm32_userspace ? "yes" : "no") << "\n";
        return 0;
    }
    const auto info = mc3::inspect_game_data(argv[1]);
    if (!info) return 1;
    std::cout << "root=" << info->root.string() << "\n"
              << "apk_extract=" << (info->looks_like_apk_extract ? "yes" : "no") << "\n"
              << "obb_root=" << (info->looks_like_obb_root ? "yes" : "no") << "\n";
    if (info->armv7_library) {
        std::cout << "armv7_library=" << info->armv7_library->path.string() << "\n"
                  << "abi=" << info->armv7_library->abi << "\n"
                  << "size=" << info->armv7_library->size << "\n";
    } else {
        std::cout << "armv7_library=missing\n";
        return 3;
    }
    return 0;
}
