#pragma once

#include "mc3/loader/elf32.hpp"
#include "mc3/loader/memory.hpp"

#include <cstdint>
#include <string_view>

namespace mc3::arm {

class GuestCpu;

class DlfcnBridge {
public:
    bool configure(
        loader::GuestMemory& memory,
        const loader::ElfImage& image,
        std::uint32_t load_bias
    );

    bool handles(std::string_view symbol) const;

    void handle(
        std::string_view symbol,
        GuestCpu& guest
    );

private:
    std::string_view read_guest_string(
        std::uint32_t address
    ) const;

    loader::GuestMemory* memory_ = nullptr;
    const loader::ElfImage* image_ = nullptr;
    std::uint32_t load_bias_ = 0;
    std::uint32_t module_handle_ = 0x4D433331u;
};

} // namespace mc3::arm
