include(FetchContent)

# Dynarmic is used only as an ARM CPU execution/JIT component.
# The guest Android environment, ELF loader, JNI, and platform shims remain ours.
set(DYNARMIC_FRONTENDS "A32" CACHE STRING "Dynarmic guest frontends" FORCE)
set(DYNARMIC_TESTS OFF CACHE BOOL "Disable Dynarmic own tests" FORCE)
set(DYNARMIC_TESTS_USE_UNICORN OFF CACHE BOOL "Disable Dynarmic Unicorn tests" FORCE)
set(DYNARMIC_USE_LLVM OFF CACHE BOOL "Disable Dynarmic LLVM support" FORCE)
set(DYNARMIC_USE_PRECOMPILED_HEADERS OFF CACHE BOOL "Keep dependency setup simple" FORCE)
set(DYNARMIC_USE_BUNDLED_EXTERNALS ON CACHE BOOL "Use Dynarmic bundled dependencies" FORCE)

FetchContent_Declare(
    dynarmic
    GIT_REPOSITORY https://github.com/azahar-emu/dynarmic.git
    GIT_TAG e77b1ba0b7da7cbe93021b01a663acfe7c4dd516
    GIT_SUBMODULES
        externals/fmt
        externals/mcl
        externals/robin-map
        externals/xbyak
        externals/zycore
        externals/zydis
)

FetchContent_MakeAvailable(dynarmic)
