#include "mc3/arm/guest_cpu.hpp"

#include "mc3/log.hpp"

#if defined(MC3_HAVE_DYNARMIC)

#include <dynarmic/interface/A32/a32.h>
#include <dynarmic/interface/A32/config.h>
#include <dynarmic/interface/halt_reason.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace mc3::arm {

namespace {

constexpr std::uint8_t kSvcImport = 0xFF;
constexpr std::uint8_t kSvcReturn = 0xFE;

class Callbacks final : public Dynarmic::A32::UserCallbacks {
public:
    explicit Callbacks(loader::GuestMemory* memory)
        : memory_(memory) {}

    Dynarmic::A32::Jit* cpu = nullptr;
    GuestCpu* owner = nullptr;
    loader::GuestMemory* memory_ = nullptr;
    SvcHandler svc_handler{};
    std::uint64_t ticks_remaining = 0;
    bool faulted = false;
    bool returned_from_call = false;

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
        if (swi == kSvcReturn) {
            returned_from_call = true;
            if (cpu) {
                cpu->HaltExecution(
                    Dynarmic::HaltReason::UserDefined1
                );
            }
            return;
        }

        if (swi == kSvcImport) {
            if (!svc_handler || !owner) {
                faulted = true;
                log_error("Guest import SVC invoked without a handler.");
                if (cpu) {
                    cpu->HaltExecution(
                        Dynarmic::HaltReason::UserDefined1
                    );
                }
                return;
            }

            svc_handler(swi, *owner);
            return;
        }

        faulted = true;
        log_error(
            "Unhandled guest SVC #" +
            std::to_string(swi)
        );

        if (cpu) {
            cpu->HaltExecution(
                Dynarmic::HaltReason::UserDefined1
            );
        }
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
    explicit Impl(loader::GuestMemory* memory, GuestCpu* owner)
        : callbacks(memory) {
        callbacks.owner = owner;
        callbacks.memory_ = memory;

        Dynarmic::A32::UserConfig config{};
        config.callbacks = &callbacks;
        config.arch_version = Dynarmic::A32::ArchVersion::v7;

        cpu = std::make_unique<Dynarmic::A32::Jit>(config);
        callbacks.cpu = cpu.get();
    }

    std::unique_ptr<Dynarmic::A32::Jit> cpu;
    Callbacks callbacks;
    std::uint32_t stack_top = 0;
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
        impl_ = std::make_unique<Impl>(memory, this);

        constexpr std::size_t kGuestStackSize = 8u * 1024u * 1024u;
        const auto stack =
            memory->allocate_tail(
                kGuestStackSize,
                loader::GuestMemory::PageSize
            );

        if (!stack) {
            log_error("Failed to allocate ARM guest stack.");
            impl_.reset();
            return false;
        }

        const auto raw_top =
            static_cast<std::uint64_t>(*stack) + kGuestStackSize;
        if (raw_top > 0xFFFFFFFFull) {
            log_error("ARM guest stack address overflow.");
            impl_.reset();
            return false;
        }

        impl_->stack_top =
            static_cast<std::uint32_t>(raw_top) & ~0x7u;

        if (!memory->contains(
                impl_->stack_top - 0x100,
                0x100
            )) {
            log_error("ARM guest stack is outside guest memory.");
            impl_.reset();
            return false;
        }

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
    impl_->callbacks.returned_from_call = false;
    impl_->callbacks.ticks_remaining = ticks;

    for (std::size_t i = 0; i < 13; ++i) {
        impl_->cpu->Regs()[i] = 0;
    }

    impl_->cpu->Regs()[13] =
        impl_->stack_top - 0x100;

    impl_->cpu->Regs()[14] =
        impl_->stack_top |
        (thumb ? 1u : 0u);

    impl_->cpu->Regs()[15] = entry;
    impl_->cpu->SetCpsr(
        0x00000010u |
        (thumb ? 0x00000020u : 0u)
    );

    impl_->cpu->Run();

