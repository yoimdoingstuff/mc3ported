# Architecture

## 1. Overview

MC3 Ported is not an Android emulator. It is a **targeted compatibility runtime** that hosts the original Android ARMv7 MC3 native code on a desktop OS.

The architectural model is:

```
User-provided MC3 APK/OBB/data
            |
            v
      Asset/Data Loader
            |
            v
    Original ARMv7 game code
            |
            v
      ARMv7 execution layer
            |
            v
     Compatibility ABI
   +--------+---------+---------+
   |        |         |         |
  JNI     libc      EGL/GLES  Android IO
   |        |         |         |
   +--------+---------+---------+
            |
            v
       PC platform layer
   +--------+---------+---------+
   |        |         |         |
  SDL3   Graphics   Audio    Networking
   |        |         |         |
   +--------+---------+---------+
            |
            v
         Windows
```

The exact third-party libraries are implementation choices. The boundaries above should remain stable.

## 2. Why this architecture

The public MC3 Vita loader demonstrates that MC3 can be hosted by replacing Android-facing dependencies with native implementations. Its project contains a custom shared-object loader, FalsoJNI integration, Android-style asset handling, POSIX/Bionic compatibility, and EGL-related reimplementations.

The PC port can use the same fundamental concept while replacing Vita APIs with Windows/SDL3/native APIs.

The largest difference is CPU architecture:

- Vita: ARM-based, so the ARMv7 game code can execute natively.
- Typical Windows PC: x86-64, so an ARMv7 translation/emulation layer is required.

That ARM execution layer is therefore a first-class subsystem rather than part of the Android compatibility shim.

## 3. Repository layout

The current implementation is intentionally small. The loader, JNI, Android shims, graphics, audio, input, and networking are added as separate subsystems rather than hidden inside one platform file.

Recommended source tree:

```
mc3ported/
├── agents.md
├── architecture.md
├── TODO.md
├── CMakeLists.txt
├── cmake/
├── docs/
├── launcher/
├── runtime/
│   ├── arm/
│   ├── loader/
│   ├── jni/
│   ├── android/
│   ├── libc/
│   ├── graphics/
│   ├── audio/
│   ├── input/
│   ├── filesystem/
│   └── networking/
├── platform/
│   └── windows/
├── tools/
│   ├── apk/
│   ├── obb/
│   └── inspection/
├── tests/
│   ├── unit/
│   ├── integration/
│   └── compatibility/
└── third_party/
```

Original proprietary game data should not be vendored into the repository.

## 4. Launcher

The launcher is a small native executable responsible for:

1. locating the user-provided game data;
2. validating the expected MC3 package/version;
3. constructing the virtual Android filesystem;
4. loading runtime configuration;
5. starting the ARM execution layer;
6. initializing the compatibility services;
7. launching the original MC3 native entry point.

The launcher should expose simple configuration rather than Android-like package management.

Example layout:

```
MC3-PC/
├── mc3.exe
├── runtime/
├── data/
├── saves/
└── config/
```

## 5. ARMv7 execution layer

Responsibilities:

- load ARM ELF/shared objects;
- resolve relocations/imports;
- provide memory mappings;
- execute ARMv7 code on x86-64;
- expose hooks for diagnostics;
- provide controlled calls between ARM code and native host shims.

The project currently uses Dynarmic's A32 JIT as the ARMv7 execution backend. The pinned revision is e77b1ba0b7da7cbe93021b01a663acfe7c4dd516. Only the A32 frontend is enabled. The current integration only proves CPU execution with a smoke test; it does not yet load MC3's ELF.

The execution layer must not depend on the existence of a complete Android userspace. Dynarmic only owns guest CPU execution and callbacks into our guest memory system.

## 6. Dynamic loader

The loader replaces the role normally performed by Android's dynamic linker for the game process.

Responsibilities:

- parse ELF headers;
- load required segments;
- resolve symbols;
- apply relocations;
- bind game imports to compatibility implementations;
- expose `dlopen`, `dlsym`, `dlclose`-style behavior as required.

The loader should produce useful logs for missing libraries and symbols.

## 7. Imported-symbol ABI

The dynamic loader needs two different kinds of resolution:

1. guest symbols defined inside MC3 itself, which resolve to the loaded guest image;
2. undefined symbols imported from Android/Bionic, EGL/GLES, sockets, libc, compiler runtime, AssetManager, and similar APIs.

Imported host functions cannot simply write a 64-bit Windows function pointer into a 32-bit ARM relocation. The project therefore needs a guest-callable trampoline/bridge layer. That layer will own the ABI conversion between ARM registers/stack and native host calls.

## 8. Minimal JNI

MC3 may expect Java/JNI services even though the game logic is primarily native.

The JNI layer should implement only the classes/methods/fields actually requested.

Likely categories include:

- class/method lookup;
- strings;
- arrays;
- object references;
- primitive values;
- lifecycle calls;
- filesystem/path helpers;
- preferences/configuration;
- display/device queries;
- callbacks requested by the native library.

The implementation should follow a demand-driven model:

```
MC3 requests symbol
       |
       v
known compatibility shim? -- no --> log + fail clearly
       |
      yes
       |
       v
native host implementation
```

## 9. Android filesystem

Present MC3 with Android-like logical paths while storing data in normal desktop directories.

Example mapping:

