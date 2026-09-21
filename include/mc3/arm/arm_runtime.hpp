#pragma once
#include <filesystem>
#include <string>
namespace mc3::arm {
struct RuntimeInfo {
    std::string backend_name;
    bool available = false;
    bool is_arm32_userspace = false;
};
RuntimeInfo query_runtime();
class ArmRuntime {
public:
    bool initialize();
    bool load_library(const std::filesystem::path& path);
    bool call_entrypoint(const std::string& symbol);
    const RuntimeInfo& info() const { return info_; }
private:
    RuntimeInfo info_{};
    std::filesystem::path loaded_library_;
};
} // namespace mc3::arm
