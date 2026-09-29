# M8input1 Phase 1 result — 2026-09-27

**Input-only local implementation and static validation complete. Quest validation pending.** No ADB, installation, GameData provisioning, launch or physical test was performed.

## APK

- Package: `org.openxwa.xwaquest.m8input1`
- Version: `0.8.2-input1`; versionCode `1`
- ABI: `arm64-v8a`
- Path: `C:\OpenXWA\XWAQuest\m8input1\app\build\outputs\apk\debug\app-debug.apk`
- Bytes: 13,303,945
- SHA-256: `44AFA97152376FD47A817B6475D77AB4D4DD05B55A8A576F772695702B142857`

## Implemented path

Existing OpenXR Touch actions → input-only adapter → one SDL virtual raw joystick (`XWAQuest Touch Input1`) → Aeron controller snapshot → existing XWA configurable controller mapping → WinMM compatibility provider → native XWA joystick/action processing.

No direct gameplay function calls or simulated entity writes. No renderer/scene/physics/weapon/target/AI/mission changes. The new library links the existing M2 engine/dependencies without recompiling them.

Touch sampling precedes the existing Aeron SDL input pump. Hidden Android window focus is handled through SDL's background joystick hint, while XR action focus/availability explicitly gates the adapter. Controls require neutral after loss/recovery or menu/flight transitions. Native options and persistent mappings remain authoritative; initial defaults are seeded once into the new package's private config. Subsequent user mappings and device deselection are preserved.

## Initial mapping

| Quest input | Native binding |
|---|---|
| Left stick X | `XWA_CONTROLLER_AXIS_YAW` |
| Left stick Y | `XWA_CONTROLLER_AXIS_PITCH` |
| Right stick X | `XWA_CONTROLLER_AXIS_ROLL` |
| Right stick Y | `XWA_CONTROLLER_AXIS_THROTTLE`, persistent input lever |
| Right trigger | 156 **Fire weapon** |
| A | 116 **Next target** |
| B | 27 **Options screen** |
| X | 119 **Cycle weapon settings** |
| Y | 121 **Previous target** |
| Left trigger, both grips, both stick clicks | Exposed as raw buttons, unbound by default |

Forward/back right stick adjusts the throttle input lever at up to 0.5 range/second; center holds. This sets native input, never speed/physics. Initial lever is idle, but the game may retain mission-start throttle until an input bucket changes. Weapon firing follows the currently selected native weapon: no invented primary/secondary actions.

Outside flight, original menu adapter remains: A Return, B Escape, trigger mouse click, left-stick arrows, right-stick cursor, X pilot helper. Virtual gameplay buttons are neutral outside flight except explicit native-binding capture. No Meta/system button is bound. Legacy Y hold-to-exit remains menu-only.

On native binding pages, both stick clicks toggle pointer/navigation mode versus raw capture. Raw capture suppresses pointer clicks and A/X/Y/arrows; B cancels. Toggle back to navigate. This temporary, isolated mode avoids simultaneous menu actions while rebinding. Its ergonomics remain a hardware test item.

## Files

All paths below are relative to **C:\OpenXWA\XWAQuest\m8input1\**. Full paths/hashes are recorded in `evidence/input1-changes.csv`; the complete copied baseline is in `evidence/test3-derived-source.csv`.

Modified relative to copied test3:

1. `input_xr.c`: additional real OpenXR actions, context routing, virtual input calls, binding-page mode, lifecycle.
2. `CMakeLists.txt`: adapter source and input/config-only linker wrappers.
3. `Build-Target.ps1`: private input1 native-build path.
4. `Build-Apk.ps1`: private input1 Gradle cache/output path.
5. `app/build.gradle`: isolated package/version.
6. `settings.gradle`: isolated project name.
7. `app/src/main/AndroidManifest.xml`: input1 label.
8. `app/src/main/java/org/openxwa/xwaquest/m8test2/M8Activity.java`: input1 package declaration/log tag (historical folder name retained; APK activity verified as input1).
9. `tests/Verify-APK.ps1`: input1 identity, packaged/native hash equality, input symbols/imports/providers, all .so ABI/dependency audit.

New implementation and tests:

