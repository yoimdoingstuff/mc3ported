#include "mc3/arm/jni_bridge.hpp"

#include "mc3/arm/guest_cpu.hpp"
#include "mc3/log.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>

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


std::optional<std::uint32_t> reference_method_id(std::string_view name) {
    struct Entry {
        std::uint32_t id;
        std::string_view name;
    };

    static constexpr Entry entries[] = {
        {102, "getPreferenceString"},
        {103, "getPackage"},
        {104, "getSaveFolder"},
        {105, "getContext"},
        {106, "getUserAgent"},
        {107, "getResource"},
        {108, "setupPaths"},
        {109, "createView"},
        {110, "setViewSettings"},
        {111, "setCurrentContext"},
        {112, "enableAccelerometer"},
        {113, "enableGyroscope"},
        {114, "openBrowser"},
        {115, "openGLLive"},
        {116, "notifyTrophy"},
        {117, "openIGP"},
        {118, "getDeviceLanguage"},
        {119, "setCalibratingGyro"},
        {120, "resetGyroCalibration"},
        {121, "setVisibility"},
        {122, "getText"},
        {123, "setText"},
        {124, "getSystemService"},
        {125, "hasGyroscope"},
        {126, "isXperiaPlay"},
        {127, "isDeviceOpened"},
        {128, "mustSwapButtons"},
        {129, "isHTCDevice"},
        {130, "isGyroSensibilityHigh"},
        {131, "isGyroSensibilityLow"},
        {132, "isGyroPendingToFix"},
        {133, "ignoreGyroLowInc"},
        {134, "isGyroInvertedX"},
        {135, "isInvertedPitchYaw"},
        {136, "isSwitchRollToYaw"},
        {137, "useGeneration0"},
        {138, "useGeneration1"},
        {139, "useGeneration2"},
        {140, "getProcessorNum"},
        {141, "isFirstRunOfTheDay"},
        {142, "isHoneycombDevice"},
        {143, "isIceCreamSandwich"},
        {144, "isSamsung_I9250"},
        {145, "isSamsung_I510"},
        {146, "isSamsung_P7100"},
        {147, "isAsus_TF201"},
        {148, "isHTC_Nexus9"},
        {149, "getZipName"},
        {150, "getPatchFileLocation"},
        {151, "deleteOldData"},
        {152, "sendURLRequest"},
        {153, "getServerResponse"},
        {154, "hasServerResponse"},
        {155, "showDisclaimer"},
        {156, "showCantGoBackPopup"},
        {157, "showParentButton"},
        {158, "hideParentButton"},
        {159, "getTotalItems"},
        {160, "getData"},
        {161, "GetState"},
        {162, "android/content/Intent/<init>"},
        {163, "setClassName"},
        {164, "putExtra"},
        {165, "android/os/Bundle/<init>"},
        {166, "putString"},
        {167, "getByteArray"},
        {168, "putByteArray"},
        {169, "getInt"},
        {170, "putInt"},
        {171, "getLong"},
        {172, "putLong"},
        {173, "containsKey"},
        {174, "clear"},
        {175, "getSDFolder"},
        {176, "showloading"},
        {177, "android/media/AudioTrack/<init>"},
        {178, "getMinBufferSize"},
        {179, "play"},
        {180, "pause"},
        {181, "stop"},
        {182, "release"},
        {183, "write"},
        {184, "getPlayState"},
        {185, "getNativeOutputSampleRate"},
        {186, "setThreadPriority"},
        {187, "setFlags"},
        {188, "getIntExtra"},
        {189, "removeExtra"},
        {190, "addCategory"},
        {191, "startActivity"},
        {192, "startActivityForResult"},
        {193, "hasWindowFocus"},
        {194, "getWindow"},
        {195, "setVolumeControlStream"},
        {196, "requestWindowFeature"},
        {197, "setContentView"},
        {198, "getIntent"},
        {199, "finish"},
        {200, "getWindowManager"},
        {201, "hideloading"},
        {202, "hidelogoscreen"},
        {203, "HasPushNotification"},
        {204, "LaunchGamebyNotification"},
        {205, "ResetNotificationStatus"},
        {206, "ResetLaunchStatus"},
        {207, "EnableDisablePushNotifications"},
        {208, "ShowAlert"},
        {209, "GetTokenID"},
        {210, "GetUserID"},
        {211, "SendPush"},
        {212, "SendPushToMyself"},
        {213, "SetOfflineUserCredential"},
        {214, "SetOnlineUserCredential"},
        {215, "SetOfflineDeviceCredential"},
        {216, "DeletePush"},
        {217, "GetBundleData"},
        {218, "GetJanusToken"},
        {219, "getString"},
        {220, "getIMEI"},
        {221, "getHDIDFVd"},
        {222, "getCountryCode"},
        {223, "getDeviceName"},
        {224, "getDeviceFW"},
        {225, "IsWifiEnabled"}
    };

    for (const auto& entry : entries) {
        if (entry.name == name) {
            return entry.id;
        }
    }
    return std::nullopt;
}

