#include "mc3/arm/libc_bridge.hpp"

#include "mc3/arm/guest_cpu.hpp"
#include "mc3/log.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <thread>

namespace mc3::arm {
namespace {

std::uint32_t arg(GuestCpu& guest, std::size_t index) {
    return guest.register_value(index);
}

bool write_guest(
    loader::GuestMemory& memory,
    std::uint32_t address,
    const void* source,
    std::size_t size
) {
    return memory.write(address, source, size);
}

std::optional<std::string> read_guest_string(
    loader::GuestMemory& memory,
    std::uint32_t address,
    std::size_t maximum
) {
    if (address == 0 || !memory.contains(address)) {
        return std::nullopt;
    }

    const auto* data = memory.data(address);
    if (!data) {
        return std::nullopt;
    }

    std::string value;
    value.reserve(std::min<std::size_t>(maximum, 128));

    for (std::size_t i = 0; i < maximum; ++i) {
        if (!memory.contains(address + static_cast<std::uint32_t>(i))) {
            return std::nullopt;
        }

        const char c =
            static_cast<char>(
                memory.data(
                    address + static_cast<std::uint32_t>(i)
                )[0]
            );

        if (c == '\0') {
            return value;
        }

        value.push_back(c);
    }

    return std::nullopt;
}

} // namespace

LibcBridge::~LibcBridge() {
    for (auto& [handle, entry] : files_) {
        (void)handle;
        if (entry.file && entry.owns_file) {
            std::fclose(entry.file);
        }
    }
}

bool LibcBridge::configure(
    loader::GuestMemory& memory,
    AndroidFsBridge& filesystem
) {
    memory_ = &memory;
    filesystem_ = &filesystem;
    return true;
}

bool LibcBridge::handles(std::string_view symbol) const {
    static constexpr std::string_view names[] = {
        "malloc", "calloc", "realloc", "free",
        "memcpy", "memmove", "memset", "memchr",
        "strlen", "strcmp", "strncmp", "strcpy", "strncpy",
        "strcat", "strncat", "strchr", "strstr",
        "atoi", "atol",
        "open", "open64", "__open_2",
        "close", "read", "write",
        "fopen", "fopen64", "fclose", "fread", "fwrite",
        "fseek", "ftell", "rewind",
        "access", "mkdir", "unlink", "remove", "rename",
        "clock_gettime", "gettimeofday", "time", "usleep", "sleep",
        "__android_log_print", "__android_log_write",
        "abort", "exit",
        "__aeabi_memcpy", "__aeabi_memcpy4", "__aeabi_memcpy8",
        "__aeabi_memclr", "__aeabi_memclr4", "__aeabi_memclr8",
        "__aeabi_memset", "__aeabi_memset4", "__aeabi_memset8"
    };

    for (const auto name : names) {
        if (symbol == name) {
            return true;
        }
    }

    return
        symbol == "memcpy_chk" ||
        symbol == "__memcpy_chk" ||
        symbol == "__memset_chk" ||
        symbol == "__strlen_chk" ||
        symbol == "__strcpy_chk" ||
        symbol == "__strncpy_chk";
}

std::optional<std::string> LibcBridge::read_string(
    std::uint32_t address,
    std::size_t maximum
) const {
    if (!memory_) {
        return std::nullopt;
    }

    return read_guest_string(*memory_, address, maximum);
}

bool LibcBridge::write_bytes(
    std::uint32_t address,
    const void* data,
    std::size_t size
) const {
    return memory_ &&
        memory_->write(address, data, size);
}

std::uint32_t LibcBridge::allocate(
    std::size_t size,
    std::size_t alignment,
    bool clear
) {
    if (!memory_ || size == 0) {
        return 0;
    }

    const auto address =
        memory_->allocate_tail(size, static_cast<std::uint32_t>(alignment));

    if (!address) {
        return 0;
    }

    allocations_[*address] = size;

    if (clear && !memory_->zero(*address, size)) {
        allocations_.erase(*address);
        return 0;
    }

    return *address;
}

std::uint32_t LibcBridge::new_file_handle(
    std::FILE* file
) {
    if (!file) {
        return 0;
    }

    while (files_.contains(next_handle_)) {
        ++next_handle_;
    }

    const auto handle = next_handle_++;
    files_.emplace(handle, FileHandle{file, true});
    return handle;
}

LibcBridge::FileHandle* LibcBridge::get_file(
    std::uint32_t handle
) {
    const auto it = files_.find(handle);
    if (it == files_.end()) {
        return nullptr;
    }
    return &it->second;
}

void LibcBridge::erase_file(
    std::uint32_t handle
) {
    files_.erase(handle);
}

std::string LibcBridge::fopen_mode_from_flags(
    std::uint32_t flags
) {
    constexpr std::uint32_t O_WRONLY = 1;
    constexpr std::uint32_t O_RDWR = 2;
    constexpr std::uint32_t O_CREAT = 64;
    constexpr std::uint32_t O_TRUNC = 512;
    constexpr std::uint32_t O_APPEND = 1024;

    const bool read_write = (flags & O_RDWR) != 0;
    const bool write_only = (flags & O_WRONLY) != 0;

    if (!read_write && !write_only) {
        return "rb";
    }

    std::string mode =
        read_write ? "r+b" : "wb";

    if ((flags & O_CREAT) && !read_write && !write_only) {
        mode = "wb";
    }

    if (flags & O_TRUNC) {
        mode = read_write ? "w+b" : "wb";
    }

    if (flags & O_APPEND) {
        mode = read_write ? "a+b" : "ab";
    }

    return mode;
}

void LibcBridge::handle(
    std::string_view symbol,
    GuestCpu& guest
) {
    if (!memory_) {
        guest.set_register(0, 0);
        return;
    }

    const auto r0 = arg(guest, 0);
    const auto r1 = arg(guest, 1);
    const auto r2 = arg(guest, 2);
    const auto r3 = arg(guest, 3);

    if (symbol == "malloc") {
        guest.set_register(0, allocate(r0, 16, false));
        return;
    }

    if (symbol == "calloc") {
        const auto size =
            static_cast<std::uint64_t>(r0) * r1;
        if (size > static_cast<std::size_t>(-1)) {
            guest.set_register(0, 0);
            return;
        }
        guest.set_register(
            0,
            allocate(static_cast<std::size_t>(size), 16, true)
        );
        return;
    }

    if (symbol == "free") {
        allocations_.erase(r0);
        guest.set_register(0, 0);
        return;
    }

    if (symbol == "realloc") {
        if (r1 == 0) {
            allocations_.erase(r0);
            guest.set_register(0, 0);
            return;
        }

        const auto old = allocations_.find(r0);
        const auto replacement = allocate(r1, 16, false);
        if (replacement != 0 && old != allocations_.end()) {
            std::memcpy(
                memory_->data(replacement),
                memory_->data(r0),
                std::min(old->second, static_cast<std::size_t>(r1))
            );
            allocations_.erase(old);
        }

        guest.set_register(0, replacement);
        return;
    }

    if (
        symbol == "memcpy" ||
        symbol == "__memcpy_chk" ||
        symbol == "memcpy_chk" ||
        symbol == "__aeabi_memcpy" ||
        symbol == "__aeabi_memcpy4" ||
        symbol == "__aeabi_memcpy8"
    ) {
        if (memory_->contains(r0, r2) &&
            memory_->contains(r1, r2)) {
            std::memcpy(memory_->data(r0), memory_->data(r1), r2);
            guest.set_register(0, r0);
        } else {
            guest.set_register(0, 0);
        }
        return;
    }

    if (symbol == "memmove") {
        if (memory_->contains(r0, r2) &&
            memory_->contains(r1, r2)) {
            std::memmove(memory_->data(r0), memory_->data(r1), r2);
            guest.set_register(0, r0);
        } else {
            guest.set_register(0, 0);
        }
        return;
    }

    if (
        symbol == "memset" ||
        symbol == "__memset_chk"
    ) {
        if (memory_->contains(r0, r2)) {
            std::memset(
                memory_->data(r0),
                static_cast<int>(r1 & 0xFFu),
                r2
            );
            guest.set_register(0, r0);
        } else {
            guest.set_register(0, 0);
        }
        return;
    }

    if (
        symbol == "__aeabi_memclr" ||
        symbol == "__aeabi_memclr4" ||
        symbol == "__aeabi_memclr8"
    ) {
        if (memory_->zero(r0, r1)) {
            guest.set_register(0, r0);
        } else {
            guest.set_register(0, 0);
        }
        return;
    }

    if (
        symbol == "__aeabi_memset" ||
        symbol == "__aeabi_memset4" ||
        symbol == "__aeabi_memset8"
    ) {
        if (memory_->contains(r0, r1)) {
            std::memset(
                memory_->data(r0),
                static_cast<int>(r2 & 0xFFu),
                r1
            );
            guest.set_register(0, r0);
        } else {
            guest.set_register(0, 0);
        }
        return;
    }

    if (symbol == "memchr") {
        if (!memory_->contains(r0, r2)) {
            guest.set_register(0, 0);
            return;
        }

        const auto* data = memory_->data(r0);
        for (std::uint32_t i = 0; i < r2; ++i) {
            if (data[i] == static_cast<std::uint8_t>(r1)) {
                guest.set_register(0, r0 + i);
                return;
            }
        }

        guest.set_register(0, 0);
        return;
    }

    if (
        symbol == "strlen" ||
        symbol == "__strlen_chk"
    ) {
        const auto string = read_string(r0, 1024 * 1024);
        guest.set_register(
            0,
            string
                ? static_cast<std::uint32_t>(string->size())
                : 0
        );
        return;
    }

    if (symbol == "strcmp" ||
        symbol == "strncmp") {
        const auto left = read_string(r0, 1024 * 1024);
        const auto right = read_string(r1, 1024 * 1024);

        if (!left || !right) {
            guest.set_register(0, 0);
            return;
        }

        std::string lhs = *left;
        std::string rhs = *right;

        if (symbol == "strncmp") {
            lhs.resize(std::min<std::size_t>(lhs.size(), r2));
            rhs.resize(std::min<std::size_t>(rhs.size(), r2));
        }

        const auto result =
            lhs.compare(rhs);

        guest.set_register(
            0,
            static_cast<std::uint32_t>(result)
        );
        return;
    }

    if (
        symbol == "strcpy" ||
        symbol == "__strcpy_chk"
    ) {
        const auto source = read_string(r1, 1024 * 1024);
        if (!source ||
            !memory_->contains(
                r0,
                source->size() + 1
            )) {
            guest.set_register(0, 0);
            return;
        }

        std::memcpy(
            memory_->data(r0),
            source->c_str(),
            source->size() + 1
        );
        guest.set_register(0, r0);
        return;
    }

    if (symbol == "strncpy" ||
        symbol == "__strncpy_chk") {
        const auto source = read_string(r1, 1024 * 1024);
        if (!source ||
            !memory_->contains(r0, r2)) {
            guest.set_register(0, 0);
            return;
        }

        const auto copy_length =
            std::min<std::size_t>(source->size(), r2);

        std::memcpy(
            memory_->data(r0),
            source->data(),
            copy_length
        );

        if (copy_length < r2) {
            std::memset(
                memory_->data(r0) + copy_length,
                0,
                r2 - copy_length
            );
        }

        guest.set_register(0, r0);
        return;
    }

    if (symbol == "strcat" ||
        symbol == "strncat") {
        const auto destination = read_string(r0, 1024 * 1024);
        const auto source = read_string(r1, 1024 * 1024);
        if (!destination || !source) {
            guest.set_register(0, 0);
            return;
        }

        std::string result = *destination;
        if (symbol == "strncat") {
            result += source->substr(0, r2);
        } else {
            result += *source;
        }

        if (!memory_->contains(r0, result.size() + 1)) {
            guest.set_register(0, 0);
            return;
        }

        std::memcpy(
            memory_->data(r0),
            result.c_str(),
            result.size() + 1
        );
        guest.set_register(0, r0);
        return;
    }

    if (symbol == "strchr" ||
        symbol == "strstr") {
        const auto haystack = read_string(r0, 1024 * 1024);
        const auto needle = read_string(r1, 1024 * 1024);
        if (!haystack || !needle) {
            guest.set_register(0, 0);
            return;
        }

        if (symbol == "strchr") {
            const auto pos =
                haystack->find(
                    static_cast<char>(r1 & 0xFFu)
                );
            guest.set_register(
                0,
                pos == std::string::npos
                    ? 0
                    : r0 + static_cast<std::uint32_t>(pos)
            );
        } else {
            const auto pos =
                haystack->find(*needle);
            guest.set_register(
                0,
                pos == std::string::npos
                    ? 0
                    : r0 + static_cast<std::uint32_t>(pos)
            );
        }

        return;
    }

    if (symbol == "atoi" ||
        symbol == "atol") {
        const auto string = read_string(r0, 1024 * 1024);
        if (!string) {
            guest.set_register(0, 0);
            return;
        }

        try {
            const auto value =
                std::stol(*string);
            guest.set_register(
                0,
                static_cast<std::uint32_t>(value)
            );
        } catch (...) {
            guest.set_register(0, 0);
        }
        return;
    }

    if (
        symbol == "open" ||
        symbol == "open64" ||
        symbol == "__open_2" ||
        symbol == "fopen" ||
        symbol == "fopen64"
    ) {
        if (!filesystem_) {
            guest.set_register(0, 0);
            return;
        }

        const auto path = read_string(r0, 8192);
        if (!path) {
            guest.set_register(0, 0);
            return;
        }

        const auto resolved = filesystem_->resolve(*path);
        if (!resolved) {
            guest.set_register(0, 0);
            return;
        }

        if (
            filesystem_->is_read_only_path(*path) &&
            symbol.starts_with("open") &&
            (r1 & 3u) != 0
        ) {
            guest.set_register(0, static_cast<std::uint32_t>(-1));
            return;
        }

        std::filesystem::path host_path = *resolved;
        std::error_code ec;
        if ((r1 & 64u) != 0 ||
            symbol.starts_with("fopen")) {
            std::filesystem::create_directories(
                host_path.parent_path(),
                ec
            );
        }

        const auto mode =
            symbol.starts_with("fopen")
                ? std::string(
                    read_string(r1, 32).value_or("rb")
                )
                : fopen_mode_from_flags(r1);

        auto* file = std::fopen(
            host_path.string().c_str(),
            mode.c_str()
        );

        if (!file) {
            guest.set_register(0, static_cast<std::uint32_t>(-1));
            return;
        }

        guest.set_register(
            0,
            new_file_handle(file)
        );
        return;
    }

    if (symbol == "close" ||
        symbol == "fclose") {
        auto* file = get_file(r0);
        if (!file || !file->file) {
            guest.set_register(0, static_cast<std::uint32_t>(-1));
            return;
        }

        const int result =
            std::fclose(file->file);
        erase_file(r0);
        guest.set_register(
            0,
            static_cast<std::uint32_t>(result)
        );
        return;
    }

    if (symbol == "read" ||
        symbol == "fread") {
        auto* file = get_file(
            symbol == "fread" ? r3 : r0
        );

        if (!file || !file->file) {
            guest.set_register(0, static_cast<std::uint32_t>(-1));
            return;
        }

        if (symbol == "fread") {
            const auto size = static_cast<std::uint64_t>(r1) * r2;
            if (size == 0 || size > static_cast<std::size_t>(-1) ||
                !memory_->contains(r0, static_cast<std::size_t>(size))) {
                guest.set_register(0, 0);
                return;
            }

            const auto count =
                std::fread(
                    memory_->data(r0),
                    r1,
                    r2,
                    file->file
                );
            guest.set_register(0, static_cast<std::uint32_t>(count));
            return;
        }

        if (!memory_->contains(r1, r2)) {
            guest.set_register(0, static_cast<std::uint32_t>(-1));
            return;
        }

        const auto count =
            std::fread(
                memory_->data(r1),
                1,
                r2,
                file->file
            );
        guest.set_register(0, static_cast<std::uint32_t>(count));
        return;
    }

    if (symbol == "write" ||
        symbol == "fwrite") {
        auto* file = get_file(
            symbol == "fwrite" ? r3 : r0
        );

        if (!file || !file->file) {
            guest.set_register(0, static_cast<std::uint32_t>(-1));
            return;
        }

        if (symbol == "fwrite") {
            const auto size = static_cast<std::uint64_t>(r1) * r2;
            if (size > static_cast<std::size_t>(-1) ||
                !memory_->contains(r0, static_cast<std::size_t>(size))) {
                guest.set_register(0, 0);
                return;
            }

            const auto count =
                std::fwrite(
                    memory_->data(r0),
                    r1,
                    r2,
                    file->file
                );
            guest.set_register(0, static_cast<std::uint32_t>(count));
            return;
        }

        if (!memory_->contains(r1, r2)) {
            guest.set_register(0, static_cast<std::uint32_t>(-1));
            return;
        }

        const auto count =
            std::fwrite(
                memory_->data(r1),
                1,
                r2,
                file->file
            );
        guest.set_register(0, static_cast<std::uint32_t>(count));
        return;
    }

    if (symbol == "fseek") {
        auto* file = get_file(r0);
        if (!file) {
            guest.set_register(0, static_cast<std::uint32_t>(-1));
            return;
        }

        guest.set_register(
            0,
            std::fseek(
                file->file,
                static_cast<long>(static_cast<std::int32_t>(r1)),
                static_cast<int>(r2)
            ) == 0
                ? 0
                : static_cast<std::uint32_t>(-1)
        );
        return;
    }

    if (symbol == "ftell") {
        auto* file = get_file(r0);
        if (!file) {
            guest.set_register(0, static_cast<std::uint32_t>(-1));
            return;
        }

        guest.set_register(
            0,
            static_cast<std::uint32_t>(std::ftell(file->file))
        );
        return;
    }

    if (symbol == "rewind") {
        if (auto* file = get_file(r0)) {
            std::rewind(file->file);
        }
        guest.set_register(0, 0);
        return;
    }

    if (symbol == "access") {
        const auto path = read_string(r0, 8192);
        const auto resolved =
            path && filesystem_
                ? filesystem_->resolve(*path)
                : std::nullopt;

        guest.set_register(
            0,
            resolved &&
            std::filesystem::exists(*resolved)
                ? 0
                : static_cast<std::uint32_t>(-1)
        );
        return;
    }

    if (symbol == "mkdir") {
        const auto path = read_string(r0, 8192);
        const auto resolved =
            path && filesystem_
                ? filesystem_->resolve(*path)
                : std::nullopt;

        if (!resolved) {
            guest.set_register(0, static_cast<std::uint32_t>(-1));
            return;
        }

        std::error_code ec;
        std::filesystem::create_directories(*resolved, ec);
        guest.set_register(0, ec ? static_cast<std::uint32_t>(-1) : 0);
        return;
    }

    if (symbol == "unlink" || symbol == "remove") {
        const auto path = read_string(r0, 8192);
        const auto resolved =
            path && filesystem_
                ? filesystem_->resolve(*path)
                : std::nullopt;

        if (!resolved) {
            guest.set_register(0, static_cast<std::uint32_t>(-1));
            return;
        }

        std::error_code ec;
        std::filesystem::remove(*resolved, ec);
        guest.set_register(
            0,
            (!ec && !std::filesystem::exists(*resolved))
                ? 0
                : static_cast<std::uint32_t>(-1)
        );
        return;
    }

    if (symbol == "rename") {
        const auto old_path = read_string(r0, 8192);
        const auto new_path = read_string(r1, 8192);

        const auto old_resolved =
            old_path && filesystem_
                ? filesystem_->resolve(*old_path)
                : std::nullopt;
        const auto new_resolved =
            new_path && filesystem_
                ? filesystem_->resolve(*new_path)
                : std::nullopt;

        if (!old_resolved || !new_resolved) {
            guest.set_register(0, static_cast<std::uint32_t>(-1));
            return;
        }

        std::error_code ec;
        std::filesystem::create_directories(
            new_resolved->parent_path(),
            ec
        );
        ec.clear();

        std::filesystem::rename(*old_resolved, *new_resolved, ec);
        guest.set_register(
            0,
            ec ? static_cast<std::uint32_t>(-1) : 0
        );
        return;
    }

    if (symbol == "clock_gettime") {
        const auto now =
            std::chrono::system_clock::now().time_since_epoch();
        const auto seconds =
            std::chrono::duration_cast<std::chrono::seconds>(now);
        const auto nanos =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                now - seconds
            );

        struct Timespec32 {
            std::int32_t tv_sec;
            std::int32_t tv_nsec;
        } ts{
            .tv_sec = static_cast<std::int32_t>(seconds.count()),
            .tv_nsec = static_cast<std::int32_t>(nanos.count())
        };

        guest.set_register(
            0,
            write_guest(*memory_, r1, &ts, sizeof(ts)) ? 0 : -1
        );
        return;
    }

