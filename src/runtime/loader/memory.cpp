#include "mc3/loader/memory.hpp"

#include "mc3/log.hpp"

#include <cstring>
#include <limits>
#include <string>

namespace mc3::loader {

bool GuestMemory::reserve(
    std::uint32_t base,
    std::uint32_t size
) {
    if (size == 0 ||
        base > std::numeric_limits<std::uint32_t>::max() - size) {
        return false;
    }

    try {
        bytes_.assign(size, 0);
    } catch (...) {
        log_error(
            "Unable to allocate guest memory: " +
            std::to_string(size) + " bytes"
        );
        return false;
    }

    base_ = base;
    size_ = size;
    return true;
}

void GuestMemory::clear() {
    bytes_.clear();
    base_ = 0;
    size_ = 0;
}

std::optional<std::uint32_t> GuestMemory::allocate_tail(
    std::size_t size,
    std::uint32_t alignment
) {
    if (size == 0 || alignment == 0) {
        return std::nullopt;
    }

    const auto current =
        static_cast<std::uint64_t>(base_) + size_;

    const auto aligned =
        (current + alignment - 1) &
        ~static_cast<std::uint64_t>(alignment - 1);

    const auto end =
        aligned + size;

    if (aligned > std::numeric_limits<std::uint32_t>::max() ||
        end > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1ull) {
        return std::nullopt;
    }

    const auto new_size =
        end - base_;

    if (base_ == 0 && size_ == 0) {
        return std::nullopt;
    }

    try {
        bytes_.resize(static_cast<std::size_t>(new_size), 0);
    } catch (...) {
        log_error(
            "Unable to grow guest memory by " +
            std::to_string(size) + " bytes"
        );
        return std::nullopt;
    }

    size_ = static_cast<std::uint32_t>(new_size);
    return static_cast<std::uint32_t>(aligned);
}

bool GuestMemory::contains(
    std::uint32_t address,
    std::size_t size
) const {
    if (address < base_) {
        return false;
    }

    const auto offset =
        static_cast<std::uint64_t>(address) - base_;

    if (offset > size_) {
        return false;
    }

    return size <=
        static_cast<std::uint64_t>(size_) - offset;
}

std::uint8_t* GuestMemory::data(
    std::uint32_t address
) {
    if (!contains(address)) {
        return nullptr;
    }
    return bytes_.data() + (address - base_);
}

const std::uint8_t* GuestMemory::data(
    std::uint32_t address
) const {
    if (!contains(address)) {
        return nullptr;
    }
    return bytes_.data() + (address - base_);
}

bool GuestMemory::write(
    std::uint32_t address,
    const void* source,
    std::size_t size
) {
    if (!contains(address, size) || source == nullptr) {
        return false;
    }

    std::memcpy(
        bytes_.data() + (address - base_),
        source,
        size
    );
    return true;
}

bool GuestMemory::zero(
    std::uint32_t address,
    std::size_t size
) {
    if (!contains(address, size)) {
        return false;
    }

    std::memset(
        bytes_.data() + (address - base_),
        0,
        size
    );
    return true;
}

} // namespace mc3::loader
