#include "mc3/loader/elf32.hpp"

#include "mc3/loader/memory.hpp"
#include "mc3/log.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace mc3::loader {

namespace {

constexpr std::uint8_t ELFCLASS32 = 1;
constexpr std::uint8_t ELFDATA2LSB = 1;
constexpr std::uint16_t ET_EXEC = 2;
constexpr std::uint16_t ET_DYN = 3;
constexpr std::uint16_t EM_ARM = 40;

constexpr std::uint32_t PT_LOAD = 1;
constexpr std::uint32_t PT_DYNAMIC = 2;

constexpr std::uint32_t DT_NULL = 0;
constexpr std::uint32_t DT_NEEDED = 1;
constexpr std::uint32_t DT_PLTRELSZ = 2;
constexpr std::uint32_t DT_HASH = 4;
constexpr std::uint32_t DT_STRTAB = 5;
constexpr std::uint32_t DT_SYMTAB = 6;
constexpr std::uint32_t DT_RELA = 7;
constexpr std::uint32_t DT_STRSZ = 10;
constexpr std::uint32_t DT_SYMENT = 11;
constexpr std::uint32_t DT_REL = 17;
constexpr std::uint32_t DT_RELSZ = 18;
constexpr std::uint32_t DT_RELENT = 19;
constexpr std::uint32_t DT_PLTREL = 20;
constexpr std::uint32_t DT_JMPREL = 23;

constexpr std::uint32_t R_ARM_ABS32 = 2;
constexpr std::uint32_t R_ARM_GLOB_DAT = 21;
constexpr std::uint32_t R_ARM_JUMP_SLOT = 22;
constexpr std::uint32_t R_ARM_RELATIVE = 23;

constexpr std::uint16_t SHN_UNDEF = 0;
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

struct Elf32Dynamic {
    std::int32_t tag = 0;
    std::uint32_t value = 0;
};

struct Elf32Sym {
    std::uint32_t name = 0;
    std::uint32_t value = 0;
    std::uint32_t size = 0;
    std::uint8_t info = 0;
    std::uint8_t other = 0;
    std::uint16_t shndx = 0;
};

struct Elf32Rel {
    std::uint32_t offset = 0;
    std::uint32_t info = 0;
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

std::optional<std::uint32_t> virtual_to_file_offset(
    const ElfImage& image,
    std::uint32_t virtual_address
) {
    for (const auto& segment : image.load_segments) {
        if (virtual_address < segment.virtual_address) {
            continue;
        }
        const auto delta = virtual_address - segment.virtual_address;
        if (delta < segment.file_size) {
            return segment.file_offset + delta;
        }
    }
    return std::nullopt;
}

std::optional<std::string> read_dyn_string(
    std::ifstream& file,
    const ElfImage& image,
    std::uint32_t string_table,
    std::uint32_t string_table_size,
    std::uint32_t offset
) {
    if (offset >= string_table_size) {
        return std::nullopt;
    }

    const auto file_offset = virtual_to_file_offset(image, string_table + offset);
    if (!file_offset) {
        return std::nullopt;
    }

    std::string value;
    const auto max_length = string_table_size - offset;
    value.reserve(max_length);

    for (std::uint32_t i = 0; i < max_length; ++i) {
        char character = '\0';
        file.seekg(
            static_cast<std::streamoff>(*file_offset + i),
            std::ios::beg
        );
        file.read(&character, 1);
        if (file.gcount() != 1) {
            return std::nullopt;
        }
        if (character == '\0') {
            return value;
        }
        value.push_back(character);
    }

    return std::nullopt;
}

std::optional<Elf32Sym> read_symbol(
    std::ifstream& file,
    const ElfImage& image,
    std::uint32_t symbol_table,
    std::uint32_t symbol_entry_size,
    std::uint32_t symbol_index
) {
    if (symbol_entry_size < sizeof(Elf32Sym)) {
        return std::nullopt;
    }

    const auto symbol_virtual =
        symbol_table + symbol_index * symbol_entry_size;
    const auto file_offset = virtual_to_file_offset(image, symbol_virtual);
    if (!file_offset) {
        return std::nullopt;
    }

    Elf32Sym symbol{};
    file.seekg(static_cast<std::streamoff>(*file_offset), std::ios::beg);
    file.read(reinterpret_cast<char*>(&symbol), sizeof(symbol));
    if (file.gcount() != static_cast<std::streamsize>(sizeof(symbol))) {
        return std::nullopt;
    }

    return symbol;
}

bool read_rel_table(
    std::ifstream& file,
    const ElfImage& image,
    std::uint32_t address,
    std::uint32_t size,
    std::uint32_t entry_size,
    std::vector<Elf32Rel>& output
) {
    if (size == 0) {
        return true;
    }
    if (entry_size != sizeof(Elf32Rel) || size % entry_size != 0) {
        return false;
    }

    const auto file_offset = virtual_to_file_offset(image, address);
    if (!file_offset) {
        return false;
    }

    for (std::uint32_t offset = 0; offset < size; offset += entry_size) {
        Elf32Rel rel{};
        file.seekg(
            static_cast<std::streamoff>(*file_offset + offset),
            std::ios::beg
        );
        file.read(reinterpret_cast<char*>(&rel), sizeof(rel));
        if (file.gcount() != static_cast<std::streamsize>(sizeof(rel))) {
            return false;
        }
        output.push_back(rel);
    }

    return true;
}

std::optional<std::uint32_t> read_guest_u32(
    const GuestMemory& memory,
    std::uint32_t address
) {
    if (!memory.contains(address, sizeof(std::uint32_t))) {
        return std::nullopt;
    }
    std::uint32_t value = 0;
    std::memcpy(
        &value,
        memory.data(address),
        sizeof(value)
    );
    return value;
}

bool write_guest_u32(
    GuestMemory& memory,
    std::uint32_t address,
    std::uint32_t value
) {
    return memory.write(address, &value, sizeof(value));
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

    if (header.ident[4] != ELFCLASS32 ||
        header.ident[5] != ELFDATA2LSB) {
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

    const std::uint64_t program_header_bytes =
        static_cast<std::uint64_t>(header.program_header_count) *
        header.program_header_size;

    if (!checked_range(
            header.program_header_offset,
            program_header_bytes,
            file_size
        )) {
        log_error("ARM ELF program-header table is outside the file.");
        return std::nullopt;
    }

    ElfImage image{};
    image.path = path;
    image.type = header.type;
    image.machine = header.machine;
    image.entry = header.entry;
    image.program_header_offset = header.program_header_offset;
    image.program_header_count = header.program_header_count;
    image.lowest_address = std::numeric_limits<std::uint32_t>::max();

    for (std::uint16_t index = 0;
         index < header.program_header_count;
         ++index) {
        Elf32ProgramHeader program{};
        const auto offset =
            static_cast<std::uint64_t>(header.program_header_offset) +
            static_cast<std::uint64_t>(index) * header.program_header_size;

        file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        file.read(reinterpret_cast<char*>(&program), sizeof(program));
        if (file.gcount() != static_cast<std::streamsize>(sizeof(program))) {
            log_error(
                "Failed to read ARM ELF program header " +
                std::to_string(index)
            );
            return std::nullopt;
        }

        if (program.type == PT_DYNAMIC) {
            if (!checked_range(
                    program.file_offset,
                    program.file_size,
                    file_size
                )) {
                log_error("ARM ELF PT_DYNAMIC points outside the file.");
                return std::nullopt;
            }
            image.dynamic_file_offset = program.file_offset;
            image.dynamic_file_size = program.file_size;
            continue;
        }

        if (program.type != PT_LOAD) {
            continue;
        }

        if (program.file_size > program.memory_size) {
            log_error("ARM ELF PT_LOAD has file_size > memory_size.");
            return std::nullopt;
        }

        if (!checked_range(
                program.file_offset,
                program.file_size,
                file_size
            )) {
            log_error("ARM ELF PT_LOAD points outside the file.");
            return std::nullopt;
        }

        if (program.virtual_address >
            std::numeric_limits<std::uint32_t>::max() -
            program.memory_size) {
            log_error("ARM ELF PT_LOAD address overflows 32-bit space.");
            return std::nullopt;
        }

        const auto end = program.virtual_address + program.memory_size;
        const auto aligned_end = align_up(end);
        if (aligned_end == 0) {
            log_error("ARM ELF PT_LOAD end address overflows.");
            return std::nullopt;
        }

        image.lowest_address = std::min(
            image.lowest_address,
            align_down(program.virtual_address)
        );
        image.highest_address = std::max(
            image.highest_address,
            aligned_end
        );

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

std::optional<ElfDynamicInfo> inspect_dynamic(const ElfImage& image) {
    if (!image.has_dynamic_section()) {
        return ElfDynamicInfo{.present = false};
    }

    std::ifstream file(image.path, std::ios::binary);
    if (!file) {
        log_error("Failed to open ARM ELF dynamic section: " + image.path.string());
        return std::nullopt;
    }

    ElfDynamicInfo info{};
    info.present = true;

    const auto entry_count =
        image.dynamic_file_size / sizeof(Elf32Dynamic);

    for (std::uint32_t index = 0;
         index < entry_count;
         ++index) {
        Elf32Dynamic dynamic{};
        const auto offset =
            static_cast<std::uint64_t>(image.dynamic_file_offset) +
            static_cast<std::uint64_t>(index) * sizeof(Elf32Dynamic);

        file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        file.read(reinterpret_cast<char*>(&dynamic), sizeof(dynamic));

        if (file.gcount() != static_cast<std::streamsize>(sizeof(dynamic))) {
            log_error("Failed to read ARM ELF dynamic entry.");
            return std::nullopt;
        }

        switch (static_cast<std::uint32_t>(dynamic.tag)) {
            case DT_NULL:
                index = entry_count;
                break;
            case DT_STRTAB:
                info.string_table_address = dynamic.value;
                break;
            case DT_STRSZ:
                info.string_table_size = dynamic.value;
                break;
            case DT_SYMTAB:
                info.symbol_table_address = dynamic.value;
                break;
            case DT_SYMENT:
                info.symbol_entry_size = dynamic.value;
                break;
            case DT_REL:
                info.relocation_address = dynamic.value;
                break;
            case DT_RELSZ:
                info.relocation_size = dynamic.value;
                break;
            case DT_RELENT:
                info.relocation_entry_size = dynamic.value;
                break;
            case DT_JMPREL:
                info.plt_relocation_address = dynamic.value;
                break;
            case DT_PLTRELSZ:
                info.plt_relocation_size = dynamic.value;
                break;
            case DT_PLTREL:
                info.plt_relocation_type = dynamic.value;
                break;
            case DT_HASH: {
                const auto hash_offset =
                    virtual_to_file_offset(image, dynamic.value);
                if (!hash_offset) {
                    break;
                }

                std::uint32_t bucket_count = 0;
                std::uint32_t chain_count = 0;

                file.seekg(
                    static_cast<std::streamoff>(*hash_offset),
                    std::ios::beg
                );
                file.read(
                    reinterpret_cast<char*>(&bucket_count),
                    sizeof(bucket_count)
                );
                file.read(
                    reinterpret_cast<char*>(&chain_count),
                    sizeof(chain_count)
                );

                if (file.gcount() ==
                    static_cast<std::streamsize>(sizeof(chain_count))) {
                    info.symbol_count = chain_count;
                }
                break;
            }
            default:
                break;
        }
    }

    if (info.symbol_entry_size == 0) {
        info.symbol_entry_size = sizeof(Elf32Sym);
    }

    if (info.plt_relocation_size != 0 &&
        info.plt_relocation_type != DT_REL) {
        log_error("ARM ELF uses an unsupported PLT relocation format.");
        return std::nullopt;
    }

    for (std::uint32_t index = 0;
         index < entry_count;
         ++index) {
        Elf32Dynamic dynamic{};
        const auto offset =
            static_cast<std::uint64_t>(image.dynamic_file_offset) +
            static_cast<std::uint64_t>(index) * sizeof(Elf32Dynamic);

        file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        file.read(reinterpret_cast<char*>(&dynamic), sizeof(dynamic));
        if (file.gcount() != static_cast<std::streamsize>(sizeof(dynamic))) {
            return std::nullopt;
        }

        if (static_cast<std::uint32_t>(dynamic.tag) == DT_NULL) {
            break;
        }

        if (static_cast<std::uint32_t>(dynamic.tag) != DT_NEEDED) {
            continue;
        }

        const auto name = read_dyn_string(
            file,
            image,
            info.string_table_address,
            info.string_table_size,
            dynamic.value
        );
        if (name) {
            info.needed_libraries.push_back(*name);
        }
    }

    std::vector<Elf32Rel> relocations;
    const auto entry_size =
        info.relocation_entry_size == 0
            ? sizeof(Elf32Rel)
            : info.relocation_entry_size;

    if (!read_rel_table(
            file,
            image,
            info.relocation_address,
            info.relocation_size,
            entry_size,
            relocations
        ) ||
        !read_rel_table(
            file,
            image,
            info.plt_relocation_address,
            info.plt_relocation_size,
            entry_size,
            relocations
        )) {
        log_error("Failed to read ARM ELF relocation tables.");
        return std::nullopt;
    }

    std::uint32_t maximum_symbol_index = 0;
    for (const auto& relocation : relocations) {
        maximum_symbol_index = std::max(
            maximum_symbol_index,
            relocation.info >> 8
        );
    }

    if (info.symbol_count == 0) {
        info.symbol_count = maximum_symbol_index + 1;
    }

    for (const auto& relocation : relocations) {
        const auto symbol_index = relocation.info >> 8;
        const auto relocation_type = relocation.info & 0xff;

        if (relocation_type == R_ARM_RELATIVE) {
            ++info.relative_relocations;
        }

        if (symbol_index >= info.symbol_count ||
            info.symbol_table_address == 0 ||
            info.string_table_address == 0 ||
            info.string_table_size == 0) {
            continue;
        }

        const auto symbol = read_symbol(
            file,
            image,
            info.symbol_table_address,
            info.symbol_entry_size,
            symbol_index
        );

        if (!symbol || symbol->shndx != SHN_UNDEF) {
            continue;
        }

        if (relocation_type != R_ARM_ABS32 &&
            relocation_type != R_ARM_GLOB_DAT &&
            relocation_type != R_ARM_JUMP_SLOT) {
            continue;
        }

        const auto name = read_dyn_string(
            file,
            image,
            info.string_table_address,
            info.string_table_size,
            symbol->name
        );

        info.imports.push_back(ElfImport{
            .relocation_offset = relocation.offset,
            .symbol_index = symbol_index,
            .relocation_type = relocation_type,
            .name = name.value_or("<invalid-string>")
        });
    }

    return info;
}

std::optional<ElfSymbolInfo> find_dynamic_symbol(
    const ElfImage& image,
    const ElfDynamicInfo& dynamic,
    const std::string& name
) {
    if (!dynamic.present ||
        dynamic.symbol_table_address == 0 ||
        dynamic.symbol_entry_size == 0 ||
        dynamic.symbol_count == 0 ||
        dynamic.string_table_address == 0 ||
        dynamic.string_table_size == 0) {
        return std::nullopt;
    }

    std::ifstream file(image.path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }

    for (std::uint32_t index = 0;
         index < dynamic.symbol_count;
         ++index) {
        const auto symbol = read_symbol(
            file,
            image,
            dynamic.symbol_table_address,
            dynamic.symbol_entry_size,
            index
        );

        if (!symbol || symbol->shndx == SHN_UNDEF) {
            continue;
        }

        const auto symbol_name = read_dyn_string(
            file,
            image,
            dynamic.string_table_address,
            dynamic.string_table_size,
            symbol->name
        );

        if (symbol_name && *symbol_name == name) {
            return ElfSymbolInfo{
                .value = symbol->value,
                .size = symbol->size,
                .shndx = symbol->shndx
            };
        }
    }

    return std::nullopt;
}

bool apply_arm32_relocations(
    const ElfImage& image,
    const ElfDynamicInfo& dynamic,
    GuestMemory& memory,
    std::uint32_t load_bias
) {
    if (!dynamic.present) {
        return true;
    }

    std::ifstream file(image.path, std::ios::binary);
    if (!file) {
        return false;
    }

    std::vector<Elf32Rel> relocations;
    const auto entry_size =
        dynamic.relocation_entry_size == 0
            ? sizeof(Elf32Rel)
            : dynamic.relocation_entry_size;

    if (!read_rel_table(
            file,
            image,
            dynamic.relocation_address,
            dynamic.relocation_size,
            entry_size,
            relocations
        ) ||
        !read_rel_table(
            file,
            image,
            dynamic.plt_relocation_address,
            dynamic.plt_relocation_size,
            entry_size,
            relocations
        )) {
        return false;
    }

    for (const auto& relocation : relocations) {
        const auto relocation_type = relocation.info & 0xff;
        const auto symbol_index = relocation.info >> 8;
        const auto target = relocation.offset + load_bias;

        if (!memory.contains(target, sizeof(std::uint32_t))) {
            log_error("ARM relocation target is outside guest memory.");
            return false;
        }

        if (relocation_type == R_ARM_RELATIVE) {
            const auto original = read_guest_u32(memory, target);
            if (!original ||
                !write_guest_u32(
                    memory,
                    target,
                    *original + load_bias
                )) {
                return false;
            }
            continue;
        }

        if (relocation_type == R_ARM_ABS32) {
            if (symbol_index == 0) {
                log_warning("ARM ABS32 relocation has no symbol.");
            }
            continue;
        }

        if (relocation_type == R_ARM_GLOB_DAT ||
            relocation_type == R_ARM_JUMP_SLOT) {
            continue;
        }

        log_warning(
            "Unsupported ARM relocation type: " +
            std::to_string(relocation_type)
        );
    }

    return true;
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

    if (load_bias >
        std::numeric_limits<std::uint32_t>::max() -
        image.highest_address) {
        log_error("ARM ELF load bias overflows guest address space.");
        return std::nullopt;
    }

    const auto mapped_base = image.lowest_address + load_bias;
    const auto mapped_end = image.highest_address + load_bias;

    if (mapped_end <= mapped_base) {
        log_error("ARM ELF mapped range is invalid.");
        return std::nullopt;
    }

    if (!memory.reserve(
            mapped_base,
            mapped_end - mapped_base
        )) {
        return std::nullopt;
    }

    std::ifstream file(image.path, std::ios::binary);
    if (!file) {
        log_error(
            "Failed to reopen ARM ELF for segment loading: " +
            image.path.string()
        );
        memory.clear();
        return std::nullopt;
    }

    for (const auto& segment : image.load_segments) {
        const auto address = segment.virtual_address + load_bias;

        if (!memory.zero(address, segment.memory_size)) {
            log_error(
                "Failed to zero guest memory for ARM ELF segment."
            );
            memory.clear();
            return std::nullopt;
        }

        if (segment.file_size == 0) {
            continue;
        }

        std::vector<std::uint8_t> bytes(segment.file_size);
        file.seekg(
            static_cast<std::streamoff>(segment.file_offset),
            std::ios::beg
        );
        file.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size())
        );

        if (file.gcount() !=
            static_cast<std::streamsize>(bytes.size())) {
            log_error("Failed to read ARM ELF segment data.");
            memory.clear();
            return std::nullopt;
        }

        if (!memory.write(
                address,
                bytes.data(),
                bytes.size()
            )) {
            log_error(
                "Failed to copy ARM ELF segment into guest memory."
            );
            memory.clear();
            return std::nullopt;
        }
    }

    if (image.has_dynamic_section()) {
        const auto dynamic = inspect_dynamic(image);
        if (!dynamic) {
            memory.clear();
            return std::nullopt;
        }

        if (!apply_arm32_relocations(
                image,
                *dynamic,
                memory,
                load_bias
            )) {
            memory.clear();
            return std::nullopt;
        }
    }

    return load_bias;
}

} // namespace mc3::loader
