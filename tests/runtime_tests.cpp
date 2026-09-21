#include "mc3/arm/arm_runtime.hpp"
#include "mc3/game_data.hpp"
#include "mc3/arm/guest_cpu.hpp"
#include "mc3/arm/jni_bridge.hpp"
#include "mc3/loader/elf32.hpp"
#include "mc3/loader/imports.hpp"
#include "mc3/loader/memory.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {

void put16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void put32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8));
    }
}

std::filesystem::path write_test_elf() {
    const auto path =
        std::filesystem::temp_directory_path() / "mc3ported-test.elf";

    std::vector<std::uint8_t> bytes(0x104, 0);

    bytes[0] = 0x7f;
    bytes[1] = 'E';
    bytes[2] = 'L';
    bytes[3] = 'F';
    bytes[4] = 1;
    bytes[5] = 1;

    put16(bytes, 16, 3);
    put16(bytes, 18, 40);
    put32(bytes, 20, 1);
    put32(bytes, 24, 0);
    put32(bytes, 28, 52);
    put16(bytes, 40, 52);
    put16(bytes, 42, 32);
    put16(bytes, 44, 1);

    const std::size_t ph = 52;
    put32(bytes, ph + 0, 1);
    put32(bytes, ph + 4, 0x100);
    put32(bytes, ph + 8, 0);
    put32(bytes, ph + 12, 0);
    put32(bytes, ph + 16, 4);
    put32(bytes, ph + 20, 8);
    put32(bytes, ph + 24, 5);
    put32(bytes, ph + 28, 0x1000);

    bytes[0x100] = 0x78;
    bytes[0x101] = 0x56;
    bytes[0x102] = 0x34;
    bytes[0x103] = 0x12;

    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    return path;
}

} // namespace

int main() {
    const auto temp =
        std::filesystem::temp_directory_path() / "mc3ported-runtime-test";

    std::error_code ec;
    std::filesystem::remove_all(temp, ec);
    std::filesystem::create_directories(temp / "lib" / "armeabi-v7a", ec);
    assert(!ec);

    {
        std::ofstream library(
            temp / "lib" / "armeabi-v7a" / "libModernCombat3.so",
            std::ios::binary
        );
        library << "placeholder";
    }

    const auto info = mc3::inspect_game_data(temp);
    assert(info.has_value());
    assert(info->armv7_library.has_value());
    assert(info->armv7_library->abi == "armeabi-v7a");
    assert(mc3::validate_game_data(*info));

    const auto elf_path = write_test_elf();
    const auto image = mc3::loader::parse_arm32_elf(elf_path);
    assert(image.has_value());
    assert(image->is_dynamic());
    assert(image->machine == 40);
    assert(image->load_segments.size() == 1);

    mc3::loader::GuestMemory memory;
    const auto bias = mc3::loader::load_arm32_elf(*image, memory);
    assert(bias.has_value());
    assert(memory.base() == 0x10000000);
    assert(memory.size() == 0x1000);

    const auto* loaded = memory.data(0x10000000);
    assert(loaded != nullptr);
    assert(loaded[0] == 0x78);
    assert(loaded[1] == 0x56);
    assert(loaded[2] == 0x34);
    assert(loaded[3] == 0x12);
    assert(loaded[4] == 0);
    assert(loaded[7] == 0);

    const auto runtime = mc3::arm::query_runtime();
    assert(!runtime.backend_name.empty());

    mc3::loader::GuestMemory cpu_memory;
    assert(cpu_memory.reserve(0x12000000, 0x1000));

    const std::uint8_t thumb_code[] = {
        0x88, 0x00,
        0xfe, 0xe7
    };
    assert(cpu_memory.write(
        0x12000000,
        thumb_code,
        sizeof(thumb_code)
    ));

    mc3::arm::GuestCpu cpu;
    assert(cpu.initialize(&cpu_memory));
    cpu.set_register(1, 2);
    assert(cpu.run(0x12000000, true, 1));
    assert(cpu.register_value(0) == 8);

    mc3::loader::GuestImportTable import_table(cpu_memory);
    const auto stub = import_table.bind("puts");
    assert(stub.has_value());
    assert(import_table.size() == 1);
    assert(import_table.symbol_for_slot(1).has_value());
    assert(*import_table.symbol_for_slot(1) == "puts");

    const auto stub2 = import_table.bind("malloc");
    assert(stub2.has_value());
    assert((*stub2 - *stub) == 12);

    mc3::arm::JniBridge jni_bridge;
    mc3::loader::GuestMemory jni_memory;
    assert(jni_memory.reserve(0x13000000, 0x1000));

    mc3::loader::GuestImportTable jni_imports(jni_memory);
    assert(jni_bridge.initialize(jni_memory, jni_imports));
    assert(jni_bridge.vm_pointer() != 0);
    assert(jni_bridge.env_pointer() != 0);
    assert(jni_bridge.handles("jni.fn.4"));
    assert(jni_bridge.handles("jni.vm.6"));
    assert(!jni_bridge.handles("libc.so"));

    mc3::arm::GuestCpu jni_cpu;
    assert(jni_cpu.initialize(&jni_memory));
    jni_bridge.handle("jni.fn.4", jni_cpu);
    assert(jni_cpu.register_value(0) == 0x00010006u);

    mc3::arm::GuestCpu jni_vm_cpu;
    assert(jni_vm_cpu.initialize(&jni_memory));
    const auto env_out = jni_memory.allocate_tail(4, 4);
    assert(env_out.has_value());
    jni_vm_cpu.set_register(1, *env_out);
    jni_bridge.handle("jni.vm.6", jni_vm_cpu);
    std::uint32_t env_value = 0;
    std::memcpy(
        &env_value,
        jni_memory.data(*env_out),
        sizeof(env_value)
    );
    assert(env_value == jni_bridge.env_pointer());

    mc3::arm::GuestCpu import_cpu;
    assert(import_cpu.initialize(&cpu_memory));
    import_cpu.set_svc_handler(
        [](std::uint32_t immediate, mc3::arm::GuestCpu& guest) {
            assert(immediate == 0xFFu);
            assert(guest.register_value(12) == 1);
            guest.set_register(0, 0x12345678u);
        }
    );

    // Return from the trampoline into an ARM "b ." loop.
    const std::uint32_t loop = 0xEAFFFFFEu;
    assert(cpu_memory.write(
        0x12000000,
        &loop,
        sizeof(loop)
    ));

    import_cpu.set_register(14, 0x12000000u);
    assert(import_cpu.run(*stub, false, 20));
    assert(import_cpu.register_value(0) == 0x12345678u);

    std::filesystem::remove(elf_path, ec);
    std::filesystem::remove_all(temp, ec);
    return 0;
}
