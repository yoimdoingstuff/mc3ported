#include "mc3/arm/arm_runtime.hpp"

#include "mc3/arm/guest_cpu.hpp"
#include "mc3/log.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>

namespace mc3::arm {

namespace {

bool guest_cpu_smoke_test() {
    loader::GuestMemory memory;
    constexpr std::uint32_t base = 0x10000000;

    if (!memory.reserve(base, 0x1000)) {
        return false;
    }

    constexpr std::array<std::uint8_t, 4> code = {
        0x88, 0x00, // lsls r0, r1, #2
        0xfe, 0xe7  // b .
    };

    if (!memory.write(base, code.data(), code.size())) {
        return false;
    }

    GuestCpu cpu;
    if (!cpu.initialize(&memory)) {
        return false;
    }

    cpu.set_register(1, 2);

    if (!cpu.run(base, true, 1)) {
        return false;
    }

    return cpu.register_value(0) == 8;
}

} // namespace

RuntimeInfo query_runtime() {
    RuntimeInfo info;

#if defined(MC3_HAVE_DYNARMIC)
    info.backend_name = "dynarmic-a32";
    info.available = true;
#else
    info.backend_name = "not-configured";
    info.available = false;
#endif

#if defined(_M_ARM) || defined(__arm__)
    info.is_arm32_userspace = true;
#else
    info.is_arm32_userspace = false;
#endif

    return info;
}

bool ArmRuntime::initialize() {
    info_ = query_runtime();
    log_info("ARM runtime backend: " + info_.backend_name);

    if (!info_.available) {
        log_warning("ARM execution backend is not connected yet.");
        return false;
    }

    if (!guest_cpu_smoke_test()) {
        log_error("Guest CPU memory-backed ARMv7 smoke test failed.");
        info_.available = false;
        return false;
    }

    log_info("Guest CPU memory-backed ARMv7 test passed.");

    return true;
}

bool ArmRuntime::load_library(
    const std::filesystem::path& path
) {
    if (!info_.available) {
        log_error(
            "Cannot load ARM library before an execution backend is integrated: " +
            path.string()
        );
        return false;
    }

    const auto parsed = loader::parse_arm32_elf(path);
    if (!parsed) {
        log_error("ARM ELF parsing failed: " + path.string());
        return false;
    }

    if (!loader::load_arm32_elf(*parsed, memory_)) {
        log_error("ARM ELF memory mapping failed: " + path.string());
        return false;
    }

    image_ = parsed;
    loaded_library_ = path;
    load_bias_ = memory_.base() - parsed->lowest_address;

    char buffer[16]{};
    std::snprintf(
        buffer,
        sizeof(buffer),
        "%08x",
        memory_.base()
    );
    log_info(
        "ARM ELF mapped at guest base 0x" +
        std::string(buffer)
    );
    log_info(
        "ARM ELF guest size: " +
        std::to_string(memory_.size()) +
        " bytes"
    );

    std::snprintf(
        buffer,
        sizeof(buffer),
        "%08x",
        parsed->entry + load_bias_
    );
    log_info(
        "ARM ELF entry: 0x" +
        std::string(buffer)
    );

    if (const auto dynamic = loader::inspect_dynamic(*parsed)) {
        if (dynamic->present) {
            log_info(
                "ARM ELF dynamic imports: " +
                std::to_string(dynamic->imports.size())
            );
            log_info(
                "ARM ELF relative relocations: " +
                std::to_string(dynamic->relative_relocations)
            );

            for (const auto& library : dynamic->needed_libraries) {
                log_info(
                    "ARM ELF dependency: " +
                    library
                );
            }

            for (const auto& import : dynamic->imports) {
                log_info(
                    "ARM ELF import[" +
                    std::to_string(import.relocation_type) +
                    "]: " +
                    import.name
                );
            }
        }
    } else {
        log_error("Failed to inspect ARM ELF dynamic section.");
        return false;
    }

    cpu_ = std::make_unique<GuestCpu>();
    if (!cpu_->initialize(&memory_)) {
        log_error("Failed to bind guest CPU to MC3 memory.");
        cpu_.reset();
        return false;
    }

    return true;
}

bool ArmRuntime::call_entrypoint(
    const std::string& symbol
) {
    if (!image_ || loaded_library_.empty() || !cpu_) {
        log_error("No ARM library is loaded.");
        return false;
    }

    log_info(
        "Entrypoint execution remains pending for symbol: " +
        symbol
    );
    return false;
}

} // namespace mc3::arm
