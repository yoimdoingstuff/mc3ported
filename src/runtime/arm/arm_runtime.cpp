#include "mc3/arm/arm_runtime.hpp"
#include "mc3/log.hpp"
#if defined(_M_ARM) || defined(__arm__)
#define MC3_HOST_IS_ARM32 1
#else
#define MC3_HOST_IS_ARM32 0
#endif
namespace mc3::arm {
RuntimeInfo query_runtime() {
    RuntimeInfo info;
#if MC3_HOST_IS_ARM32
    info.backend_name = "native-arm32-placeholder";
    info.is_arm32_userspace = true;
#else
    info.backend_name = "not-configured";
    info.is_arm32_userspace = false;
#endif
    info.available = false;
    return info;
}
bool ArmRuntime::initialize() {
    info_ = query_runtime();
    log_info("ARM runtime backend: " + info_.backend_name);
    if (!info_.available) {
        log_warning("ARM execution backend is not connected yet.");
        return false;
    }
    return true;
}
bool ArmRuntime::load_library(const std::filesystem::path& path) {
    if (!info_.available) {
        log_error("Cannot load ARM library before an execution backend is integrated: " + path.string());
        return false;
    }
    loaded_library_ = path;
    log_info("Loaded ARM library: " + path.string());
    return true;
}
bool ArmRuntime::call_entrypoint(const std::string& symbol) {
    if (loaded_library_.empty()) {
        log_error("No ARM library is loaded.");
        return false;
    }
    log_info("Would call ARM entry point: " + symbol);
    return true;
}
} // namespace mc3::arm
