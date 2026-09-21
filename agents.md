# Agents Guide

## Project mission

MC3 Ported is a native-PC compatibility project for **Modern Combat 3: Fallen Nation**. The long-term goal is to run the original Android ARMv7 game code on Windows through a small, purpose-built Android compatibility environment, rather than installing or embedding a full Android operating system.

The project should ultimately feel like a normal PC game:

- keyboard and mouse are first-class controls;
- gamepads are supported;
- touchscreen controls are hidden/disabled on PC;
- the game uses resizable/widescreen PC resolutions;
- PC-oriented menus and input behavior are used where practical;
- multiplayer can work over local Wi-Fi/LAN and, where protocol-compatible, between PC, Android, and iOS clients;
- an optional rehosted/community server path may be added where the original service is unavailable.

## Source/data policy

The repository may contain compatibility code and tooling, but the original game binary/data must remain user-supplied unless its redistribution is clearly authorized.

The current reference asset repository is:

- `yoimdoingstuff/mc3assets`
- contains `Modern-Combat-3-Fallen-Nation-v1-1-7g.apk`

Do not copy the APK, OBB files, extracted proprietary assets, or proprietary symbols into this repository as source-controlled generated files. Development tooling should support importing a user-owned APK/OBB/data set from a configurable path.

## Core technical strategy

Follow the architecture demonstrated by the public MC3 Vita loader as a reference, not as a package to blindly copy.

The PC runtime should provide only the interfaces MC3 actually needs:

1. ARMv7 execution/translation for the original Android native library.
2. A minimal JNI implementation.
3. Android/Bionic-style libc/POSIX compatibility where required.
4. Dynamic-loading compatibility (`dlopen`, `dlsym`, etc.).
5. Android-style filesystem/path translation.
6. AssetManager-compatible access to packaged game data.
7. EGL/OpenGL ES compatibility mapped to a desktop graphics backend.
8. Audio compatibility.
9. Input/lifecycle compatibility.
10. A native PC networking layer where protocol work permits it.

Do not build a general Android emulator. Avoid shipping an Android system image, ART/Dalvik runtime, Package Manager, SystemUI, Google Play Services, or unrelated framework components.

## Storage budget

Target runtime overhead beyond the user-supplied game data:

- preferred: under 50 MB;
- hard design target: under 100 MB;
- no complete Android installation.

Dependencies must be selected and built as narrowly as practical.

## Development rules

### 1. Implement what MC3 needs

Do not speculate about the whole Android API. Add compatibility functions based on observed imports, runtime logs, crashes, traces, or behavior from the original game.

### 2. Keep platform code separate

Use clear boundaries between:

- `platform/windows`
- `runtime/arm`
- `runtime/jni`
- `runtime/android`
- `runtime/graphics`
- `runtime/audio`
- `runtime/input`
- `runtime/network`
- `tools`

Do not bury Windows-specific behavior inside the generic compatibility layer.

### 3. Preserve original game behavior

The original game library should be treated as the reference executable. Avoid modifying its code unless a documented compatibility patch is necessary.

Patches must be:

- small;
- isolated;
- documented;
- reproducible;
- disabled by default unless required.

### 4. Prefer diagnostics over guessing

Every missing symbol, JNI method, Android service, filesystem path, graphics call, or network operation should be logged with enough context to implement it properly.

A useful failure message is better than a silent crash.

### 5. Test incrementally

Required milestones should be testable independently:

- executable starts;
- APK/data discovery works;
- ARM library loads;
- imports resolve;
- JNI initializes;
- graphics context initializes;
- audio initializes;
- MC3 engine initializes;
- title/menu reaches a usable state;
- gameplay starts;
- input is fully usable;
- multiplayer transport works.

### 6. No touchscreen overlay on PC

The PC build must not display virtual buttons, touch HUD elements, or other mobile-only control overlays unless explicitly enabled for debugging.

Mouse/keyboard/controller input should be translated directly into the game's input model.

### 7. Multiplayer compatibility is a protocol problem

Do not assume LAN compatibility merely because packets can be sent.

Document:

- discovery;
- matchmaking/lobby;
- player/session identity;
- packet formats;
- version checks;
- reliability/ordering;
- NAT/internet requirements;
- server dependencies;
- Android/iOS interoperability.

Any rehosted-server implementation must be kept separate from the compatibility runtime.

### 8. Do not fake completion

A task is not complete merely because the code compiles. Mark functionality complete only after a reproducible runtime test exists.

## Preferred implementation style

- C/C++ for the runtime and compatibility layers.
- CMake for builds.
- Keep external dependencies minimal.
- Use SDL3 for the PC window/input/audio/platform layer where it simplifies the implementation.
- Prefer an established ARMv7 user-mode translation/emulation component over writing a CPU translator from scratch.
- Keep graphics translation behind an explicit interface so the backend can evolve without changing MC3-facing compatibility code.

## Commit guidance

Use focused commits such as:

- `runtime: add ELF loader skeleton`
- `jni: implement class/method lookup`
- `android: add AssetManager compatibility`
- `graphics: add EGL bootstrap`
- `input: map mouse to camera controls`
- `network: add LAN discovery prototype`

Avoid giant commits that mix runtime, renderer, UI, and networking changes.

## Definition of success

The project is successful when a user can point the launcher at their own compatible MC3 Android game data and start MC3 directly on Windows without installing Android, with normal PC controls and no touchscreen HUD obstructing play.

Multiplayer success requires a separately verified protocol path for LAN/Wi-Fi and, where technically possible, cross-platform clients.
