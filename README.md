# MC3 Ported

A native-PC compatibility project for Modern Combat 3: Fallen Nation.

The goal is to run the original Android ARMv7 game through a small purpose-built compatibility runtime instead of installing a complete Android environment.

## Current status

The repository is now in the M1 bootstrap stage.

Implemented:

- project architecture and agent guidance;
- CMake build skeleton;
- runtime logging;
- configurable game-data discovery;
- detection of the MC3 ARMv7 native library;
- developer game-data inspection tool;
- initial runtime tests;
- Dynarmic A32 ARMv7 JIT integration and smoke test;
- pinned external dependency configuration;
- initial Linux CI.

Not implemented yet:

- ARM ELF loading;
- Android/Bionic compatibility;
- minimal JNI;
- Android AssetManager;
- EGL/OpenGL ES bridge;
- audio;
- PC input;
- actual MC3 startup;
- multiplayer.

## Game data

The project expects the user to provide compatible MC3 game data.

The development reference asset repository is:

https://github.com/yoimdoingstuff/mc3assets

Do not add the original APK/OBB or extracted proprietary game data to this repository as normal source files.

The eventual setup tool will accept an APK/OBB directory and prepare the runtime automatically.

For the current bootstrap, pass an extracted directory containing:

    lib/armeabi-v7a/libModernCombat3.so

Example:

    mc3 --game-data C:\path\to\mc3-data

or:

    mc3inspect C:\path\to\mc3-data
    mc3inspect --runtime

## Build

Requires CMake 3.20+ and a C++20-capable compiler.

    cmake -S . -B build
    cmake --build build
    ctest --test-dir build --output-on-failure

The default build uses Dynarmic for ARMv7 execution. Set MC3_ENABLE_DYNARMIC=OFF to build the host/tooling skeleton without that backend.

Dynarmic is fetched at a pinned commit and configured with only the A32 frontend for this project.

## Runtime design

    Original MC3 ARMv7 code
            |
            v
    ARMv7 execution/JIT layer
            |
            v
    Minimal Android compatibility layer
            |
            +-- JNI
            +-- libc/POSIX
            +-- Android filesystem
            +-- AssetManager
            +-- EGL/GLES
            +-- audio
            +-- input
            |
            v
    PC platform layer
            |
            +-- Windows
            +-- keyboard/mouse
            +-- gamepad
            +-- LAN/Wi-Fi

No complete Android system image is intended.

## Storage goal

The target is under 50 MB preferred runtime overhead, with 100 MB as the hard design ceiling, excluding the user-supplied game data and optional caches.

Dynarmic's JIT code cache is memory rather than a shipped Android system image. The project will measure the final packaged executable/runtime size before release.

## Multiplayer goal

The long-term target is:

- PC to PC LAN/Wi-Fi;
- PC to Android LAN/Wi-Fi;
- PC to iOS LAN/Wi-Fi;
- Android to iOS where the original protocol permits it;
- optional community rehosted servers if the original service protocol can be reproduced.

Protocol compatibility must be tested rather than assumed.

## Reference implementations

The architecture is informed by the public MC3 Vita loader:

https://github.com/v-atamanenko/mc3-vita

The ARM execution layer currently uses Dynarmic's A32 JIT backend:

https://github.com/azahar-emu/dynarmic

The PC runtime is a separate implementation and does not require a Vita installation.

See agents.md, architecture.md, and TODO.md for the engineering roadmap.
