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
- [ ] Add CMake project skeleton.
- [ ] Add CI build for the PC runtime.
- [ ] Add formatting/static-analysis configuration.
- [ ] Add a basic runtime logging system.

## Phase 1: Reference game/data pipeline

- [ ] Document the supported MC3 Android package/version.
- [ ] Add APK inspection tool.
- [ ] Add APK/ZIP asset enumeration.
- [ ] Add OBB discovery and validation.
- [ ] Add configurable game-data directory.
- [ ] Validate ARM ABI and native library presence.
- [ ] Detect required native libraries automatically.
- [ ] Add a clean first-run error when game data is missing.
- [ ] Keep proprietary game data outside the source tree.

## Phase 2: ARM execution

- [ ] Evaluate ARMv7 user-mode translation/emulation options.
- [ ] Select the smallest suitable runtime.
- [ ] Integrate the ARM32 execution backend.
- [ ] Implement ARM ELF loading.
- [ ] Implement required memory mappings.
- [ ] Implement relocation/symbol handling.
- [ ] Add diagnostics for unresolved imports.
- [ ] Load the original MC3 native library.
- [ ] Reach the first native entry point.
- [ ] Verify startup without Android installed.

## Phase 3: Dynamic loading / ABI

- [ ] Implement `dlopen` compatibility.
- [ ] Implement `dlsym` compatibility.
- [ ] Implement `dlclose` compatibility.
- [ ] Implement required `dlerror` behavior.
- [ ] Map required Bionic/libc symbols.
- [ ] Map required pthread symbols.
- [ ] Map required time/clock functions.
- [ ] Map required memory APIs.
- [ ] Add missing-symbol logging and callsite diagnostics.

## Phase 4: Minimal Android environment

### JNI

- [ ] Add minimal JNI runtime.
- [ ] Implement class lookup.
- [ ] Implement method lookup.
- [ ] Implement field lookup.
- [ ] Implement strings and primitive JNI values.
- [ ] Implement arrays.
- [ ] Implement object/reference handling.
- [ ] Add demand-driven JNI stubs from runtime logs.
- [ ] Implement required MC3 callbacks.

### Android filesystem

- [ ] Implement logical Android path mapping.
- [ ] Implement app-private data path.
- [ ] Implement OBB path mapping.
- [ ] Implement external-storage path mapping.
- [ ] Implement preferences/config path.
- [ ] Prevent uncontrolled writes outside the runtime directory.

### AssetManager

- [ ] Implement asset open/read/seek/close.
- [ ] Implement asset size queries.
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
- [ ] Document any version/build restrictions.
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

## Current milestone

**Milestone M0: Architecture and repository foundation**

Done:

- project goal defined;
- minimal-Android strategy defined;
- storage constraint defined;
- PC-control target defined;
- multiplayer target defined;
- architecture document created;
- agent/developer guidance created.

Next milestone:

**M1: executable + game-data pipeline + ARMv7 runtime proof-of-concept**
