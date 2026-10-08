#include "mc3/arm/arm_runtime.hpp"
#include "mc3/game_data.hpp"
#include "mc3/arm/guest_cpu.hpp"
#include "mc3/arm/asset_bridge.hpp"
#include "mc3/arm/dlfcn_bridge.hpp"
#include "mc3/arm/pthread_bridge.hpp"
#include "mc3/arm/jni_bridge.hpp"
#include "mc3/arm/android_fs_bridge.hpp"
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

    const auto class_ref_a = jni_bridge.find_class_reference("com/gameloft/glf/GL2JNILib");
    const auto class_ref_b = jni_bridge.find_class_reference("com/gameloft/glf/GL2JNILib");
    const auto class_ref_other = jni_bridge.find_class_reference("com/gameloft/android/ANMP/GloftM3HM/GloftM3HM");
    assert(class_ref_a != 0);
    assert(class_ref_a == class_ref_b);
    assert(class_ref_other != 0);
    assert(class_ref_other != class_ref_a);

    const auto find_class_name = jni_memory.allocate_tail(64, 1);
    assert(find_class_name.has_value());
    const char find_class_text[] = "com/gameloft/glf/GL2JNILib";
    assert(jni_memory.write(*find_class_name, find_class_text, sizeof(find_class_text)));
    mc3::arm::GuestCpu find_class_cpu_a;
    assert(find_class_cpu_a.initialize(&jni_memory));
    find_class_cpu_a.set_register(1, *find_class_name);
    jni_bridge.handle("jni.fn.6", find_class_cpu_a);
    const auto find_class_ref_a = find_class_cpu_a.register_value(0);
    mc3::arm::GuestCpu find_class_cpu_b;
    assert(find_class_cpu_b.initialize(&jni_memory));
    find_class_cpu_b.set_register(1, *find_class_name);
    jni_bridge.handle("jni.fn.6", find_class_cpu_b);
    assert(find_class_ref_a != 0);
    assert(find_class_cpu_b.register_value(0) == find_class_ref_a);

    mc3::arm::GuestCpu jni_cpu;
    assert(jni_cpu.initialize(&jni_memory));
    jni_bridge.handle("jni.fn.4", jni_cpu);
    assert(jni_cpu.register_value(0) == 0x00010006u);

    const auto jni_probe = jni_memory.allocate_tail(8, 2);
    assert(jni_probe.has_value());

    const std::uint16_t jni_thumb[] = {
        0x6801,
        0x690A,
        0x4790,
        0x4770
    };

    assert(jni_memory.write(
        *jni_probe,
        jni_thumb,
        sizeof(jni_thumb)
    ));

    mc3::arm::GuestCpu jni_guest;
    assert(jni_guest.initialize(&jni_memory));
    assert(
        jni_guest.call(
            *jni_probe,
            true,
            {jni_bridge.env_pointer()}
        )
    );
    assert(jni_guest.register_value(0) == 0x00010006u);

    const auto method_name = jni_memory.allocate_tail(32, 1);
    const auto method_sig = jni_memory.allocate_tail(32, 1);
    assert(method_name && method_sig);
    const char method_text[] = "getPackage";
    const char method_sig_text[] = "()Ljava/lang/String;";
    assert(jni_memory.write(*method_name, method_text, sizeof(method_text)));
    assert(jni_memory.write(*method_sig, method_sig_text, sizeof(method_sig_text)));

    mc3::arm::GuestCpu method_id_cpu;
    assert(method_id_cpu.initialize(&jni_memory));
    method_id_cpu.set_register(1, 0x22000000u);
    method_id_cpu.set_register(2, *method_name);
    method_id_cpu.set_register(3, *method_sig);
    jni_bridge.handle("jni.fn.33", method_id_cpu);
    assert(method_id_cpu.register_value(0) == 103u);

    const char zip_name_text[] = "getZipName";
    assert(jni_memory.write(*method_name, zip_name_text, sizeof(zip_name_text)));
    mc3::arm::GuestCpu zip_method_cpu;
    assert(zip_method_cpu.initialize(&jni_memory));
    zip_method_cpu.set_register(1, 0x22000000u);
    zip_method_cpu.set_register(2, *method_name);
    zip_method_cpu.set_register(3, *method_sig);
    jni_bridge.handle("jni.fn.33", zip_method_cpu);
    assert(zip_method_cpu.register_value(0) == 149u);

    const auto native_name = jni_memory.allocate_tail(32, 1);
    const auto native_signature = jni_memory.allocate_tail(32, 1);
    const auto native_table = jni_memory.allocate_tail(12, 4);
    assert(native_name && native_signature && native_table);

    const char native_name_text[] = "mc3Probe";
    const char native_signature_text[] = "(I)I";
    assert(jni_memory.write(*native_name, native_name_text, sizeof(native_name_text)));
    assert(jni_memory.write(*native_signature, native_signature_text, sizeof(native_signature_text)));

    const std::uint32_t native_entry[3] = {
        *native_name,
        *native_signature,
        0x12345679u
    };
    assert(jni_memory.write(*native_table, native_entry, sizeof(native_entry)));

    mc3::arm::GuestCpu register_cpu;
    assert(register_cpu.initialize(&jni_memory));
    register_cpu.set_register(1, 0x22000000u);
    register_cpu.set_register(2, *native_table);
    register_cpu.set_register(3, 1);
    jni_bridge.handle("jni.fn.215", register_cpu);
    assert(register_cpu.register_value(0) == 0);

    const auto registered = jni_bridge.find_registered_native("mc3Probe", "(I)I");
    assert(registered.has_value());
    assert(registered->clazz == 0x22000000u);
    assert(registered->function == 0x12345679u);
    assert(!jni_bridge.find_registered_native("missing", "(I)I").has_value());

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

    const std::uint32_t loop = 0xEAFFFFFEu;
    assert(cpu_memory.write(
        0x12000000,
        &loop,
        sizeof(loop)
    ));

    import_cpu.set_register(14, 0x12000000u);
    assert(import_cpu.run(*stub, false, 20));
    assert(import_cpu.register_value(0) == 0x12345678u);

    const auto asset_root =
        std::filesystem::temp_directory_path() / "mc3ported-assets-test";
    std::filesystem::remove_all(asset_root, ec);
    std::filesystem::create_directories(asset_root / "empty", ec);
    std::filesystem::create_directories(asset_root / "dir", ec);
    assert(!ec);

    {
        std::ofstream asset(asset_root / "hello.bin", std::ios::binary);
        const char bytes[] = "asset-data";
        asset.write(bytes, sizeof(bytes) - 1);
    }
    {
        std::ofstream first(asset_root / "dir" / "z-last.txt");
        first << "z";
        std::ofstream second(asset_root / "dir" / "a-first.txt");
        second << "a";
    }

    mc3::loader::GuestMemory asset_memory;
    assert(asset_memory.reserve(0x14000000, 0x2000));

    mc3::arm::AssetBridge assets;
    assert(assets.configure(
        asset_memory,
        asset_root,
        {},
        {}
    ));

    assert(assets.handles("AAssetManager_open"));
    assert(assets.handles("AAsset_read"));
    assert(assets.handles("AAsset_seek"));
    assert(assets.handles("AAsset_getLength"));
    assert(!assets.handles("AAssetManager_openFile"));

    const auto filename_address = asset_memory.allocate_tail(32, 1);
    const auto buffer_address = asset_memory.allocate_tail(32, 1);
    const auto dir_name_address = asset_memory.allocate_tail(32, 1);
    const auto dir_buffer_address = asset_memory.allocate_tail(32, 1);
    assert(filename_address && buffer_address && dir_name_address && dir_buffer_address);

    const char filename[] = "hello.bin";
    assert(asset_memory.write(*filename_address, filename, sizeof(filename)));

    mc3::arm::GuestCpu asset_cpu;
    assert(asset_cpu.initialize(&asset_memory));
    asset_cpu.set_register(1, *filename_address);
    assets.handle("AAssetManager_open", asset_cpu);

    const auto asset_handle = asset_cpu.register_value(0);
    assert(asset_handle != 0);

    asset_cpu.set_register(0, asset_handle);
    asset_cpu.set_register(1, *buffer_address);
    asset_cpu.set_register(2, 5);
    assets.handle("AAsset_read", asset_cpu);
    assert(asset_cpu.register_value(0) == 5);

    char read_back[6]{};
    std::memcpy(read_back, asset_memory.data(*buffer_address), 5);
    assert(std::string(read_back, 5) == "asset");

    asset_cpu.set_register(0, asset_handle);
    assets.handle("AAsset_getLength", asset_cpu);
    assert(asset_cpu.register_value(0) == 10);

    asset_cpu.set_register(0, asset_handle);
    assets.handle("AAsset_close", asset_cpu);
    assert(asset_cpu.register_value(0) == 0);

    const char dir_name[] = "dir";
    assert(asset_memory.write(*dir_name_address, dir_name, sizeof(dir_name)));
    asset_cpu.set_register(1, *dir_name_address);
    assets.handle("AAssetManager_openDir", asset_cpu);
    const auto dir_handle = asset_cpu.register_value(0);
    assert(dir_handle != 0);

    asset_cpu.set_register(0, dir_handle);
    asset_cpu.set_register(1, *dir_buffer_address);
    assets.handle("AAssetDir_getNextFileName", asset_cpu);
    char first_name[32]{};
    std::memcpy(first_name, asset_memory.data(*dir_buffer_address), sizeof(first_name));
    assert(std::string(first_name) == "a-first.txt");

    asset_cpu.set_register(0, dir_handle);
    asset_cpu.set_register(1, *dir_buffer_address);
    assets.handle("AAssetDir_getNextFileName", asset_cpu);
    char second_name[32]{};
    std::memcpy(second_name, asset_memory.data(*dir_buffer_address), sizeof(second_name));
    assert(std::string(second_name) == "z-last.txt");

    asset_cpu.set_register(0, dir_handle);
    asset_cpu.set_register(1, *dir_buffer_address);
    assets.handle("AAssetDir_getNextFileName", asset_cpu);
    assert(asset_cpu.register_value(0) == 0);

    asset_cpu.set_register(0, dir_handle);
    assets.handle("AAssetDir_close", asset_cpu);
    assert(asset_cpu.register_value(0) == 0);

    asset_cpu.set_register(1, *dir_name_address);
    assert(asset_memory.write(*dir_name_address, "empty", 6));
    assets.handle("AAssetManager_openDir", asset_cpu);
    const auto empty_dir_handle = asset_cpu.register_value(0);
    assert(empty_dir_handle != 0);
    asset_cpu.set_register(0, empty_dir_handle);
    asset_cpu.set_register(1, *dir_buffer_address);
    assets.handle("AAssetDir_getNextFileName", asset_cpu);
    assert(asset_cpu.register_value(0) == 0);
    asset_cpu.set_register(0, empty_dir_handle);
    assets.handle("AAssetDir_close", asset_cpu);

    mc3::arm::AndroidFsBridge filesystem_bridge;
    const auto fs_root =
        std::filesystem::temp_directory_path() / "mc3ported-fs-test";
    std::filesystem::remove_all(fs_root, ec);
    assert(filesystem_bridge.configure(
        fs_root / "obb",
        fs_root / "app",
        fs_root / "external",
        fs_root / "data"
    ));

    const auto safe_app = filesystem_bridge.resolve(
        "/data/data/com.gameloft.android.ANMP.GloftM3HM/config/settings.bin"
    );
    assert(safe_app.has_value());
    assert(*safe_app == (fs_root / "app" / "config" / "settings.bin"));

    assert(!filesystem_bridge.resolve(
        "/data/data/com.gameloft.android.ANMP.GloftM3HM/../outside.txt"
    ).has_value());
    assert(!filesystem_bridge.resolve(
        "relative/../../outside.txt"
    ).has_value());
    assert(!filesystem_bridge.resolve(
        "/sdcard/Android/data/com.gameloft.android.ANMP.GloftM3HM/files/../outside.txt"
    ).has_value());
    assert(!filesystem_bridge.resolve(
        "safe/../../outside.txt"
    ).has_value());

    std::filesystem::remove_all(fs_root, ec);

    mc3::arm::DlfcnBridge dlfcn;

    const auto dynamic_image = mc3::loader::parse_arm32_elf(elf_path);
    assert(dynamic_image.has_value());
    assert(dlfcn.configure(
        asset_memory,
        *dynamic_image,
        0
    ));
    assert(dlfcn.handles("dlsym"));
    assert(dlfcn.handles("dlopen"));
    assert(!dlfcn.handles("dlwrite"));

    mc3::loader::GuestMemory thread_memory;
    assert(thread_memory.reserve(0x15000000, 0x4000));

    const std::uint8_t thread_code[] = {
        0x2A, 0x20,
        0x70, 0x47
    };
    assert(thread_memory.write(
        0x15000000,
        thread_code,
        sizeof(thread_code)
    ));

    mc3::arm::PthreadBridge pthread_bridge;
    mc3::arm::GuestCpu thread_control_cpu;
    assert(thread_control_cpu.initialize(&thread_memory));
    assert(pthread_bridge.configure(
        thread_memory,
        [](std::uint32_t, mc3::arm::GuestCpu&) {}
    ));

    const auto thread_out = thread_memory.allocate_tail(4, 4);
    const auto join_out = thread_memory.allocate_tail(4, 4);
    assert(thread_out && join_out);

    thread_control_cpu.set_register(0, *thread_out);
    thread_control_cpu.set_register(1, 0);
    thread_control_cpu.set_register(2, 0x15000001u);
    thread_control_cpu.set_register(3, 123);
    pthread_bridge.handle("pthread_create", thread_control_cpu);
    assert(thread_control_cpu.register_value(0) == 0);

    std::uint32_t thread_handle = 0;
    std::memcpy(
        &thread_handle,
        thread_memory.data(*thread_out),
        sizeof(thread_handle)
    );
    assert(thread_handle >= 2);

    mc3::arm::GuestCpu join_cpu;
    assert(join_cpu.initialize(&thread_memory));
    join_cpu.set_register(0, thread_handle);
    join_cpu.set_register(1, *join_out);
    pthread_bridge.handle("pthread_join", join_cpu);
    assert(join_cpu.register_value(0) == 0);

    std::uint32_t thread_result = 0;
    std::memcpy(
        &thread_result,
        thread_memory.data(*join_out),
        sizeof(thread_result)
    );
    assert(thread_result == 42);

    std::filesystem::remove(elf_path, ec);
    std::filesystem::remove_all(temp, ec);
    std::filesystem::remove_all(asset_root, ec);
    return 0;
}