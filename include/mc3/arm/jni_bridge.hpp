#pragma once

#include "mc3/loader/imports.hpp"
#include "mc3/loader/memory.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mc3::arm {

class GuestCpu;

class JniBridge {
public:
    struct RegisteredNative {
        std::uint32_t clazz = 0;
        std::uint32_t name = 0;
        std::uint32_t signature = 0;
        std::uint32_t function = 0;
    };

    JniBridge() = default;
    ~JniBridge() = default;

    JniBridge(const JniBridge&) = delete;
    JniBridge& operator=(const JniBridge&) = delete;

    bool initialize(
        loader::GuestMemory& memory,
        loader::GuestImportTable& imports
    );

    bool handles(std::string_view symbol) const;

    void handle(
        std::string_view symbol,
        GuestCpu& guest
    );

    std::uint32_t vm_pointer() const { return vm_pointer_; }
    std::uint32_t env_pointer() const { return env_pointer_; }
    std::uint32_t find_class_reference(std::string_view class_name);
    const std::vector<RegisteredNative>& registered_natives() const { return registered_natives_; }

    std::optional<RegisteredNative> find_registered_native(
        std::string_view name,
        std::string_view signature
    ) const;

private:
    std::uint32_t allocate_function_table(
        loader::GuestMemory& memory,
        loader::GuestImportTable& imports,
        const char* prefix,
        std::uint32_t count
    );

    bool write_pointer(
        loader::GuestMemory& memory,
        std::uint32_t address,
        std::uint32_t value
    );

    std::uint32_t hash_handle(
        GuestCpu& guest,
        std::uint32_t first_arg,
        std::uint32_t second_arg
    ) const;

    std::uint32_t allocate_jni_reference(std::uint32_t backing_address);

    std::uint32_t resolve_jni_reference(std::uint32_t reference) const;

    std::uint32_t read_guest_string_length(
        GuestCpu& guest,
        std::uint32_t address
    ) const;

    std::string read_guest_string(
        std::uint32_t address,
        std::uint32_t maximum
    ) const;

    bool write_guest_u32(
        GuestCpu& guest,
        std::uint32_t address,
        std::uint32_t value
    ) const;

    std::uint32_t vm_pointer_ = 0;
    std::uint32_t env_pointer_ = 0;
    std::uint32_t vm_table_ = 0;
    std::uint32_t env_table_ = 0;
    std::uint32_t next_jni_reference_ = 0xF0000001u;
    loader::GuestMemory* memory_ = nullptr;
    loader::GuestImportTable* imports_ = nullptr;
    std::vector<RegisteredNative> registered_natives_;
    std::unordered_map<std::uint32_t, std::uint32_t> jni_reference_backing_;
    std::unordered_map<std::string, std::uint32_t> jni_class_references_;
};

} // namespace mc3::arm