    if (symbol == "gettimeofday") {
        const now =
            std::chrono::system_clock::now().time_since_epoch();
        const auto seconds =
            std::chrono::duration_cast<std::chrono::seconds>(now);
        const auto micros =
            std::chrono::duration_cast<std::chrono::microseconds>(
                now - seconds
            );

        struct Timeval32 {
            std::int32_t tv_sec;
            std::int32_t tv_usec;
        } tv{
            .tv_sec = static_cast<std::int32_t>(seconds.count()),
            .tv_usec = static_cast<std::int32_t>(micros.count())
        };

        guest.set_register(
            0,
            r0 == 0 || write_guest(*memory_, r0, &tv, sizeof(tv))
                ? 0
                : static_cast<std::uint32_t>(-1)
        );
        return;
    }

    if (symbol == "time") {
        const auto now =
            std::chrono::system_clock::to_time_t(
                std::chrono::system_clock::now()
            );

        if (r0 != 0) {
            const auto value =
                static_cast<std::int32_t>(now);
            if (!write_guest(*memory_, r0, &value, sizeof(value))) {
                guest.set_register(0, static_cast<std::uint32_t>(-1));
                return;
            }
        }

        guest.set_register(
            0,
            static_cast<std::uint32_t>(
                static_cast<std::int32_t>(now)
            )
        );
        return;
    }

    if (symbol == "usleep") {
        std::this_thread::sleep_for(
            std::chrono::microseconds(r0)
        );
        guest.set_register(0, 0);
        return;
    }

    if (symbol == "sleep") {
        std::this_thread::sleep_for(
            std::chrono::seconds(r0)
        );
        guest.set_register(0, 0);
        return;
    }

    if (symbol == "__android_log_print" ||
        symbol == "__android_log_write") {
        guest.set_register(0, 0);
        return;
    }

    if (symbol == "abort" ||
        symbol == "exit") {
        log_error(
            "Guest called " +
            std::string(symbol)
        );
        guest.set_register(0, 0);
        return;
    }

    guest.set_register(0, 0);
}

} // namespace mc3::arm
