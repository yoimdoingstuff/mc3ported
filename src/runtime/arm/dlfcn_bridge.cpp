#include "mc3/arm/dlfcn_bridge.hpp"

#include "mc3/arm/guest_cpu.hpp"
#include "mc3/log.hpp"

#include <algorithm>
#include <string>

namespace mc3::arm {

bool DlfcnBridge::configure(
    loader::GuestMemory& memory,
    const loader::ElfImage& image,
    std::uint32_t load_bias
) {
    memory_ = &memory;
    image_ = &image;
    load_bias_ = load_bias;
    return true;
}

bool DlfcnBridge::handles(std::string_view symbol) const {
    return
        symbol == "dlopen" ||
        symbol == "dlclose" ||
        symbol == "dlsym" ||
        symbol == "dlerror" ||
        symbol == "__loader_dlopen" ||
        symbol == "__loader_dlsym";
}

std::string_view DlfcnBridge::read_guest_string(
    std::uint32_t address
) const {
    if (!memory_ || address == 0 || !memory_->contains(address)) {
        return {};
    }

    const auto* data = memory_->data(address);
    if (!data) {
        return {};
    }

    constexpr std::size_t kMax = 8192;
    std::size_t length = 0;

    while (length < kMax &&
           memory_->contains(
               address + static_cast<std::uint32_t>(length)
           )) {
        if (data[length] == 0) {
            return std::string_view(
                reinterpret_cast<const char*>(data),
                length
            );
        }
        ++length;
    }

    return {};
}

void DlfcnBridge::handle(
    std::string_view symbol,
    GuestCpu& guest
) {
    if (!image_) {
        guest.set_register(0, 0);
        return;
    }

    const auto r0 = guest.register_value(0);
    const auto r1 = guest.register_value(1);

    if (
        symbol == "dlopen" ||
        symbol == "__loader_dlopen"
    ) {
        guest.set_register(
            0,
            module_handle_
        );
        return;
    }

    if (symbol == "dlclose") {
        guest.set_register(0, 0);
        return;
    }

    if (symbol == "dlerror") {
        guest.set_register(0, 0);
        return;
    }

    if (
        symbol == "dlsym" ||
        symbol == "__loader_dlsym"
    ) {
        const auto name = read_guest_string(r1);
        if (name.empty()) {
            guest.set_register(0, 0);
            return;
        }

        const auto dynamic = loader::inspect_dynamic(*image_);
        if (!dynamic || !dynamic->present) {
            guest.set_register(0, 0);
            return;
        }

        const auto guest_symbol =
            loader::find_dynamic_symbol(
                *image_,
                *dynamic,
                std::string(name)
            );

        if (!guest_symbol) {
            log_debug(
                "dlsym could not resolve guest symbol: " +
                std::string(name)
            );
            guest.set_register(0, 0);
            return;
        }

        const auto address =
            guest_symbol->value + load_bias_;

        guest.set_register(0, address);
        return;
    }

    guest.set_register(0, 0);
}

} // namespace mc3::arm
