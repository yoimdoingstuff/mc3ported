#include "mc3/arm/arm_runtime.hpp"

#include "mc3/arm/guest_cpu.hpp"
#include "mc3/arm/jni_bridge.hpp"
#include "mc3/log.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace mc3::arm {

ArmRuntime::~ArmRuntime() = default;

bool ArmRuntime::configure_assets(
    const std::filesystem::path& filesystem_assets_root,
    const std::filesystem::path& apk_path,
    const std::filesystem::path& obb_root
) {
    if (!assets_.configure(
            memory_,
            filesystem_assets_root,
            apk_path,
            obb_root
        )) {
        log_error("Failed to configure Android AssetManager bridge.");
        return false;
    }

    return true;
}

bool ArmRuntime::configure_filesystem(
    const std::filesystem::path& obb_root,
    const std::filesystem::path& app_root,
    const std::filesystem::path& external_root,
    const std::filesystem::path& data_root
) {
    if (!filesystem_.configure(
            obb_root,
            app_root,
            external_root,
            data_root
        )) {
        log_error("Failed to configure Android filesystem paths.");
        return false;
    }

    return true;
}

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

    std::optional<loader::ElfDynamicInfo> dynamic_info;
    if (const auto dynamic = loader::inspect_dynamic(*parsed)) {
        dynamic_info = dynamic;
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

    if (!dynamic_info) {
        return false;
    }

    const auto& dynamic_imports = dynamic_info->imports;

    imports_ =
        std::make_unique<loader::GuestImportTable>(memory_);

    for (const auto& import : dynamic_imports) {
        if (!imports_->bind(import.name)) {
            log_error(
                "Failed to create guest trampoline for import: " +
                import.name
            );
            imports_.reset();
            return false;
        }
    }

    const auto resolver =
        [this](std::string_view name)
            -> std::optional<std::uint32_t> {
            if (!imports_) {
                return std::nullopt;
            }
            return imports_->bind(name);
        };

    if (!loader::patch_arm32_import_relocations(
            *parsed,
            *dynamic_info,
            memory_,
            load_bias_,
            resolver
        )) {
        log_error("Failed to patch MC3 imported symbols.");
        imports_.reset();
        return false;
    }

    if (!libc_.configure(memory_, filesystem_)) {
        log_error("Failed to initialize the libc compatibility bridge.");
        imports_.reset();
        return false;
    }

    cpu_ = std::make_unique<GuestCpu>();
    if (!cpu_->initialize(&memory_)) {
        log_error("Failed to bind guest CPU to MC3 memory.");
        cpu_.reset();
        imports_.reset();
        return false;
    }

    jni_ = std::make_unique<JniBridge>();
    if (!jni_->initialize(memory_, *imports_)) {
        log_error("Failed to initialize guest JNI bridge.");
        jni_.reset();
        return false;
    }

    if (!dlfcn_.configure(memory_, *image_, load_bias_)) {
        log_error("Failed to initialize the dynamic-loader bridge.");
        return false;
    }

    const auto dispatch_svc =
        [this](std::uint32_t immediate, GuestCpu& guest) {
            if (immediate != 0xFFu || !imports_) {
                return;
            }

            const auto slot = guest.register_value(12);
            const auto symbol = imports_->symbol_for_slot(slot);

            if (!symbol) {
                log_error(
                    "Unknown guest import slot: " +
                    std::to_string(slot)
                );
                guest.set_register(0, 0);
                return;
            }

            if (jni_ && jni_->handles(*symbol)) {
                jni_->handle(*symbol, guest);
                return;
            }

            if (libc_.handles(*symbol)) {
                libc_.handle(*symbol, guest);
                return;
            }

            if (dlfcn_.handles(*symbol)) {
                dlfcn_.handle(*symbol, guest);
                return;
            }

            if (assets_.handles(*symbol)) {
                assets_.handle(*symbol, guest);
                return;
            }

            if (pthread_.handles(*symbol)) {
                pthread_.handle(*symbol, guest);
                return;
            }

            log_debug("Guest import call: " + *symbol);
            guest.set_register(0, 0);
        };

    if (!pthread_.configure(memory_, dispatch_svc)) {
        log_error("Failed to initialize pthread bridge.");
        return false;
    }

    cpu_->set_svc_handler(dispatch_svc);

    return true;
}