- `touch_state.h`: pure sample conversion, hysteresis, neutral re-arm and input throttle lever.
- `touch_joystick.h`, `touch_joystick.c`: virtual device, one-time native config seed, diagnostic/native-poll wrappers and context readers.
- `tests/touch_state_test.c`: input conversion/safety tests.
- `tests/native_mapping_test.c`, `tests/Run-Input-Tests.ps1`: tests using exact extracted production XWA/Aeron conversion bodies, not substitute mapping logic.
- `tests/Verify-Frozen.ps1`: read-only source comparison.
- `CHECKPOINT-M8INPUT1.md`, this report, generated test files/evidence/manifests and private build/cache artifacts.

No edits outside m8input1 in this implementation. Copied historical/provisioning helpers are not input1 procedures and must not be run without review/adaptation under separate authorization.

## Validation

- `tests/Run-Input-Tests.ps1`: PASS. Axis ranges/signs, throttle clamps/hold/dt cap, button hysteresis, focus/disconnect neutrality, transition re-arm, capture behavior; real native mapping endpoints, inversion, remapping and focus handling.
- `tests/Run-CPU-Tests.ps1`: PASS. Existing eye math, seat math and VrConvert self-tests.
- `Build-Target.ps1`: ARM64 compile/link PASS, no native warnings/errors. Final incremental pass compiled only the changed adapter translation unit and relinked.
- `Build-Apk.ps1`: BUILD SUCCESSFUL. Warnings: AGP compileSdk compatibility, obsolete Java 8 settings/deprecated inherited APIs, ignored riscv64 metadata. Initial sandbox SDK access failures were resolved by approved execution of the same isolated build, not source/toolchain changes.
- `tests/Verify-APK.ps1`: PASS. Correct package/version/activity/ABI, new input symbols/markers, SDL virtual input imports backed by actual bundled SDL exports. All bundled native files are AArch64; dependencies are packaged or Android system libraries; no Windows dependencies.
- Packaged libOpenXWAM8.so SHA matches current native library after expected stripping: `66D7C00993E337BF861E530792660CD6B950B069ABE985E363BC61D2A9429A4C`.

## Preservation evidence

All 7 pinned test3 sources matched before copying; all 33 design-inventory files also matched at pre-final-build inspection. All 186 frozen source inventory entries matched before builds. Final comparison: **185 unchanged, one externally updated document**: `m8test4/CHECKPOINT-M8TEST4.md`, containing additional UPDATE MUSE findings. This task did not change or restore it. Exact before/after hashes are in the input1 checkpoint and `evidence/frozen-final.csv`.

Every inventoried frozen code file remains unchanged. Ten input1 inherited render/runtime/diagnostic files also match test3 byte-for-byte. Hash scope excludes cache/build/tmp/evidence trees; three inaccessible historical evidence directories were excluded. This is not a claim that every byte of all four directories remained static while another investigator was working.

## Diagnostics and physical test

`M8I_DEVICE`, `M8I_INPUT_READY`, `M8I_BINDINGS` describe initialization. At most one `M8I_SAMPLE` and one `M8I_NATIVE` line per second record XR values → virtual state → selected raw device → values consumed by the native joystick poll. Press/release masks accumulate across the sample interval. Previous native key modifiers/control mask/throttle are explicitly labeled previous state. No marker alone proves actual fire/target/flight success.

After separate install/provision/run authorization:

1. Prepare only input1's package-private assets and capture; preserve test3/test4 and pilots. Launch once after all laptop approvals are complete.
2. Navigate manually to Combat Simulator → Single Player → Quick Skirmish → Flight.
3. Release controls to arm; verify head-only motion does not steer. Test left X yaw, left Y pitch and right X roll independently.
4. Push/pull right Y, then center: native throttle should change and hold without a simulated-state override.
5. Test A/Y target cycling, X weapon settings, RT hold/release firing and B Options. Verify no accidental menu click during flight and no Y flight exit.
6. Check Controller Setup device, use both-stick-click binding mode, change/save one binding, verify it and later persistence.
7. Verify focus loss/recovery with trigger held requires neutral before firing resumes. Observe throttle resume behavior.
8. Correlate physical results with XR/virtual/native diagnostics. Keep known frontend closeness/flicker/starfield issues separate; this experiment does not fix them.

Remaining limits: all Quest behavior untested; native UI rebinding-mode ergonomics and application focus transitions require hardware validation; no haptics, grip-tilt steering, frontend screen, or dedicated secondary-fire mechanism.

**STOPPED BEFORE INSTALL / PROVISION / LAUNCH / ADB.**
