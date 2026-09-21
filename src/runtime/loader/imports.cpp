#include "mc3/loader/imports.hpp"

#include "mc3/log.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <unordered_map>

namespace mc3::loader {

namespace {

constexpr std::uint32_t R_ARM_ABS32 = 2;
constexpr std::uint32_t R_ARM_GLOB_DAT = 21;
constexpr std::uint32_t R_ARM_JUMP_SLOT = 22;
constexpr std::uint32_t PT_DYNAMIC = 2;

#pragma pack(push, 1)
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

bool write_u32(
    GuestMemory& memory,
    std::uint32_t address,
    std::uint32_t value
) {
    return memory.write(address, &value, sizeof(value));
}

std::uint32_t movw_r12(std::uint16_t value) {
    return
        0xE3000000u |
        ((static_cast<std::uint32_t>(value) & 0xF000u) << 4) |
        (12u << 12) |
        (static_cast<std::uint32_t>(value) & 0x0FFFu);
}

std::uint32_t svc(std::uint8_t immediate) {
    return 0xEF000000u |
           static_cast<std::uint32_t>(immediate);
}

std::uint32_t bx_lr() {
    return 0xE12FFF1Eu;
}

std::optional<std::uint32_t> allocate_guest_bytes(
    GuestMemory& memory,
    std::size_t size
) {
    return memory.allocate_tail(size, 4);
}

} // namespace

GuestImportTable::GuestImportTable(GuestMemory& memory)
    : memory_(memory) {}

std::optional<std::uint32_t> GuestImportTable::allocate_stub(
    std::uint32_t slot
) {
    if (slot == 0 || slot > 0xFFFFu) {
        log_error("Guest import slot space exhausted.");
        return std::nullopt;
    }

    const auto address =
        allocate_guest_bytes(memory_, 12);

    if (!address) {
        return std::nullopt;
    }

    const std::array<std::uint32_t, 3> instructions = {
        movw_r12(static_cast<std::uint16_t>(slot)),
        svc(0xFF),
        bx_lr()
    };

    for (std::size_t i = 0; i < instructions.size(); ++i) {
        if (!write_u32(
                memory_,
                *address + static_cast<std::uint32_t>(i * 4),
                instructions[i]
            )) {
            return std::nullopt;
        }
    }

    return address;
}

std::optional<std::uint32_t> GuestImportTable::bind(
    std::string_view symbol
) {
    const std::string name(symbol);

    if (const auto existing = by_symbol_.find(name);
        existing != by_symbol_.end()) {
        return existing->second;
    }

    const auto slot = next_slot_++;
    const auto address = allocate_stub(slot);
    if (!address) {
        return std::nullopt;
    }

    by_symbol_.emplace(name, *address);
    bindings_.push_back(Binding{
        .slot = slot,
        .address = *address,
        .symbol = name
    });

    return *address;
}

std::optional<std::string> GuestImportTable::symbol_for_slot(
    std::uint32_t slot
) const {
    for (const auto& binding : bindings_) {
        if (binding.slot == slot) {
            return binding.symbol;
        }
    }
    return std::nullopt;
}

bool patch_arm32_import_relocations(
    const ElfImage& image,
    const ElfDynamicInfo& dynamic,
    GuestMemory& memory,
    std::uint32_t load_bias,
    const SymbolResolver& resolver
) {
    if (!resolver) {
        return true;
    }

    std::unordered_map<std::uint32_t, std::string> import_names;
    import_names.reserve(dynamic.imports.size());

    for (const auto& import : dynamic.imports) {
        import_names.emplace(import.relocation_offset, import.name);
    }

    std::ifstream file(image.path, std::ios::binary);
    if (!file) {
        log_error(
            "Failed to open ARM ELF while patching imported symbols: " +
            image.path.string()
        );
        return false;
    }

    // Reuse the same relocation format used by the main ELF loader.
    const auto entry_size =
        dynamic.relocation_entry_size == 0
            ? sizeof(Elf32Rel)
            : dynamic.relocation_entry_size;

    if (entry_size != sizeof(Elf32Rel)) {
        log_error("Unsupported ARM relocation entry size.");
        return false;
    }

    auto patch_table =
        [&](std::uint32_t address, std::uint32_t size) -> bool {
            if (size == 0) {
                return true;
            }

            const auto file_offset =
                [&]() -> std::optional<std::uint32_t> {
                    for (const auto& segment : image.load_segments) {
                        if (address < segment.virtual_address) {
                            continue;
                        }

                        const auto delta =
                            address - segment.virtual_address;

                        if (delta < segment.file_size) {
                            return segment.file_offset + delta;
                        }
                    }
                    return std::nullopt;
                }();

            if (!file_offset || size % entry_size != 0) {
                return false;
            }

            for (std::uint32_t offset = 0;
                 offset < size;
                 offset += entry_size) {
                Elf32Rel rel{};
                file.seekg(
                    static_cast<std::streamoff>(*file_offset + offset),
                    std::ios::beg
                );
                file.read(
                    reinterpret_cast<char*>(&rel),
                    sizeof(rel)
                );

                if (file.gcount() !=
                    static_cast<std::streamsize>(sizeof(rel))) {
                    return false;
                }

                const auto type = rel.info & 0xFFu;
                if (type != R_ARM_ABS32 &&
                    type != R_ARM_GLOB_DAT &&
                    type != R_ARM_JUMP_SLOT) {
                    continue;
                }

                const auto name_it =
                    import_names.find(rel.offset);

                if (name_it == import_names.end()) {
                    continue;
                }

                const auto target =
                    resolver(name_it->second);

                if (!target) {
                    log_error(
                        "Unresolved ARM import: " +
                        name_it->second
                    );
                    return false;
                }

                const auto guest_target =
                    rel.offset + load_bias;

                if (!write_u32(
                        memory,
                        guest_target,
                        *target
                    )) {
                    log_error(
                        "Failed to patch ARM import relocation for " +
                        name_it->second
                    );
                    return false;
                }
            }

            return true;
        };

    return
        patch_table(
            dynamic.relocation_address,
            dynamic.relocation_size
        ) &&
        patch_table(
            dynamic.plt_relocation_address,
            dynamic.plt_relocation_size
        );
}

} // namespace mc3::loader
