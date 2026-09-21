#pragma once

#include "mc3/loader/memory.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace mc3::arm {

class GuestCpu {
public:
    GuestCpu();
    ~GuestCpu();

    GuestCpu(GuestCpu&&) noexcept;
    GuestCpu& operator=(GuestCpu&&) noexcept;

    GuestCpu(const GuestCpu&) = delete;
    GuestCpu& operator=(const GuestCpu&) = delete;

    bool initialize(loader::GuestMemory* memory);

    bool run(
        std::uint32_t entry,
        bool thumb,
        std::uint64_t ticks
    );

    std::uint32_t register_value(std::size_t index) const;
    void set_register(std::size_t index, std::uint32_t value);

    std::uint32_t program_counter() const;

    bool faulted() const;
    void clear_fault();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mc3::arm
