# TODO

Legend:
- [ ] Not started
- [~] In progress
- [x] Completed
- [!] Blocked / needs investigation

## Phase 0: Project foundation

- [x] Create project architecture and agent guidance.
- [x] Define user-supplied game-data workflow.
- [x] Define sub-100 MB runtime-overhead goal.
- [x] Add CMake project skeleton.
- [x] Add initial CI build for the runtime.
- [ ] Add formatting/static-analysis configuration.
- [x] Add a basic runtime logging system.
- [x] Add a .gitignore for builds and runtime data.

## Phase 1: Reference game/data pipeline

- [ ] Document the supported MC3 Android package/version.
- [x] Add game-data inspection tool.
- [x] Add APK/ZIP asset enumeration.
- [x] Add OBB discovery and validation.
- [x] Add configurable game-data directory.
- [x] Validate ARM ABI and native library presence.
- [x] Detect libModernCombat3.so automatically.
- [x] Add a clean error when game data is missing.
- [x] Keep proprietary game data outside the source tree.
- [ ] Add an APK/OBB setup/import tool.

## Phase 2: ARM execution

- [x] Evaluate ARMv7 user-mode translation/emulation options.
- [x] Select Dynarmic A32 as the initial x86-64 ARMv7 JIT backend.
- [x] Integrate Dynarmic into the build.
- [x] Add an ARMv7 execution smoke test.
- [x] Implement 32-bit ARM ELF parsing and PT_LOAD mapping.
- [x] Inspect PT_DYNAMIC dependencies, symbols, and ARM relocations.
- [x] Apply R_ARM_RELATIVE relocations.
- [x] Add mc3inspect dynamic-import reporting.
- [x] Implement ARM ELF loading into the live Dynarmic guest CPU.
- [x] Implement guest memory mappings.
- [~] Implement relocation/symbol handling.
- [x] Add diagnostics for unresolved imports.
- [x] Load the original MC3 native library through the custom loader.
- [ ] Reach the first real MC3 native entry point.
- [ ] Verify startup without Android installed.

## Phase 3: Dynamic loading / ABI

- [ ] Implement dlopen compatibility.
- [ ] Implement dlsym compatibility.
- [ ] Implement dlclose compatibility.
- [ ] Implement required dlerror behavior.
- [~] Map required Bionic/libc symbols.
- [ ] Map required pthread symbols.
- [~] Map required time/clock functions.
- [ ] Map required memory APIs.
- [ ] Add missing-symbol logging and callsite diagnostics.
- [ ] Add ARM EABI helper compatibility.
- [ ] Add host-address/trampoline support for guest-callable imported functions.

## Phase 4: Minimal Android environment

### JNI

- [ ] Add minimal JNI runtime.
- [~] Implement class lookup.
- [~] Implement method lookup.
- [~] Implement field lookup.
- [~] Implement strings and primitive JNI values.
- [ ] Implement arrays.
- [ ] Implement object/reference handling.
- [~] Add demand-driven JNI stubs from runtime logs.
- [ ] Implement required MC3 callbacks.

### Android filesystem

- [~] Implement logical Android path mapping.
- [~] Implement app-private data path.
- [~] Implement OBB path mapping.
- [~] Implement external-storage path mapping.
- [ ] Implement preferences/config path.
- [~] Prevent uncontrolled writes outside the runtime directory.

### AssetManager

- [ ] Implement asset open/read/seek/close.
- [ ] Implement asset size/remaining-length queries.
- [ ] Implement required directory enumeration.
- [ ] Add APK/OBB-backed asset access.
- [ ] Avoid mandatory full asset extraction.

### Android device/services

- [ ] Implement only required display/device queries.
- [ ] Implement required lifecycle calls.
- [ ] Implement required vibration behavior as a no-op or optional PC haptic mapping.
- [ ] Implement required locale/timezone queries.
- [ ] Implement required notification/service calls as safe PC equivalents or no-ops.

## Phase 5: Graphics

- [ ] Add desktop window bootstrap.
- [ ] Implement EGL compatibility.
- [ ] Create GLES context.
- [ ] Implement buffer swapping.
- [ ] Implement required GLES state.
- [ ] Implement textures.
- [ ] Implement shaders/programs.
- [ ] Implement buffers.
- [ ] Implement framebuffers.
- [ ] Implement required extensions.
- [ ] Add high-resolution output.
- [ ] Add widescreen/aspect-ratio support.
- [ ] Add vsync configuration.
- [ ] Verify title/menu rendering.
- [ ] Verify 3D gameplay rendering.

## Phase 6: Audio

- [ ] Identify MC3 audio API/import requirements.
- [ ] Implement required audio device functions.
- [ ] Implement streaming/buffer submission.
- [ ] Implement pause/resume.
- [ ] Add volume controls.
- [ ] Verify menu audio.
- [ ] Verify in-game sound.
- [ ] Verify music.

## Phase 7: PC input / no-touch UI