bool ArmRuntime::bootstrap_jni() {
    if (!jni_ || !cpu_) {
        log_error("JNI bootstrap requested before ARM runtime setup.");
        return false;
    }

    const auto vm = jni_->vm_pointer();
    const auto env = jni_->env_pointer();

    log_info("Calling MC3 JNI_OnLoad...");
    const std::array<std::uint32_t, 2> jni_onload_args{vm, 0};
    if (!call_symbol("JNI_OnLoad", jni_onload_args)) {
        log_error("MC3 JNI_OnLoad failed.");
        return false;
    }

    const auto jni_version = cpu_->register_value(0);
    log_info(
        "MC3 JNI_OnLoad returned 0x" +
        std::to_string(jni_version) +
        " through the guest runtime. JNIEnv=0x" +
        std::to_string(env)
    );

    if (jni_version != 0x00010002u &&
        jni_version != 0x00010004u &&
        jni_version != 0x00010006u) {
        log_warning(
            "MC3 JNI_OnLoad returned an unexpected JNI version: 0x" +
            std::to_string(jni_version)
        );
    }

    if (jni_->registered_natives().empty()) {
        log_warning("MC3 JNI_OnLoad registered no native methods.");
    } else {
        log_info(
            "MC3 JNI_OnLoad registered " +
            std::to_string(jni_->registered_natives().size()) +
            " native methods."
        );
    }

    // The public MC3 Vita loader for the same 1.1.7g build calls these
    // exported JNI entry points in this exact order after JNI_OnLoad. Keep
    // this list data-driven and version-specific rather than guessing names
    // from another MC3 release.
    const auto call_required = [this](
        const char* symbol,
        std::initializer_list<std::uint32_t> arguments
    ) {
        const std::vector<std::uint32_t> args(arguments);
        if (!call_symbol(symbol, args)) {
            log_error(
                "MC3 1.1.7g bootstrap export failed: " +
                std::string(symbol)
            );
            return false;
        }

        log_info(
            "MC3 1.1.7g bootstrap export passed: " +
            std::string(symbol)
        );
        return true;
    };

    const auto gl2jni_class = jni_->find_class_reference("com/gameloft/glf/GL2JNILib");
    const auto billing_class = jni_->find_class_reference("com/gameloft/android/ANMP/GloftM3HM/iab/InAppBilling");
    const auto game_activity_class = jni_->find_class_reference("com/gameloft/android/ANMP/GloftM3HM/GloftM3HM");
    constexpr std::uint32_t dummy_object = 0x24242424u;
    if (gl2jni_class == 0 || billing_class == 0 || game_activity_class == 0) return false;

    if (!call_required(
            "Java_com_gameloft_glf_GL2JNILib_init",
            {env, gl2jni_class}
        )) {
        return false;
    }

    if (!call_required(
            "Java_com_gameloft_android_ANMP_GloftM3HM_iab_InAppBilling_nativeInit",
            {env, billing_class, dummy_object}
        )) {
        return false;
    }

    if (!call_required(
            "Java_video_MyVideoView_Init",
            {env}
        )) {
        return false;
    }

    if (!call_required(
            "Java_com_gameloft_android_ANMP_GloftM3HM_GloftM3HM_Init",
            {env, game_activity_class}
        )) {
        return false;
    }

    if (!call_required(
            "Java_com_gameloft_android_ANMP_GloftM3HM_GloftM3HM_nativeMC3Init",
            {env, game_activity_class}
        )) {
        return false;
    }

    if (!call_required(
            "Java_com_gameloft_glf_GL2JNILib_getViewSettings",
            {}
        )) {
        return false;
    }

    if (!call_required(
            "Java_com_gameloft_glf_GL2JNILib_resize",
            {env, gl2jni_class, 960u, 544u}
        )) {
        return false;
    }

    log_info("MC3 1.1.7g native launcher bootstrap completed.");
    return true;
}

