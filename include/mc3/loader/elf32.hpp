#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace mc3::loader {

class GuestMemory;

struct ElfSegment {
    std::uint32_t file_offset = 0;
    std::uint32_t virtual_address = 0;
    std::uint32_t file_size = 0;
    std::uint32_t memory_size = 0;
    std::uint32_t flags = 0;
    std::uint32_t alignment = 0;
};

struct ElfImage {
    std::filesystem::path path;
    std::uint16_t type = 0;
    std::uint16_t machine = 0;
    std::uint32_t entry = 0;
    std::uint32_t program_header_offset = 0;
    std::uint16_t program_header_count = 0;
    std::vector<ElfSegment> load_segments;
    std::uint32_t lowest_address = 0;
    std::uint32_t highest_address = 0;

    bool is_dynamic() const { return type == 3; }
};

std::optional<ElfImage> parse_arm32_elf(const std::filesystem::path& path);

std::optional<std::uint32_t> load_arm32_elf(
    const ElfImage& image,
    GuestMemory& memory
);

} // namespace mc3::loader