    return !impl_->callbacks.faulted;
}

bool GuestCpu::call(
    std::uint32_t entry,
    bool thumb,
    std::span<const std::uint32_t> arguments
) {
    if (!impl_ || !impl_->cpu || !impl_->callbacks.memory_) {
        return false;
    }

    const auto return_stub =
        impl_->callbacks.memory_->allocate_tail(
            4,
            loader::GuestMemory::PageSize
        );

    if (!return_stub) {
        log_error("Failed to allocate ARM guest return trampoline.");
        return false;
    }

    if (thumb) {
        const std::uint16_t svc = 0xDFFE;
        if (!impl_->callbacks.memory_->write(
                *return_stub,
                &svc,
                sizeof(svc)
            )) {
            return false;
        }
    } else {
        const std::uint32_t svc = 0xEF0000FEu;
        if (!impl_->callbacks.memory_->write(
                *return_stub,
                &svc,
                sizeof(svc)
            )) {
            return false;
        }
    }

    impl_->callbacks.faulted = false;
    impl_->callbacks.returned_from_call = false;
    constexpr std::uint64_t kCallTickBudget = 10000000;
    impl_->callbacks.ticks_remaining = kCallTickBudget;

    for (std::size_t i = 0; i < 13; ++i) {
        impl_->cpu->Regs()[i] = 0;
    }

    const std::size_t stack_argument_count =
        arguments.size() > 4 ? arguments.size() - 4 : 0;
    const std::size_t stack_argument_bytes =
        stack_argument_count * sizeof(std::uint32_t);

    if (stack_argument_bytes > 0x1000 ||
        impl_->stack_top < 0x100 + stack_argument_bytes) {
        log_error("ARM guest call has too many stack arguments.");
        return false;
    }

    const auto stack_pointer =
        impl_->stack_top -
        0x100 -
        static_cast<std::uint32_t>(stack_argument_bytes);

    if (!impl_->callbacks.memory_->contains(
            stack_pointer,
            stack_argument_bytes
        )) {
        log_error("ARM guest call stack arguments are outside guest memory.");
        return false;
    }

    for (std::size_t i = 0; i < stack_argument_count; ++i) {
        const auto value = arguments[i + 4];
        if (!impl_->callbacks.memory_->write(
                stack_pointer +
                    static_cast<std::uint32_t>(
                        i * sizeof(std::uint32_t)
                    ),
                &value,
                sizeof(value)
            )) {
            log_error("Failed to write ARM guest call stack argument.");
            return false;
        }
    }

    impl_->cpu->Regs()[13] = stack_pointer;
    impl_->cpu->Regs()[14] =
        *return_stub |
        (thumb ? 1u : 0u);

    for (std::size_t i = 0; i < arguments.size() && i < 4; ++i) {
        impl_->cpu->Regs()[i] = arguments[i];
    }

    impl_->cpu->Regs()[15] = entry;
    impl_->cpu->SetCpsr(
        0x00000010u |
        (thumb ? 0x00000020u : 0u)
    );

    impl_->cpu->Run();

    return
        !impl_->callbacks.faulted &&
        impl_->callbacks.returned_from_call;
}

void GuestCpu::set_svc_handler(SvcHandler handler) {
    if (impl_) {
        impl_->callbacks.svc_handler = std::move(handler);
    }
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

bool GuestCpu::returned_from_call() const {
    return impl_ && impl_->callbacks.returned_from_call;
}

void GuestCpu::clear_fault() {
    if (impl_) {
        impl_->callbacks.faulted = false;
        impl_->callbacks.returned_from_call = false;
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

bool GuestCpu::call(
    std::uint32_t,
    bool,
    std::span<const std::uint32_t>
) {
    return false;
}

void GuestCpu::set_svc_handler(SvcHandler) {}

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

bool GuestCpu::returned_from_call() const {
    return false;
}

void GuestCpu::clear_fault() {}

} // namespace mc3::arm

#endif
