#include "mc3/arm/arm_runtime.hpp"
#include "mc3/game_data.hpp"
#include "mc3/loader/elf32.hpp"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

void usage() {
    std::cout
        << "Usage: mc3inspect <game-data-directory>\n"
        << "       mc3inspect --elf <libModernCombat3.so>\n"
        << "       mc3inspect --runtime\n";
}

void print_elf(const mc3::loader::ElfImage& image) {
    std::cout << "type=" << (image.is_dynamic() ? "ET_DYN" : "ET_EXEC") << "\n"
              << "machine=ARM\n"
              << "entry=0x" << std::hex << image.entry << std::dec << "\n"
              << "segments=" << image.load_segments.size() << "\n"
              << "lowest=0x" << std::hex << image.lowest_address
              << " highest=0x" << image.highest_address << std::dec << "\n";

    for (std::size_t i = 0; i < image.load_segments.size(); ++i) {
        const auto& segment = image.load_segments[i];
        std::cout << "segment[" << i << "]"
                  << " offset=0x" << std::hex << segment.file_offset
                  << " vaddr=0x" << segment.virtual_address
                  << " filesz=0x" << segment.file_size
                  << " memsz=0x" << segment.memory_size
                  << " flags=0x" << segment.flags
                  << " align=0x" << segment.alignment
                  << std::dec << "\n";
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--runtime") {
        const auto info = mc3::arm::query_runtime();
        std::cout
            << "backend=" << info.backend_name << "\n"
            << "available=" << (info.available ? "yes" : "no") << "\n"
            << "host_arm32=" << (info.is_arm32_userspace ? "yes" : "no") << "\n";
        return 0;
    }

    if (argc == 3 && std::string(argv[1]) == "--elf") {
        const auto image = mc3::loader::parse_arm32_elf(argv[2]);
        if (!image) return 1;
        print_elf(*image);
        return 0;
    }

    if (argc != 2) {
        usage();
        return 2;
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
