#include "mc3/arm/jni_bridge.hpp"

#include "mc3/arm/guest_cpu.hpp"
#include "mc3/log.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <cstring>

namespace mc3::arm {
namespace {

constexpr std::uint32_t kJniFunctionCount = 233;
constexpr std::uint32_t kVmFunctionCount = 8;

constexpr std::uint32_t kJniGetVersion = 4;
constexpr std::uint32_t kJniFindClass = 6;
constexpr std::uint32_t kJniGetMethodId = 33;
constexpr std::uint32_t kJniGetFieldId = 94;
constexpr std::uint32_t kJniGetStaticMethodId = 113;
constexpr std::uint32_t kJniGetStaticFieldId = 144;
constexpr std::uint32_t kJniNewStringUtf = 167;
constexpr std::uint32_t kJniGetStringUtfLength = 168;
constexpr std::uint32_t kJniGetStringUtfChars = 169;
constexpr std::uint32_t kJniReleaseStringUtfChars = 170;
constexpr std::uint32_t kJniGetArrayLength = 171;
constexpr std::uint32_t kJniRegisterNatives = 215;
constexpr std::uint32_t kJniUnregisterNatives = 216;
constexpr std::uint32_t kJniMonitorEnter = 217;
constexpr std::uint32_t kJniMonitorExit = 218;
constexpr std::uint32_t kJniGetJavaVm = 219;
constexpr std::uint32_t kJniExceptionCheck = 228;

constexpr std::uint32_t kVmGetEnv = 6;

std::optional<std::uint32_t> parse_index(
    std::string_view symbol,
    std::string_view prefix
) {
    if (!symbol.starts_with(prefix)) {
        return std::nullopt;
    }

    const auto value = symbol.substr(prefix.size());
    if (value.empty()) {
        return std::nullopt;
    }

    std::uint32_t result = 0;
    for (const char character : value) {
        if (character < '0' || character > '9') {
            return std::nullopt;
        }

        result = result * 10u +
            static_cast<std::uint32_t>(character - '0');
    }

    return result;
}

} // namespace

bool JniBridge::initialize(
    loader::GuestMemory& memory,
    loader::GuestImportTable& imports
) {
    memory_ = &memory;
    imports_ = &imports;

    env_table_ =
        allocate_function_table(
            memory,
            imports,
            "jni.fn.",
            kJniFunctionCount
        );

    vm_table_ =
        allocate_function_table(
            memory,
            imports,
            "jni.vm.",
            kVmFunctionCount
        );

    if (env_table_ == 0 || vm_table_ == 0) {
        log_error("Failed to allocate guest JNI function tables.");
        return false;
    }

    const auto env_object =
        memory.allocate_tail(sizeof(std::uint32_t), 4);

    const auto vm_object =
        memory.allocate_tail(sizeof(std::uint32_t), 4);

    if (!env_object || !vm_object) {
        log_error("Failed to allocate guest JNI objects.");
        return false;
    }

    env_pointer_ = *env_object;
    vm_pointer_ = *vm_object;

    if (!write_pointer(memory, env_pointer_, env_table_) ||
        !write_pointer(memory, vm_pointer_, vm_table_)) {
        log_error("Failed to initialize guest JNI objects.");
        return false;
    }

    log_info(
        "Guest JNI initialized: JNIEnv=0x" +
        std::to_string(env_pointer_) +
        " JavaVM=0x" +
        std::to_string(vm_pointer_)
    );

    return true;
}

std::uint32_t JniBridge::allocate_function_table(
    loader::GuestMemory& memory,
    loader::GuestImportTable& imports,
    const char* prefix,
    std::uint32_t count
) {
    const auto table = memory.allocate_tail(
        static_cast<std::size_t>(count) * sizeof(std::uint32_t),
        4
    );

    if (!table) {
        return 0;
    }

    for (std::uint32_t index = 0; index < count; ++index) {
        const auto symbol =
            std::string(prefix) + std::to_string(index);

        const auto stub = imports.bind(symbol);
        if (!stub) {
            return 0;
        }

        if (!write_pointer(
                memory,
                *table + index * sizeof(std::uint32_t),
                *stub
            )) {
            return 0;
        }
    }

    return *table;
}

bool JniBridge::write_pointer(
    loader::GuestMemory& memory,
    std::uint32_t address,
    std::uint32_t value
) {
    return memory.write(
        address,
        &value,
        sizeof(value)
    );
}

bool JniBridge::handles(std::string_view symbol) const {
    return
        symbol.starts_with("jni.fn.") ||
        symbol.starts_with("jni.vm.");
}

std::uint32_t JniBridge::hash_handle(
    GuestCpu& guest,
    std::uint32_t first_arg,
    std::uint32_t second_arg
) const {
    // Stable, non-zero guest handle. These are opaque to the guest.
    auto mix = [](std::uint32_t value) {
        value ^= value >> 16;
        value *= 0x7feb352du;
        value ^= value >> 15;
        value *= 0x846ca68bu;
        value ^= value >> 16;
        return value;
    };

    auto result =
        mix(first_arg ^ (second_arg + 0x9e3779b9u));

    if (result == 0) {
        result = 1;
    }

    (void)guest;
    return result;
}

std::uint32_t JniBridge::read_guest_string_length(
    GuestCpu& guest,
    std::uint32_t address
) const {
    if (!memory_ || address == 0) {
        return 0;
    }

    constexpr std::uint32_t maximum = 4096;

    for (std::uint32_t i = 0; i < maximum; ++i) {
        const auto* byte = memory_->data(address + i);
        if (!byte || *byte == 0) {
            return i;
        }
    }

    return maximum;
}

bool JniBridge::write_guest_u32(
    GuestCpu& guest,
    std::uint32_t address,
    std::uint32_t value
) const {
    if (!memory_) {
        return false;
    }

    return memory_->write(
        address,
        &value,
        sizeof(value)
    );
}

void JniBridge::handle(
    std::string_view symbol,
    GuestCpu& guest
) {
    if (!memory_) {
        guest.set_register(0, 0);
        return;
    }

    if (const auto index = parse_index(symbol, "jni.vm.")) {
        if (*index == kVmGetEnv) {
            // JavaVM::GetEnv(vm, &env, version)
            const auto env_out = guest.register_value(1);
            if (!write_guest_u32(guest, env_out, env_pointer_)) {
                guest.set_register(0, 0xFFFFFFFCu);
                return;
            }

            guest.set_register(0, 0);
            return;
        }

        guest.set_register(0, 0);
        return;
    }

    const auto index = parse_index(symbol, "jni.fn.");
    if (!index) {
        guest.set_register(0, 0);
        return;
    }

    const auto slot = *index;

    switch (slot) {
        case kJniGetVersion:
            guest.set_register(0, 0x00010006u);
            return;

        case kJniFindClass:
            // Return the class-name pointer as an opaque, stable class handle.
            guest.set_register(0, guest.register_value(1));
            return;

        case kJniGetMethodId:
        case kJniGetFieldId:
        case kJniGetStaticMethodId:
        case kJniGetStaticFieldId:
            guest.set_register(
                0,
                hash_handle(
                    guest,
                    guest.register_value(2),
                    guest.register_value(3)
                )
            );
            return;

        case kJniNewStringUtf:
            guest.set_register(0, guest.register_value(1));
            return;

        case kJniGetStringUtfLength:
        case kJniGetArrayLength:
            guest.set_register(
                0,
                read_guest_string_length(
                    guest,
                    guest.register_value(1)
                )
            );
            return;

        case kJniGetStringUtfChars:
        case 165: // GetStringChars
            guest.set_register(0, guest.register_value(1));
            return;

        case kJniReleaseStringUtfChars:
        case 166: // ReleaseStringChars
        case 22:  // DeleteGlobalRef
        case 23:  // DeleteLocalRef
        case 191: // ReleaseBooleanArrayElements
        case 192:
        case 193:
        case 194:
        case 195:
        case 196:
        case 197:
        case 198:
            guest.set_register(0, 0);
            return;

        case 21: // NewGlobalRef
        case 25: // NewLocalRef
        case 31: // GetObjectClass
        case 172: // NewObjectArray, conservative handle
            guest.set_register(0, guest.register_value(1));
            return;

        case 24: // IsSameObject
            guest.set_register(
                0,
                guest.register_value(1) == guest.register_value(2)
                    ? 1u
                    : 0u
            );
            return;

        case 32: // IsInstanceOf
            guest.set_register(0, 1);
            return;

        case kJniRegisterNatives: {
            // JNINativeMethod is three 32-bit guest pointers:
            // name, signature, fnPtr.
            const auto clazz = guest.register_value(1);
            const auto methods = guest.register_value(2);
            const auto count = guest.register_value(3);

            if (count > 4096u ||
                (count != 0 && !memory_->contains(
                    methods,
                    static_cast<std::size_t>(count) * 12u
                ))) {
                log_error("Guest RegisterNatives supplied an invalid method table.");
                guest.set_register(0, static_cast<std::uint32_t>(-1));
                return;
            }

            // Multiple classes may register native tables during startup.
            // Keep earlier registrations instead of discarding them on each call.
            if (registered_natives_.size() + count > 16384u) {
                log_error("Guest RegisterNatives exceeded the native registration limit.");
                guest.set_register(0, static_cast<std::uint32_t>(-1));
                return;
            }
            registered_natives_.reserve(registered_natives_.size() + count);

            for (std::uint32_t i = 0; i < count; ++i) {
                const auto address = methods + i * 12u;
                std::uint32_t values[3]{};

                if (!memory_->contains(address, sizeof(values))) {
                    log_error("Guest RegisterNatives method entry is outside guest memory.");
                    guest.set_register(0, static_cast<std::uint32_t>(-1));
                    return;
                }

                std::memcpy(values, memory_->data(address), sizeof(values));
                registered_natives_.push_back(RegisteredNative{
                    .clazz = clazz,
                    .name = values[0],
                    .signature = values[1],
                    .function = values[2]
                });

                log_info(
                    "MC3 RegisterNatives[" +
                    std::to_string(i) +
                    "]: name=0x" + std::to_string(values[0]) +
                    " signature=0x" + std::to_string(values[1]) +
                    " fn=0x" + std::to_string(values[2])
                );
            }

            guest.set_register(0, 0);
            return;
        }

        case kJniUnregisterNatives:
        case kJniMonitorEnter:
        case kJniMonitorExit:
            guest.set_register(0, 0);
            return;

        case kJniGetJavaVm:
            if (!write_guest_u32(
                    guest,
                    guest.register_value(1),
                    vm_pointer_
                )) {
                guest.set_register(0, -1);
                return;
            }
            guest.set_register(0, 0);
            return;

        case kJniExceptionCheck:
            guest.set_register(0, 0);
            return;

        default:
            // Most unimplemented JNI calls are safe to treat as neutral
            // bootstrap stubs for now. They will become concrete services
            // as runtime traces identify what MC3 actually needs.
            log_debug(
                "Unhandled guest JNI function slot: " +
                std::to_string(slot)
            );
            guest.set_register(0, 0);
            return;
    }
}

} // namespace mc3::arm
