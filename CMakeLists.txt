cmake_minimum_required(VERSION 3.20)

project(mc3ported
    VERSION 0.1.0
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MC3_ENABLE_TESTS "Build MC3 Ported tests" ON)
option(MC3_ENABLE_TOOLS "Build MC3 Ported developer tools" ON)
option(MC3_ENABLE_DYNARMIC "Enable the ARMv7 JIT backend" ON)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(mc3_miniz STATIC
    third_party/miniz/miniz.c
    third_party/miniz/miniz_zip.c
    third_party/miniz/miniz_tdef.c
    third_party/miniz/miniz_tinfl.c
)

target_include_directories(mc3_miniz
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniz
)

if(MC3_ENABLE_DYNARMIC)
    include(cmake/Dependencies.cmake)
endif()

add_library(mc3_runtime
    src/runtime/config.cpp
    src/runtime/apk_archive.cpp
    src/runtime/game_data.cpp
    src/runtime/log.cpp
    src/runtime/arm/arm_runtime.cpp
    src/runtime/arm/guest_cpu.cpp
    src/runtime/loader/elf32.cpp
    src/runtime/loader/imports.cpp
    src/runtime/loader/memory.cpp
)

target_include_directories(mc3_runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(mc3_runtime PUBLIC mc3_miniz)

if(MC3_ENABLE_DYNARMIC)
    target_compile_definitions(mc3_runtime PUBLIC MC3_HAVE_DYNARMIC=1)
    target_link_libraries(mc3_runtime PUBLIC dynarmic)
endif()

add_executable(mc3
    src/main.cpp
)

target_link_libraries(mc3 PRIVATE mc3_runtime)

if(WIN32)
    target_compile_definitions(mc3_runtime PUBLIC MC3_PLATFORM_WINDOWS=1)
endif()

if(MC3_ENABLE_TOOLS)
    add_executable(mc3inspect
        tools/mc3inspect/main.cpp
    )
    target_link_libraries(mc3inspect PRIVATE mc3_runtime)
endif()

if(MC3_ENABLE_TESTS)
    enable_testing()

    add_executable(mc3_runtime_tests
        tests/runtime_tests.cpp
    )
    target_link_libraries(mc3_runtime_tests PRIVATE mc3_runtime)

    add_test(NAME mc3_runtime_tests COMMAND mc3_runtime_tests)
endif()

install(TARGETS mc3 DESTINATION bin)

if(MC3_ENABLE_TOOLS)
    install(TARGETS mc3inspect DESTINATION bin)
endif()
