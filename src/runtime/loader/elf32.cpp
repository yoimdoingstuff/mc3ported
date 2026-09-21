#include "mc3/loader/elf32.hpp"

#include "mc3/loader/memory.hpp"
#include "mc3/log.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>

namespace mc3::loader {

namespace {

constexpr std::uint8_t ELFCLASS32 = 1;
constexpr std::uint8_t ELFDATA2LSB = 1;
constexpr std::uint16_t ET_EXEC = 2;
constexpr std::uint16_t ET_DYN = 3;
constexpr std::uint16_t EM_ARM = 40;
constexpr std::uint32_t PT_LOAD = 1;
constexpr std::uint32_t PAGE_SIZE = 0x1000;
constexpr std::uint32_t DEFAULT_DYNAMIC_BASE = 0x10000000;

#pragma pack(push, 1)
struct Elf32Header {
    std::array<std::uint8_t, 16> ident{};
    std::uint16_t type = 0;
    std::uint16_t machine = 0;
    std::uint32_t version = 0;
    std::uint32_t entry = 0;
    std::uint32_t program_header_offset = 0;
    std::uint32_t section_header_offset = 0;
    std::uint32_t flags = 0;
    std::uint16_t header_size = 0;
    std::uint16_t program_header_size = 0;
    std::uint16_t program_header_count = 0;
    std::uint16_t section_header_size = 0;
    std::uint16_t section_header_count = 0;
    std::uint16_t string_table_index = 0;
};

struct Elf32ProgramHeader {
    std::uint32_t type = 0;
    std::uint32_t file_offset = 0;
    std::uint32_t virtual_address = 0;
    std::uint32_t physical_address = 0;
    std::uint32_t file_size = 0;
    std::uint32_t memory_size = 0;
    std::uint32_t flags = 0;
    std::uint32_t alignment = 0;
};
#pragma pack(pop)

bool checked_range(
    std::uint64_t offset,
    std::uint64_t size,
    std::uint64_t total
) {
    return offset <= total && size <= total - offset;
}

std::uint32_t align_down(std::uint32_t value) {
    return value & ~(PAGE_SIZE - 1);
}

std::uint32_t align_up(std::uint32_t value) {
    if (value > std::numeric_limits<std::uint32_t>::max() - (PAGE_SIZE - 1)) {
        return 0;
    }
    return (value + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

} // namespace

std::optional<ElfImage> parse_arm32_elf(const std::filesystem::path& path) {
    std::error_code ec;
    const auto file_size = std::filesystem::file_size(path, ec);
    if (ec || file_size < sizeof(Elf32Header)) {
        log_error("ELF file is missing, unreadable, or too small: " + path.string());
        return std::nullopt;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        log_error("Failed to open ELF: " + path.string());
        return std::nullopt;
    }

    Elf32Header header{};
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (file.gcount() != static_cast<std::streamsize>(sizeof(header))) {
        log_error("Failed to read ELF header: " + path.string());
        return std::nullopt;
    }

    if (header.ident[0] != 0x7f ||
        header.ident[1] != 'E' ||
        header.ident[2] != 'L' ||
        header.ident[3] != 'F') {
        log_error("Not an ELF file: " + path.string());
        return std::nullopt;
    }

    if (header.ident[4] != ELFCLASS32 || header.ident[5] != ELFDATA2LSB) {
        log_error("ELF is not 32-bit little-endian: " + path.string());
        return std::nullopt;
    }

    if (header.machine != EM_ARM) {
        log_error("ELF machine is not ARM: " + path.string());
        return std::nullopt;
    }

    if (header.type != ET_EXEC && header.type != ET_DYN) {
        log_error("Unsupported ARM ELF type: " + std::to_string(header.type));
        return std::nullopt;
    }

    if (header.program_header_size != sizeof(Elf32ProgramHeader)) {
        log_error("Unexpected ARM ELF program-header size: " + path.string());
        return std::nullopt;
    }

    const std::uint64_t ph_size =
        static_cast<std::uint64_t>(header.program_header_count) *
        header.program_header_size;
    if (!checked_range(header.program_header_offset, ph_size, file_size)) {
        log_error("ARM ELF program-header table is outside the file.");
        return std::nullopt;
    }

    ElfImage image;
    image.path = path;
    image.type = header.type;
    image.machine = header.machine;
    image.entry = header.entry;
    image.program_header_offset = header.program_header_offset;
    image.program_header_count = header.program_header_count;
    image.lowest_address = std::numeric_limits<std::uint32_t>::max();

    for (std::uint16_t index = 0; index < header.program_header_count; ++index) {
        Elf32ProgramHeader program{};
        const auto offset =
            static_cast<std::uint64_t>(header.program_header_offset) +
            static_cast<std::uint64_t>(index) * header.program_header_size;

        file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        file.read(reinterpret_cast<char*>(&program), sizeof(program));
        if (file.gcount() != static_cast<std::streamsize>(sizeof(program))) {
            log_error("Failed to read ARM ELF program header " + std::to_string(index));
            return std::nullopt;
        }

        if (program.type != PT_LOAD) {
            continue;
        }

        if (program.file_size > program.memory_size) {
            log_error("ARM ELF PT_LOAD has file_size > memory_size.");
            return std::nullopt;
        }

        if (!checked_range(program.file_offset, program.file_size, file_size)) {
            log_error("ARM ELF PT_LOAD points outside the file.");
            return std::nullopt;
        }

        if (program.virtual_address >
            std::numeric_limits<std::uint32_t>::max() - program.memory_size) {
            log_error("ARM ELF PT_LOAD address overflows 32-bit space.");
            return std::nullopt;
        }

        const auto end = program.virtual_address + program.memory_size;
        const auto aligned_end = align_up(end);
        if (aligned_end == 0) {
            log_error("ARM ELF PT_LOAD end address overflows.");
            return std::nullopt;
        }

        image.lowest_address =
            std::min(image.lowest_address, align_down(program.virtual_address));
        image.highest_address = std::max(image.highest_address, aligned_end);

        image.load_segments.push_back(ElfSegment{
            .file_offset = program.file_offset,
            .virtual_address = program.virtual_address,
            .file_size = program.file_size,
            .memory_size = program.memory_size,
            .flags = program.flags,
            .alignment = program.alignment
        });
    }

    if (image.load_segments.empty()) {
        log_error("ARM ELF contains no PT_LOAD segments.");
        return std::nullopt;
    }

    if (image.highest_address <= image.lowest_address) {
        log_error("ARM ELF has an invalid load-address range.");
        return std::nullopt;
    }

    return image;
}

std::optional<std::uint32_t> load_arm32_elf(
    const ElfImage& image,
    GuestMemory& memory
) {
    const std::uint32_t load_bias =
        image.is_dynamic()
            ? DEFAULT_DYNAMIC_BASE - image.lowest_address
            : 0;

    if (image.is_dynamic() &&
        DEFAULT_DYNAMIC_BASE < image.lowest_address) {
        log_error("ARM ELF image does not fit at the selected dynamic base.");
        return std::nullopt;
    }

    if (load_bias > std::numeric_limits<std::uint32_t>::max() - image.highest_address) {
        log_error("ARM ELF load bias overflows guest address space.");
        return std::nullopt;
    }

    const auto mapped_base = image.lowest_address + load_bias;
    const auto mapped_end = image.highest_address + load_bias;
    if (mapped_end <= mapped_base) {
        log_error("ARM ELF mapped range is invalid.");
        return std::nullopt;
    }

    if (!memory.reserve(mapped_base, mapped_end - mapped_base)) {
        return std::nullopt;
    }

    std::ifstream file(image.path, std::ios::binary);
    if (!file) {
        log_error("Failed to reopen ARM ELF for segment loading: " + image.path.string());
        memory.clear();
        return std::nullopt;
    }

    for (const auto& segment : image.load_segments) {
        const auto address = segment.virtual_address + load_bias;

        if (!memory.zero(address, segment.memory_size)) {
            log_error("Failed to zero guest memory for ARM ELF segment.");
            memory.clear();
            return std::nullopt;
        }

        if (segment.file_size == 0) {
            continue;
        }

        std::vector<std::uint8_t> bytes(segment.file_size);
        file.seekg(static_cast<std::streamoff>(segment.file_offset), std::ios::beg);
        file.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size())
        );

        if (file.gcount() != static_cast<std::streamsize>(bytes.size())) {
            log_error("Failed to read ARM ELF segment data.");
            memory.clear();
            return std::nullopt;
        }

        if (!memory.write(address, bytes.data(), bytes.size())) {
            log_error("Failed to copy ARM ELF segment into guest memory.");
            memory.clear();
            return std::nullopt;
        }
    }

    return load_bias;
}

} // namespace mc3::loader
