#pragma once

#include "mc3/loader/imports.hpp"
#include "mc3/loader/memory.hpp"

#include <cstdint>
#include <memory>
#include <string_view>

namespace mc3::arm {

class GuestCpu;

class JniBridge {
public:
    JniBridge() = default;
    ~JniBridge() = default;

    JniBridge(const JniBridge&) = delete;
    JniBridge& operator=(const JniBridge&) = delete;

    bool initialize(
        loader::GuestMemory& memory,
        loader::GuestImportTable& imports
    );

    bool handles(std::string_view symbol) const;

    void handle(
        std::string_view symbol,
        GuestCpu& guest
    );

    std::uint32_t vm_pointer() const { return vm_pointer_; }
    std::uint32_t env_pointer() const { return env_pointer_; }

private:
    std::uint32_t allocate_function_table(
        loader::GuestMemory& memory,
        loader::GuestImportTable& imports,
        const char* prefix,
        std::uint32_t count
    );

    bool write_pointer(
        loader::GuestMemory& memory,
        std::uint32_t address,
        std::uint32_t value
    );

    std::uint32_t hash_handle(
        GuestCpu& guest,
        std::uint32_t first_arg,
        std::uint32_t second_arg
    ) const;

    std::uint32_t read_guest_string_length(
        GuestCpu& guest,
        std::uint32_t address
    ) const;

    bool write_guest_u32(
        GuestCpu& guest,
        std::uint32_t address,
        std::uint32_t value
    ) const;

    std::uint32_t vm_pointer_ = 0;
    std::uint32_t env_pointer_ = 0;
    std::uint32_t vm_table_ = 0;
    std::uint32_t env_table_ = 0;
    loader::GuestMemory* memory_ = nullptr;
    loader::GuestImportTable* imports_ = nullptr;
};

} // namespace mc3::arm
