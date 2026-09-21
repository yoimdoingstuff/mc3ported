#pragma once

#include "mc3/arm/android_fs_bridge.hpp"
#include "mc3/loader/memory.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace mc3::arm {

class GuestCpu;

class LibcBridge {
public:
    LibcBridge() = default;
    ~LibcBridge();

    LibcBridge(const LibcBridge&) = delete;
    LibcBridge& operator=(const LibcBridge&) = delete;

    bool configure(
        loader::GuestMemory& memory,
        AndroidFsBridge& filesystem
    );

    bool handles(std::string_view symbol) const;

    void handle(
        std::string_view symbol,
        GuestCpu& guest
    );

private:
    struct FileHandle {
        std::FILE* file = nullptr;
        bool owns_file = true;
    };

    std::optional<std::string> read_string(
        std::uint32_t address,
        std::size_t maximum
    ) const;

    bool write_bytes(
        std::uint32_t address,
        const void* data,
        std::size_t size
    ) const;

    std::uint32_t allocate(
        std::size_t size,
        std::size_t alignment,
        bool clear
    );

    std::uint32_t new_file_handle(
        std::FILE* file
    );

    FileHandle* get_file(
        std::uint32_t handle
    );

    void erase_file(
        std::uint32_t handle
    );

    static std::string fopen_mode_from_flags(
        std::uint32_t flags
    );

    loader::GuestMemory* memory_ = nullptr;
    AndroidFsBridge* filesystem_ = nullptr;

    std::unordered_map<std::uint32_t, std::size_t> allocations_;
    std::unordered_map<std::uint32_t, FileHandle> files_;
    std::uint32_t next_handle_ = 1000;
};

} // namespace mc3::arm
