#pragma once

#include "mc3/loader/elf32.hpp"
#include "mc3/loader/memory.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mc3::loader {

using SymbolResolver =
    std::function<std::optional<std::uint32_t>(std::string_view)>;

class GuestImportTable {
public:
    explicit GuestImportTable(GuestMemory& memory);

    std::optional<std::uint32_t> bind(
        std::string_view symbol
    );

    std::optional<std::string> symbol_for_slot(
        std::uint32_t slot
    ) const;

    std::size_t size() const { return bindings_.size(); }

private:
    struct Binding {
        std::uint32_t slot = 0;
        std::uint32_t address = 0;
        std::string symbol;
    };

    std::optional<std::uint32_t> allocate_stub(
        std::uint32_t slot
    );

    GuestMemory& memory_;
    std::uint32_t next_slot_ = 1;
    std::unordered_map<std::string, std::uint32_t> by_symbol_;
    std::vector<Binding> bindings_;
};

bool patch_arm32_import_relocations(
    const ElfImage& image,
    const ElfDynamicInfo& dynamic,
    GuestMemory& memory,
    std::uint32_t load_bias,
    const SymbolResolver& resolver
);

} // namespace mc3::loader
