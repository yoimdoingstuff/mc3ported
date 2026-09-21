#include "mc3/arm/arm_runtime.hpp"

#include "mc3/log.hpp"

#if defined(MC3_HAVE_DYNARMIC)

#include <dynarmic/interface/A32/a32.h>
#include <dynarmic/interface/A32/config.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

class SmokeEnvironment final : public Dynarmic::A32::UserCallbacks {
public:
    static constexpr std::size_t MemorySize = 4096;
    std::array<std::uint8_t, MemorySize> memory{};
    std::uint64_t ticks_left = 0;
    bool faulted = false;

    std::uint8_t MemoryRead8(std::uint32_t address) override {
        return address < memory.size() ? memory[address] : 0;
    }

    std::uint16_t MemoryRead16(std::uint32_t address) override {
        return static_cast<std::uint16_t>(MemoryRead8(address)) |
               static_cast<std::uint16_t>(MemoryRead8(address + 1)) << 8;
    }

    std::uint32_t MemoryRead32(std::uint32_t address) override {
        return static_cast<std::uint32_t>(MemoryRead16(address)) |
               static_cast<std::uint32_t>(MemoryRead16(address + 2)) << 16;
    }

    std::uint64_t MemoryRead64(std::uint32_t address) override {
        return static_cast<std::uint64_t>(MemoryRead32(address)) |
               static_cast<std::uint64_t>(MemoryRead32(address + 4)) << 32;
    }

    void MemoryWrite8(std::uint32_t address, std::uint8_t value) override {
        if (address < memory.size()) {
            memory[address] = value;
        }
    }

    void MemoryWrite16(std::uint32_t address, std::uint16_t value) override {
        MemoryWrite8(address, static_cast<std::uint8_t>(value));
        MemoryWrite8(address + 1, static_cast<std::uint8_t>(value >> 8));
    }

    void MemoryWrite32(std::uint32_t address, std::uint32_t value) override {
        MemoryWrite16(address, static_cast<std::uint16_t>(value));
        MemoryWrite16(address + 2, static_cast<std::uint16_t>(value >> 16));
    }

    void MemoryWrite64(std::uint32_t address, std::uint64_t value) override {
        MemoryWrite32(address, static_cast<std::uint32_t>(value));
        MemoryWrite32(address + 4, static_cast<std::uint32_t>(value >> 32));
    }

    void InterpreterFallback(std::uint32_t, std::size_t) override {
        throw std::runtime_error("Dynarmic interpreter fallback reached during smoke test");
    }

    void CallSVC(std::uint32_t) override {
        faulted = true;
    }

    void ExceptionRaised(
        std::uint32_t,
        Dynarmic::A32::Exception
    ) override {
        faulted = true;
    }

    void AddTicks(std::uint64_t ticks) override {
        ticks_left = ticks > ticks_left ? 0 : ticks_left - ticks;
    }

    std::uint64_t GetTicksRemaining() override {
        return ticks_left;
    }
};

bool dynarmic_smoke_test() {
    SmokeEnvironment environment;

    // Thumb-1:
    //   lsls r0, r1, #2
    //   b .
    environment.memory[0] = 0x88;
    environment.memory[1] = 0x00;
    environment.memory[2] = 0xfe;
    environment.memory[3] = 0xe7;

    Dynarmic::A32::UserConfig config{};
    config.callbacks = &environment;
    config.arch_version = Dynarmic::A32::ArchVersion::v7;

    Dynarmic::A32::Jit cpu{config};
    cpu.Regs()[0] = 1;
    cpu.Regs()[1] = 2;
    cpu.Regs()[15] = 0;
    cpu.SetCpsr(0x00000030);

    environment.ticks_left = 1;
    cpu.Run();

    return !environment.faulted && cpu.Regs()[0] == 8;
}

} // namespace

#endif

namespace mc3::arm {

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

#if defined(MC3_HAVE_DYNARMIC)
    try {
        if (!dynarmic_smoke_test()) {
            log_error("Dynarmic ARMv7 smoke test failed.");
            info_.available = false;
            return false;
        }
    } catch (const std::exception& error) {
        log_error(std::string("Dynarmic smoke test threw: ") + error.what());
        info_.available = false;
        return false;
    }

    log_info("Dynarmic ARMv7 execution test passed.");
#endif

    if (!info_.available) {
        log_warning("ARM execution backend is not connected yet.");
        return false;
    }

    return true;
}

bool ArmRuntime::load_library(const std::filesystem::path& path) {
    if (!info_.available) {
        log_error(
            "Cannot load ARM library before an execution backend is integrated: " +
            path.string()
        );
        return false;
    }

    loaded_library_ = path;
    log_info("ARM library accepted for the next ELF-loader stage: " + path.string());
    return true;
}

bool ArmRuntime::call_entrypoint(const std::string& symbol) {
    if (loaded_library_.empty()) {
        log_error("No ARM library is loaded.");
        return false;
    }

    log_info("Entrypoint staging placeholder: " + symbol);
    return false;
}

} // namespace mc3::arm