bool ArmRuntime::call_symbol(
    const std::string& symbol,
    std::span<const std::uint32_t> arguments
) {
    if (!image_ || loaded_library_.empty() || !cpu_) {
        log_error("No ARM library is loaded.");
        return false;
    }

    const auto dynamic = loader::inspect_dynamic(*image_);
    if (!dynamic || !dynamic->present) {
        log_error("Loaded ARM library has no dynamic symbol table.");
        return false;
    }

    const auto guest_symbol =
        loader::find_dynamic_symbol(*image_, *dynamic, symbol);

    if (!guest_symbol) {
        log_error("ARM export was not found: " + symbol);
        return false;
    }

    const auto address = guest_symbol->value + load_bias_;
    const bool thumb = (address & 1u) != 0;

    log_info(
        "Calling ARM symbol " +
        symbol +
        " at guest address 0x" +
        std::to_string(address) +
        (thumb ? " (Thumb)" : " (ARM)")
    );

    return cpu_->call(
        address & ~1u,
        thumb,
        arguments
    );
}

bool ArmRuntime::call_entrypoint(
    const std::string& symbol
) {
    return call_symbol(symbol);
}

bool ArmRuntime::step_game() {
    // The public 1.1.7g Vita loader advances MC3 by calling this exact
    // exported JNI entry point once per rendered frame.
    return call_symbol("Java_com_gameloft_glf_GL2JNILib_step");
}

bool ArmRuntime::gameplay_active() const {
    if (!cpu_ || !image_ || loaded_library_.empty()) {
        return false;
    }

    const auto dynamic = loader::inspect_dynamic(*image_);
    if (!dynamic || !dynamic->present) {
        return false;
    }

    const auto guest_symbol =
        loader::find_dynamic_symbol(
            *image_,
            *dynamic,
            "Java_com_gameloft_android_ANMP_GloftM3HM_GloftM3HM_isGamePlay"
        );

    if (!guest_symbol) {
        return false;
    }

    const auto address = guest_symbol->value + load_bias_;
    const bool thumb = (address & 1u) != 0;

    // This query has no Java arguments. Keep it separate from step_game()
    // so callers can decide whether to expose gameplay-only PC controls.
    auto* mutable_cpu = cpu_.get();
    if (!mutable_cpu->call(address & ~1u, thumb, {})) {
        return false;
    }

    return mutable_cpu->register_value(0) != 0;
}


bool ArmRuntime::call_registered_native(
    std::string_view name,
    std::string_view signature,
    std::span<const std::uint32_t> arguments
) {
    if (!jni_ || !cpu_) {
        log_error("Registered JNI native call requested before JNI setup.");
        return false;
    }

    const auto native = jni_->find_registered_native(name, signature);
    if (!native) {
        log_error(
            "Registered JNI native was not found: " +
            std::string(name) +
            " " +
            std::string(signature)
        );
        return false;
    }

    // JNINativeMethod function pointers are already guest addresses.
    // The native ABI receives JNIEnv* and the registered class/object first,
    // followed by the Java arguments.
    std::vector<std::uint32_t> call_arguments;
    call_arguments.reserve(arguments.size() + 2);
    call_arguments.push_back(jni_->env_pointer());
    call_arguments.push_back(native->clazz);
    call_arguments.insert(
        call_arguments.end(),
        arguments.begin(),
        arguments.end()
    );

    const auto address = native->function;
    const bool thumb = (address & 1u) != 0;

    char buffer[16]{};
    std::snprintf(
        buffer,
        sizeof(buffer),
        "%08x",
        address
    );

    log_info(
        "Calling registered MC3 JNI native " +
        std::string(name) +
        " " +
        std::string(signature) +
        " at guest address 0x" +
        std::string(buffer) +
        (thumb ? " (Thumb)" : " (ARM)")
    );

    return cpu_->call(
        address & ~1u,
        thumb,
        call_arguments
    );
}

} // namespace mc3::arm