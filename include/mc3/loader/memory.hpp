#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace mc3::loader {

class GuestMemory {
public:
    static constexpr std::uint32_t PageSize = 0x1000;

    bool reserve(std::uint32_t base, std::uint32_t size);
    void clear();

    std::optional<std::uint32_t> allocate_tail(
        std::size_t size,
        std::uint32_t alignment = 1
    );

    bool contains(
        std::uint32_t address,
        std::size_t size = 1
    ) const;

    std::uint8_t* data(std::uint32_t address);
    const std::uint8_t* data(std::uint32_t address) const;

    bool write(
        std::uint32_t address,
        const void* source,
        std::size_t size
    );

    bool zero(
        std::uint32_t address,
        std::size_t size
    );

    std::uint32_t base() const { return base_; }
    std::uint32_t size() const { return size_; }

private:
    std::uint32_t base_ = 0;
    std::uint32_t size_ = 0;
    std::vector<std::uint8_t> bytes_;
};

} // namespace mc3::loader