```
Android logical path                 PC storage
----------------------------------------------------------
/data/data/<package>/                data/app/
/sdcard/Android/obb/<package>/       data/obb/
/sdcard/...                           data/sdcard/
save/config data                     saves/
```

The mapping must be configurable and must not write to arbitrary system locations.

## 10. AssetManager

The compatibility layer should expose Android-style asset access over extracted or mounted game data.

Requirements:

- open asset;
- close asset;
- read;
- seek;
- size;
- existence checks;
- directory enumeration where required.

A tool should be provided to inspect APK/OBB contents without modifying the original files.

The runtime may either:

1. read directly from APK/OBB containers where practical; or
2. use an extracted cache.

The extracted cache must be optional because storage is a project constraint.

## 11. Graphics

MC3 expects mobile OpenGL ES/EGL behavior.

Architecture:

```
MC3 GLES/EGL calls
        |
        v
graphics compatibility layer
        |
        v
desktop graphics backend
```

The compatibility layer should emulate the mobile-facing API rather than rewriting individual game rendering calls.

Initial priority:

- EGL initialization/context;
- surface creation;
- buffer swapping;
- texture management;
- shaders/programs;
- vertex/index buffers;
- framebuffer objects;
- common fixed/mobile-era GLES state.

The backend should support high-resolution and widescreen output without changing the game's logical coordinate assumptions until required.

## 12. Audio

Provide the Android-era audio behavior expected by the game while using a desktop audio backend.

Responsibilities:

- device initialization;
- buffer submission;
- streaming;
- latency management;
- pause/resume;
- volume.

SDL3 can provide the host-side audio device abstraction if it proves adequate.

## 13. PC input model

Input is deliberately different from the mobile UI.

### Keyboard/mouse

First-class mappings should cover:

- movement;
- camera/look;
- fire;
- aim;
- reload;
- jump/action;
- weapon switching;
- pause/menu;
- interaction.

Mouse should directly control camera/look where the game's input model permits.

### Gamepad

Support standard desktop gamepad mappings.

### Touchscreen

Disabled by default on PC.

The runtime may still expose the underlying touch-related JNI/input calls when MC3 requires them, but these should be synthesized from keyboard/mouse/gamepad input rather than drawing touch buttons over the game.

## 13. Display and PC presentation

The runtime should provide:

- windowed mode;
- borderless fullscreen;
- exclusive fullscreen where practical;
- arbitrary common resolutions;
- aspect-ratio handling;
- vsync configuration;
- frame pacing;
- configurable field-of-view/camera adjustments only where required for a good PC presentation.

Do not simply stretch a fixed phone framebuffer if the renderer can operate at a real desktop resolution.

## 15. Networking and multiplayer

Networking is split into three layers:

```
MC3 protocol compatibility
          |
          v
cross-platform transport
          |
          +---- LAN/Wi-Fi
          |
          +---- optional internet/rehosted server
```

### LAN/Wi-Fi goal

The PC client should be capable of joining or hosting a compatible local session where the original protocol allows it.

Cross-platform target:

- Windows PC
- Android
- iOS

The important requirement is protocol compatibility, not merely IP connectivity. If the original mobile clients use broadcast/multicast discovery or a particular matchmaking handshake, the implementation must reproduce that behavior or provide a compatible bridge.

### Rehosted servers

If the original online infrastructure is unavailable, investigate a separate community server implementation or protocol bridge.

Keep server code out of the client runtime where possible.

Potential server components:

- discovery/lobby;
- authentication/session identity;
- room management;
- match state;
- packet relay;
- NAT/internet support if required.

Do not claim compatibility until captured/reference traffic and multiple clients have been tested.

## 16. Diagnostics

Every subsystem should support structured logging.

Example categories:

```
[loader]
[jni]
[android]
[fs]
[asset]
[gfx]
[audio]
[input]
[net]
[arm]
```

A missing compatibility function should log:

- symbol/class/method name;
- caller/module where practical;
- arguments where safe;
- whether execution continued or failed.

This is essential because the project is intentionally implementing a subset rather than all of Android.

## 17. Testing architecture

### Unit tests

Test compatibility primitives independently:

- path mapping;
- JNI type handling;
- asset reads;
- ELF parsing;
- symbol resolution;
- network packet parsing.

### Integration tests

Verify startup stages independently:

```
launcher
  -> APK discovery
  -> data mount
  -> ELF load
  -> imports
  -> JNI
  -> graphics/audio
  -> engine init
```

### Runtime tests

Milestones:

1. process starts;
2. game library loads;
3. engine initializes;
4. main menu renders;
5. input works without touchscreen HUD;
6. a mission loads;
7. audio works;
8. save/load works;
9. LAN discovery works;
10. multiplayer session works;
11. Android/iOS interop works where protocol-compatible.

## 18. Distribution model

The project should distribute:

- PC runtime;
- compatibility code;
- launcher;
- tools;
- documentation.

The project should not automatically distribute the original proprietary MC3 APK/OBB/game assets unless redistribution rights are established.

The launcher should accept a user-provided game package/data directory.

## 19. Future portability

The compatibility interfaces should be designed so that the same core runtime can eventually target:

- Windows x64;
- Linux x64;
- ARM64 desktop/handheld Linux.

Platform-specific code belongs behind the platform layer, not inside MC3 compatibility logic.