std::optional<std::uint32_t> allocate_guest_string(
    loader::GuestMemory& memory,
    std::string_view value
) {
    const auto address =
        memory.allocate_tail(value.size() + 1, 1);
    if (!address) {
        return std::nullopt;
    }

    if (!memory.write(
            *address,
            value.data(),
            value.size()) ||
        !memory.zero(*address + static_cast<std::uint32_t>(value.size()), 1)) {
        return std::nullopt;
    }

    return address;
}

} // namespace

bool JniBridge::initialize(
    loader::GuestMemory& memory,
    loader::GuestImportTable& imports
) {
    memory_ = &memory;
    imports_ = &imports;
    registered_natives_.clear();
    jni_reference_backing_.clear();
    jni_class_references_.clear();
    next_jni_reference_ = 0xF0000001u;

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

std::uint32_t JniBridge::find_class_reference(std::string_view class_name) {
    if (!memory_ || class_name.empty()) {
        return 0;
    }

    const auto existing = jni_class_references_.find(std::string(class_name));
    if (existing != jni_class_references_.end()) {
        return existing->second;
    }

    const auto backing = allocate_guest_string(*memory_, class_name);
    if (!backing) {
        return 0;
    }

    const auto reference = allocate_jni_reference(*backing);
    if (reference == 0) {
        return 0;
    }

    jni_class_references_.emplace(std::string(class_name), reference);
    return reference;
}

std::uint32_t JniBridge::allocate_jni_reference(std::uint32_t backing_address) {
    if (backing_address == 0) {
        return 0;
    }

    while (next_jni_reference_ == 0 ||
           jni_reference_backing_.contains(next_jni_reference_)) {
        ++next_jni_reference_;
    }

    const auto reference = next_jni_reference_++;
    jni_reference_backing_.emplace(reference, backing_address);
    return reference;
}

std::uint32_t JniBridge::resolve_jni_reference(
    std::uint32_t reference
) const {
    const auto it = jni_reference_backing_.find(reference);
    return it == jni_reference_backing_.end() ? reference : it->second;
}

std::uint32_t JniBridge::hash_handle(
    GuestCpu& guest,
    std::uint32_t first_arg,
    std::uint32_t second_arg
) const {
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

    address = resolve_jni_reference(address);

    constexpr std::uint32_t maximum = 4096;

    for (std::uint32_t i = 0; i < maximum; ++i) {
        const auto* byte = memory_->data(address + i);
        if (!byte || *byte == 0) {
            return i;
        }
    }

    (void)guest;
    return maximum;
}

std::string JniBridge::read_guest_string(
    std::uint32_t address,
    std::uint32_t maximum
) const {
    if (!memory_ || address == 0 || maximum == 0) {
        return {};
    }

    address = resolve_jni_reference(address);

    std::string result;
    result.reserve(maximum);

    for (std::uint32_t i = 0; i < maximum; ++i) {
        const auto* byte = memory_->data(address + i);
        if (!byte || *byte == 0) {
            break;
        }

        const char character = static_cast<char>(*byte);
        result.push_back(
            character >= 0x20 && character <= 0x7e
                ? character
                : '?'
        );
    }

    return result;
}

bool JniBridge::write_guest_u32(
    GuestCpu& guest,
    std::uint32_t address,
    std::uint32_t value
) const {
    if (!memory_) {
        return false;
    }

    (void)guest;
    return memory_->write(
        address,
        &value,
        sizeof(value)
    );
}

std::optional<JniBridge::RegisteredNative> JniBridge::find_registered_native(
    std::string_view name,
    std::string_view signature
) const {
    for (const auto& native : registered_natives_) {
        if (read_guest_string(native.name, 1024) == name &&
            read_guest_string(native.signature, 1024) == signature) {
            return native;
        }
    }

    return std::nullopt;
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

        case kJniFindClass: {
            const auto class_name =
                read_guest_string(guest.register_value(1), 1024);
            if (class_name.empty()) {
                guest.set_register(0, 0);
                return;
            }

            guest.set_register(0, find_class_reference(class_name));
            return;
        }

        case kJniGetMethodId:
        case kJniGetStaticMethodId: {
            const auto name =
                read_guest_string(guest.register_value(2), 1024);
            if (name) {
                if (const auto known = reference_method_id(*name)) {
                    guest.set_register(0, *known);
                    return;
                }
            }

            guest.set_register(
                0,
                hash_handle(
                    guest,
                    guest.register_value(2),
                    guest.register_value(3)
                )
            );
            return;
        }

        case kJniGetFieldId:
        case kJniGetStaticFieldId: {
            const auto name =
                read_guest_string(guest.register_value(2), 256);
            if (name && *name == "WINDOW_SERVICE") {
                guest.set_register(0, 0);
                return;
            }
            if (name && *name == "SDK_INT") {
                guest.set_register(0, 1);
                return;
            }

            guest.set_register(
                0,
                hash_handle(
                    guest,
                    guest.register_value(2),
                    guest.register_value(3)
                )
            );
            return;
        }

        case kJniNewStringUtf: {
            const auto source = guest.register_value(1);
            const auto length = read_guest_string_length(guest, source);
            if (source == 0) {
                guest.set_register(0, 0);
                return;
            }

            const auto value = read_guest_string(source, length);
            const backing = allocate_guest_string(*memory_, value);
            if (!backing) {
                guest.set_register(0, 0);
                return;
            }

            guest.set_register(
                0,
                allocate_jni_reference(*backing)
            );
            return;
        }

        // JNI method invocation slots are fixed by the JNI interface table.
        // The method IDs below mirror the reference MC3 Vita loader's
        // 1.1.7g FalsoJNI table. This lets the real ARM code call the same
        // Android-side methods without inventing game-specific entrypoints.
        case 34:
        case 35:
        case 36:
        case 114:
        case 115:
        case 116: {
            const method = guest.register_value(2);
            std::optional<std::string> value;

            switch (method) {
                case 102: value = "dummy"; break;
                case 103: value = "com.gameloft.android.ANMP.GloftM3HM"; break;
                case 104: value = "dummy"; break;
                case 106: value = "dummy"; break;
                case 118: value = "dummy"; break;
                case 149:
                    value = "main.1120.com.gameloft.android.ANMP.GloftM3HM.obb";
                    break;
                case 150:
                    value = "patch.11428.com.gameloft.android.ANMP.GloftM3HM.obb";
                    break;
                case 153: value = "dummy"; break;
                case 175: value = "data"; break;
                case 177: value = "dummy"; break;
                case 187: value = "dummy"; break;
                case 209:
                case 210:
                case 211:
                case 212:
                case 218:
                case 219:
                    value = "dummy";
                    break;
                case 220:
                case 221:
                    value = "351066496380730";
                    break;
                case 222: value = "us"; break;
                case 223: value = "Xperia Play"; break;
                case 224: value = "4.1.2"; break;
                default:
                    break;
            }

            if (value) {
                if (const auto address =
                        allocate_guest_string(*memory_, *value)) {
                    guest.set_register(0, *address);
                } else {
                    guest.set_register(0, 0);
                }
            } else {
                // Reference loader returns a stable dummy object for these
                // Android framework methods.
                guest.set_register(0, 0x42424242u);
            }
            return;
        }

        case 37:
        case 38:
        case 39:
        case 117:
        case 118:
        case 119: {
            switch (guest.register_value(2)) {
                case 126: // isXperiaPlay
                case 127: // isDeviceOpened
                case 139: // useGeneration2
                    guest.set_register(0, 1);
                    break;
                default:
                    guest.set_register(0, 0);
                    break;
            }
            return;
        }

        case 49:
        case 50:
        case 51:
        case 129:
        case 130:
        case 131: {
            switch (guest.register_value(2)) {
                case 140: // getProcessorNum
                    guest.set_register(0, 3);
                    break;
                case 178: // getMinBufferSize
                    guest.set_register(0, 4096);
                    break;
                case 183: // AudioTrack.write
                    guest.set_register(0, guest.register_value(3));
                    break;
                case 184:
                case 188:
                case 203:
                case 204:
                case 215:
                case 216:
                case 225:
                case 159:
                case 161:
                case 169:
                    guest.set_register(0, 0);
                    break;
                case 185: // getNativeOutputSampleRate
                    guest.set_register(0, 44100);
                    break;
                default:
                    guest.set_register(0, 0);
                    break;
            }
            return;
        }

        case 52:
        case 53:
        case 54:
        case 132:
        case 133:
        case 134:
            guest.set_register(0, 0);
            guest.set_register(1, 0);
            return;

        case 61:
        case 62:
        case 63:
        case 141:
        case 142:
        case 143:
            guest.set_register(0, 0);
            return;

        case kJniGetStringUtfLength:
            guest.set_register(
                0,
                read_guest_string_length(
                    guest,
                    guest.register_value(1)
                )
            );
            return;

        case kJniGetStringUtfChars:
        case 165:
            guest.set_register(
                0,
                resolve_jni_reference(guest.register_value(1))
            );
            return;

        case kJniGetArrayLength:
            guest.set_register(0, 0);
            return;

        case kJniReleaseStringUtfChars:
        case 166:
        case 22:
        case 23:
        case 191:
        case 192:
        case 193:
        case 194:
        case 195:
        case 196:
        case 197:
        case 198:
            guest.set_register(0, 0);
            return;

        case 21:
        case 25:
        case 31:
        case 172:
            guest.set_register(0, guest.register_value(1));
            return;

        case 24:
            guest.set_register(
                0,
                guest.register_value(1) == guest.register_value(2)
                    ? 1u
                    : 0u
            );
            return;

        case 32:
            guest.set_register(0, 1);
            return;

        case kJniRegisterNatives: {
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

                const auto name = read_guest_string(values[0], 1024);
                const auto signature = read_guest_string(values[1], 1024);

                log_info(
                    "MC3 RegisterNatives[" +
                    std::to_string(i) +
                    "]: clazz=0x" + std::to_string(clazz) +
                    " name=" + (name.empty() ? "<invalid>" : name) +
                    " signature=" +
                    (signature.empty() ? "<invalid>" : signature) +
                    " fn=0x" + std::to_string(values[2])
                );
            }

            guest.set_register(0, 0);
            return;
        }

        case kJniUnregisterNatives: {
            const auto clazz = guest.register_value(1);
            registered_natives_.erase(
                std::remove_if(
                    registered_natives_.begin(),
                    registered_natives_.end(),
                    [clazz](const RegisteredNative& native) {
                        return native.clazz == clazz;
                    }
                ),
                registered_natives_.end()
            );
            guest.set_register(0, 0);
            return;
        }

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
                guest.set_register(0, static_cast<std::uint32_t>(-1));
                return;
            }
            guest.set_register(0, 0);
            return;

        case kJniExceptionCheck:
            guest.set_register(0, 0);
            return;

        case 145: // GetStaticObjectField
            if (guest.register_value(2) == 0) {
                if (const auto address = allocate_guest_string(*memory_, "window")) {
                    guest.set_register(0, *address);
                } else {
                    guest.set_register(0, 0);
                }
            } else {
                guest.set_register(0, 0);
            }
            return;

        case 150: // GetStaticIntField
            guest.set_register(0, guest.register_value(2) == 1 ? 19u : 0u);
            return;

        default:
            log_debug(
                "Unhandled guest JNI function slot: " +
                std::to_string(slot)
            );
            guest.set_register(0, 0);
            return;
    }
}

} // namespace mc3::arm
