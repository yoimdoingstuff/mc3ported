#pragma once

#include "mc3/loader/elf32.hpp"
#include "mc3/loader/imports.hpp"
#include "mc3/loader/memory.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace mc3::arm {

class GuestCpu;

struct RuntimeInfo {
    std::string backend_name;
    bool available = false;
    bool is_arm32_userspace = false;
};

RuntimeInfo query_runtime();

class ArmRuntime {
public:
    ~ArmRuntime();

    bool initialize();
    bool load_library(const std::filesystem::path& path);
    bool call_entrypoint(const std::string& symbol);

    const RuntimeInfo& info() const { return info_; }
    const loader::GuestMemory& memory() const { return memory_; }
    const loader::ElfImage* image() const { return image_ ? &*image_ : nullptr; }
    std::uint32_t load_bias() const { return load_bias_; }

private:
    RuntimeInfo info_{};
    std::filesystem::path loaded_library_;
    loader::GuestMemory memory_;
    std::optional<loader::ElfImage> image_;
    std::uint32_t load_bias_ = 0;
    std::unique_ptr<GuestCpu> cpu_;
    std::unique_ptr<loader::GuestImportTable> imports_;
};

} // namespace mc3::arm