- [ ] Create PC input abstraction.
- [ ] Map keyboard movement.
- [ ] Map mouse camera/look.
- [ ] Map primary fire.
- [ ] Map aim.
- [ ] Map reload/action.
- [ ] Map weapon switching.
- [ ] Map pause/menu.
- [ ] Add configurable keybindings.
- [ ] Add gamepad support.
- [ ] Add standard controller layout.
- [ ] Disable mobile touch HUD on PC.
- [ ] Remove/skip touch-only prompts where practical.
- [ ] Ensure mouse cursor/input mode behaves correctly in gameplay.
- [ ] Verify no virtual buttons obscure the game.
- [ ] Add raw-input/mouse sensitivity options.

## Phase 8: PC presentation

- [ ] Add windowed mode.
- [ ] Add borderless fullscreen.
- [ ] Add fullscreen mode where practical.
- [ ] Support common widescreen resolutions.
- [ ] Support arbitrary window sizes.
- [ ] Add graphics settings.
- [ ] Add resolution selection.
- [ ] Add frame-rate/vsync options.
- [ ] Add display scaling/aspect handling.
- [ ] Replace mobile-only presentation where necessary.

## Phase 9: Save/configuration

- [ ] Map Android preferences to PC config.
- [ ] Preserve expected save semantics.
- [ ] Add safe save-directory handling.
- [ ] Test new-game save.
- [ ] Test loading an existing save where compatible.
- [ ] Document save compatibility limitations.

## Phase 10: LAN / Wi-Fi multiplayer

- [ ] Identify original MC3 multiplayer architecture.
- [ ] Identify LAN discovery behavior.
- [ ] Identify lobby/matchmaking protocol.
- [ ] Identify session/version handshake.
- [ ] Capture/document packet structures where legally and technically appropriate.
- [ ] Implement protocol parser.
- [ ] Implement LAN host discovery.
- [ ] Implement LAN join.
- [ ] Implement local host/session creation.
- [ ] Test Windows-to-Windows.
- [ ] Test Windows-to-Android.
- [ ] Test Windows-to-iOS.
- [ ] Test Android-to-iOS through the compatible LAN path.
- [ ] Document version/build restrictions.
- [ ] Avoid assuming cross-platform compatibility without a real test.

## Phase 11: Rehosted multiplayer

- [ ] Determine which original online services are required.
- [ ] Determine which services can be replaced locally.
- [ ] Define a server-side protocol abstraction.
- [ ] Prototype lobby/session server.
- [ ] Prototype room management.
- [ ] Prototype packet relay/state handling if required.
- [ ] Add optional local community server address.
- [ ] Test PC client against the rehosted server.
- [ ] Test Android client where protocol compatibility permits.
- [ ] Test iOS client where protocol compatibility permits.
- [ ] Document authentication/account limitations.
- [ ] Keep server implementation separate from the client runtime.

## Phase 12: Performance / storage

- [ ] Measure clean runtime size.
- [ ] Keep preferred runtime overhead under 50 MB.
- [ ] Enforce hard target of under 100 MB where practical.
- [ ] Avoid mandatory extracted copies of large OBB assets.
- [ ] Profile ARM translation overhead.
- [ ] Profile graphics overhead.
- [ ] Add asset cache limits.
- [ ] Add optional cache cleanup.
- [ ] Profile startup time.
- [ ] Profile frame pacing.
- [ ] Measure Dynarmic code-cache memory separately from shipped disk size.

## Phase 13: Compatibility hardening

- [ ] Test fresh install.
- [ ] Test missing APK.
- [ ] Test missing OBB/data.
- [ ] Test invalid game version.
- [ ] Test corrupted data.
- [ ] Test missing optional dependencies.
- [ ] Test window resize.
- [ ] Test device sleep/focus loss.
- [ ] Test alt-tab.
- [ ] Test controller disconnect/reconnect.
- [ ] Test save corruption handling.
- [ ] Test multiplayer disconnect/reconnect.

## Phase 14: Release

- [ ] Create reproducible Windows build.
- [ ] Package runtime without proprietary game files.
- [ ] Package import/setup tool.
- [ ] Write setup documentation.
- [ ] Write troubleshooting guide.
- [ ] Document supported MC3 game versions.
- [ ] Document multiplayer compatibility.
- [ ] Document known limitations.
- [ ] Publish release artifacts.
- [ ] Verify runtime size against storage goal.
- [ ] Include required third-party notices.

## Current milestone

**Milestone M1: bootstrap + ARMv7 backend**

Completed:
- project architecture and developer guidance;
- CMake runtime/tooling skeleton;
- runtime logging;
- game-data discovery and ARM library validation;
- mc3inspect;
- runtime unit tests;
- CI with Linux and Windows portability jobs;
- Dynarmic A32 integration;
- ARMv7 JIT smoke test;
- live ARM ELF mapping and import trampolines;
- dynamic export lookup and guest function calls;
- guest-addressable JNI table;
- Android path mapping;
- initial libc/memory/string/file compatibility.

Next milestone:

**M3: verified MC3 JNI startup + Android asset layer**

The goal of M3 is to run the real MC3 JNI bootstrap against the supplied v1.1.7g library, trace missing JNI/Android calls, implement AssetManager compatibility, and reach the first stable native game initialization stage.
