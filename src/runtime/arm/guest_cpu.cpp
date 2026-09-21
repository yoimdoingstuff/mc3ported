#include "mc3/arm/guest_cpu.hpp"

#include "mc3/log.hpp"

#if defined(MC3_HAVE_DYNARMIC)

#include <dynarmic/interface/A32/a32.h>
#include <dynarmic/interface/A32/config.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

namespace mc3::arm {

namespace {

class Callbacks final : public Dynarmic::A32::UserCallbacks {
public:
    explicit Callbacks(loader::GuestMemory* memory)
        : memory_(memory) {}

    Dynarmic::A32::Jit* cpu = nullptr;
    loader::GuestMemory* memory_ = nullptr;
    std::uint64_t ticks_remaining = 0;
    bool faulted = false;

    std::uint8_t MemoryRead8(std::uint32_t address) override {
        const auto* value = memory_ ? memory_->data(address) : nullptr;
        if (!value) {
            faulted = true;
            return 0;
        }
        return *value;
    }

    std::uint16_t MemoryRead16(std::uint32_t address) override {
        if (!memory_ || !memory_->contains(address, sizeof(std::uint16_t))) {
            faulted = true;
            return 0;
        }

        std::uint16_t value = 0;
        std::memcpy(&value, memory_->data(address), sizeof(value));
        return value;
    }

    std::uint32_t MemoryRead32(std::uint32_t address) override {
        if (!memory_ || !memory_->contains(address, sizeof(std::uint32_t))) {
            faulted = true;
            return 0;
        }

        std::uint32_t value = 0;
        std::memcpy(&value, memory_->data(address), sizeof(value));
        return value;
    }

    std::uint64_t MemoryRead64(std::uint32_t address) override {
        if (!memory_ || !memory_->contains(address, sizeof(std::uint64_t))) {
            faulted = true;
            return 0;
        }

        std::uint64_t value = 0;
        std::memcpy(&value, memory_->data(address), sizeof(value));
        return value;
    }

    void MemoryWrite8(
        std::uint32_t address,
        std::uint8_t value
    ) override {
        if (!memory_ || !memory_->write(address, &value, sizeof(value))) {
            faulted = true;
        }
    }

    void MemoryWrite16(
        std::uint32_t address,
        std::uint16_t value
    ) override {
        if (!memory_ || !memory_->write(address, &value, sizeof(value))) {
            faulted = true;
        }
    }

    void MemoryWrite32(
        std::uint32_t address,
        std::uint32_t value
    ) override {
        if (!memory_ || !memory_->write(address, &value, sizeof(value))) {
            faulted = true;
        }
    }

    void MemoryWrite64(
        std::uint32_t address,
        std::uint64_t value
    ) override {
        if (!memory_ || !memory_->write(address, &value, sizeof(value))) {
            faulted = true;
        }
    }

    void InterpreterFallback(
        std::uint32_t pc,
        std::size_t num_instructions
    ) override {
        faulted = true;
        log_error(
            "Dynarmic interpreter fallback at 0x" +
            std::to_string(pc) +
            " for " +
            std::to_string(num_instructions) +
            " instructions"
        );
    }

    void CallSVC(std::uint32_t swi) override {
        faulted = true;
        log_error(
            "Unhandled guest SVC #" +
            std::to_string(swi)
        );
    }

    void ExceptionRaised(
        std::uint32_t pc,
        Dynarmic::A32::Exception exception
    ) override {
        faulted = true;
        log_error(
            "Guest ARM exception at 0x" +
            std::to_string(pc) +
            " code=" +
            std::to_string(static_cast<unsigned>(exception))
        );
    }

    void AddTicks(std::uint64_t ticks) override {
        ticks_remaining =
            ticks > ticks_remaining
                ? 0
                : ticks_remaining - ticks;
    }

    std::uint64_t GetTicksRemaining() override {
        return ticks_remaining;
    }
};

} // namespace

struct GuestCpu::Impl {
    explicit Impl(loader::GuestMemory* memory)
        : callbacks(memory) {
        Dynarmic::A32::UserConfig config{};
        config.callbacks = &callbacks;
        config.arch_version = Dynarmic::A32::ArchVersion::v7;

        cpu = std::make_unique<Dynarmic::A32::Jit>(config);
        callbacks.cpu = cpu.get();
    }

    std::unique_ptr<Dynarmic::A32::Jit> cpu;
    Callbacks callbacks;
};

GuestCpu::GuestCpu()
    : impl_(nullptr) {}

GuestCpu::~GuestCpu() = default;
GuestCpu::GuestCpu(GuestCpu&&) noexcept = default;
GuestCpu& GuestCpu::operator=(GuestCpu&&) noexcept = default;

bool GuestCpu::initialize(loader::GuestMemory* memory) {
    if (!memory) {
        return false;
    }

    try {
        impl_ = std::make_unique<Impl>(memory);
        return true;
    } catch (const std::exception& error) {
        log_error(
            std::string("Failed to initialize Dynarmic guest CPU: ") +
            error.what()
        );
        impl_.reset();
        return false;
    }
}

bool GuestCpu::run(
    std::uint32_t entry,
    bool thumb,
    std::uint64_t ticks
) {
    if (!impl_ || !impl_->cpu) {
        return false;
    }

    impl_->callbacks.faulted = false;
    impl_->callbacks.ticks_remaining = ticks;

    impl_->cpu->Regs()[15] = entry;
    impl_->cpu->SetCpsr(
        0x00000010u |
        (thumb ? 0x00000020u : 0u)
    );

    impl_->cpu->Run();

    return !impl_->callbacks.faulted;
}

std::uint32_t GuestCpu::register_value(std::size_t index) const {
    if (!impl_ || !impl_->cpu || index >= 16) {
        return 0;
    }
    return impl_->cpu->Regs()[index];
}

void GuestCpu::set_register(
    std::size_t index,
    std::uint32_t value
) {
    if (!impl_ || !impl_->cpu || index >= 16) {
        return;
    }
    impl_->cpu->Regs()[index] = value;
}

std::uint32_t GuestCpu::program_counter() const {
    return register_value(15);
}

bool GuestCpu::faulted() const {
    return !impl_ || impl_->callbacks.faulted;
}

void GuestCpu::clear_fault() {
    if (impl_) {
        impl_->callbacks.faulted = false;
    }
}

} // namespace mc3::arm

#else

namespace mc3::arm {

struct GuestCpu::Impl {};

GuestCpu::GuestCpu() = default;
GuestCpu::~GuestCpu() = default;
GuestCpu::GuestCpu(GuestCpu&&) noexcept = default;
GuestCpu& GuestCpu::operator=(GuestCpu&&) noexcept = default;

bool GuestCpu::initialize(loader::GuestMemory*) {
    return false;
}

bool GuestCpu::run(
    std::uint32_t,
    bool,
    std::uint64_t
) {
    return false;
}

std::uint32_t GuestCpu::register_value(std::size_t) const {
    return 0;
}

void GuestCpu::set_register(
    std::size_t,
    std::uint32_t
) {}

std::uint32_t GuestCpu::program_counter() const {
    return 0;
}

bool GuestCpu::faulted() const {
    return false;
}

void GuestCpu::clear_fault() {}

} // namespace mc3::arm

#endif
