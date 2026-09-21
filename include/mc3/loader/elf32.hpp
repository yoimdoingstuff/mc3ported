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

struct ElfImport {
    std::uint32_t relocation_offset = 0;
    std::uint32_t symbol_index = 0;
    std::uint32_t relocation_type = 0;
    std::string name;
};

struct ElfSymbolInfo {
    std::uint32_t value = 0;
    std::uint32_t size = 0;
    std::uint16_t shndx = 0;
};

struct ElfDynamicInfo {
    bool present = false;
    std::uint32_t string_table_address = 0;
    std::uint32_t string_table_size = 0;
    std::uint32_t symbol_table_address = 0;
    std::uint32_t symbol_entry_size = 0;
    std::uint32_t symbol_count = 0;
    std::uint32_t relocation_address = 0;
    std::uint32_t relocation_size = 0;
    std::uint32_t relocation_entry_size = 0;
    std::uint32_t plt_relocation_address = 0;
    std::uint32_t plt_relocation_size = 0;
    std::uint32_t plt_relocation_type = 0;
    std::uint64_t relative_relocations = 0;
    std::vector<std::string> needed_libraries;
    std::vector<ElfImport> imports;
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
    std::uint32_t dynamic_file_offset = 0;
    std::uint32_t dynamic_file_size = 0;

    bool is_dynamic() const { return type == 3; }
    bool has_dynamic_section() const { return dynamic_file_size != 0; }
};

std::optional<ElfImage> parse_arm32_elf(const std::filesystem::path& path);
std::optional<ElfDynamicInfo> inspect_dynamic(const ElfImage& image);

std::optional<ElfSymbolInfo> find_dynamic_symbol(
    const ElfImage& image,
    const ElfDynamicInfo& dynamic,
    const std::string& name
);

bool apply_arm32_relocations(
    const ElfImage& image,
    const ElfDynamicInfo& dynamic,
    GuestMemory& memory,
    std::uint32_t load_bias
);

std::optional<std::uint32_t> load_arm32_elf(
    const ElfImage& image,
    GuestMemory& memory
);

} // namespace mc3::loader
