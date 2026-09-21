# MC3 Ported

A native-PC compatibility runtime for Modern Combat 3: Fallen Nation.

The project follows the broad approach of the public MC3 Vita loader: host the original Android ARMv7 native game code in a small targeted compatibility environment instead of installing a complete Android system.

## Current status

The repository currently contains the M1 runtime foundation:

- ARMv7 ELF parsing/loading;
- guest memory management;
- Dynarmic A32 CPU backend;
- import trampoline scaffolding;
- minimal APK inspection/materialization;
- extracted-directory game-data support;
- runtime diagnostics;
- automated Linux CI.

This is not yet a playable PC port. Graphics, JNI, Android filesystem APIs, audio, PC input, and the actual MC3 startup path are still under development.

## Game data

The reference target is Modern Combat 3 v1.1.7g.

The launcher accepts either an extracted game-data directory or the .apk itself.

When given an APK, the runtime extracts only the MC3 native library into its cache. It does not install Android and does not unpack the entire APK.

Example:

    mc3.exe --game-data "C:\\Games\\MC3\\Modern-Combat-3-Fallen-Nation-v1-1-7g.apk"

The runtime cache is controlled with --runtime-root.

## Storage goal

The compatibility runtime is designed to stay small:

- preferred extra runtime overhead: under 50 MB;
- hard target: under 100 MB;
- the APK/OBB game data is not copied into the runtime cache wholesale.

The native .so is temporarily materialized into the cache because the current loader operates on a normal ELF file. A later milestone can eliminate even that copy by loading the ELF directly from the APK archive.

## Developer tools

mc3inspect can inspect an APK, an extracted game-data directory, an ARM ELF library, or the configured runtime backend.

## Multiplayer and controls

The end goal is a PC-native-feeling game:

- keyboard/mouse controls;
- gamepad controls;
- no touchscreen control overlay;
- widescreen/resizable output;
- LAN/Wi-Fi multiplayer where the original protocol permits it;
- compatibility with Android/iOS clients where the protocol can be reproduced;
- an optional community/rehosted server path.

Those features are planned but are not yet implemented.

## Legal/data boundary

The repository contains compatibility/runtime code and tooling. It does not need to redistribute the original MC3 APK, OBB files, native library, or other proprietary game assets.

Users should supply their own compatible game data.

## Reference

The architecture is informed by the public MC3 Vita loader by v-atamanenko, which targets the Android 1.1.7g release and uses the ARMv7 libModernCombat3.so from the APK.
