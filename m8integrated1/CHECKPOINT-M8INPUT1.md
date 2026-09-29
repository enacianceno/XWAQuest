# M8input1 — Phase 1 input-only checkpoint

Status: input implementation and ARM64 link successful; CPU adapter/native-mapping/eye/seat/conversion tests PASS. APK packaging/static audit pending. Hardware UNTESTED. No ADB, installation, provisioning or launch authorized in this phase.

## Baseline and boundaries

Isolated source copy of m8test3. All seven test3 source entries in the design SHA-256 inventory matched before copying. `evidence/test3-derived-source.csv` pins all 45 copied files. Historical copied reports describe their original milestones, not input1 results.

`evidence/frozen-source-before.csv` covers 186 existing source/document/test files across m8, m8test2, m8test3, m8test4. Build/cache/tmp/evidence trees are excluded. No frozen files are written. This is a SOURCE verification, not a claim to hash every cache/evidence file; three inaccessible evidence directories were encountered by initial enumeration and excluded. Rendering, cockpit, starfield, frame_bridge, xr_bridge, simulation and cleanup code stay byte-identical to test3.

Identity: org.openxwa.xwaquest.m8input1 / 0.8.2-input1 / versionCode 1 / ARM64-v8a. Independent native build, Gradle cache and private Android storage. Reuse M2 engine archives/SDL/FFmpeg/shaders read-only.

## New/modified files relative to copied baseline (before build)

- `input_xr.c`: exposes trigger/grip/stickclick actions, converts XR samples, gates flight vs frontend/modal input, shuts down virtual device; no system button binding.
- `touch_state.h` NEW: pure input adapter, signed axes, trigger hysteresis, throttle input lever, focus/context neutral re-arm. No simulation writes.
- `touch_joystick.h/.c` NEW: SDL virtual raw joystick lifecycle, package-private one-time native config defaults, raw/native diagnostics, read-only screen-context and native-poll wrappers.
- `CMakeLists.txt`: adds adapter and wraps config loading, native joystick poll and two existing controller-options screens. All original archives/render sources unchanged.
- `Build-Target.ps1`, `Build-Apk.ps1`: independent input1 output/cache paths.
- `app/build.gradle`, `settings.gradle`, `app/src/main/AndroidManifest.xml`, `app/src/main/java/org/openxwa/xwaquest/m8test2/M8Activity.java`: independent package/version/label; historical Java directory retained, package declaration is input1.
- `tests/touch_state_test.c` NEW: conversion/safety CPU tests.
- `tests/native_mapping_test.c`, `tests/Run-Input-Tests.ps1` NEW: extract and exercise exact real XWA/Aeron mapping functions; generated test bodies/hashes remain under input1.
- `tests/Verify-Frozen.ps1` NEW: source hash comparison, never restores changes.
- `tests/Verify-APK.ps1`: input1 identity/native output audit; retains original CS1/T3 checks.
- This checkpoint, input report/tests as added, generated evidence/manifests/build artifacts belong solely to input1.

## Initial exact bindings

Left stick X/Y: native YAW/PITCH (forward stick produces negative pitch input).
Right stick X: native ROLL. Right stick Y: virtual throttle lever, forward increases, back decreases, spring center holds; initial lever zero throttle, max change 0.5 range/second, dt capped at 0.1 s. Native XWA throttle pipeline owns speed/acceleration.

Virtual button 0 RT → 156 **Fire weapon**; 1 A → 116 **Next target**; 2 B → 27 **Options screen**; 3 X → 119 **Cycle weapon settings**; 4 Y → 121 **Previous target**.
5 LT, 6 right grip, 7 left grip, 8 right-stick-click, 9 left-stick-click: exposed, unbound defaults. No invented primary/secondary fire: select native weapon with X then RT.

Menu keeps existing right-stick cursor, left-stick arrows, RT click, A Return, B Escape, X pilot-name helper. Legacy Y exit remains MENU ONLY. No virtual screen changes.

Binding pages: both stick clicks toggle normal pointer/navigation vs raw binding capture. Raw mode freezes pointer and suppresses A/X/Y/arrows while native options see raw device controls; B cancels. Toggle back to navigate. Both-click gesture is consumed and device re-arms only after neutral. This temporary adapter is isolated; original binding/action UI and persisted mapping remain authoritative.

## Lifecycle/configuration/telemetry

Attach raw virtual joystick after SDL initialization during XR input setup. Enable SDL background joystick updates because the hidden Android window is not XR focus; XR action focus/availability still gates the adapter. XR sampling runs before Aeron's event pump, which makes virtual state visible to native snapshots; diagnostics run after it. Loss of XR focus/hand activity releases buttons/centers angular axes. Flight/menu transitions require neutral before re-arm; hangar and frontend modals retain menu input. Throttle lever is retained across focus/menu transitions. Neither head pose nor ship state is used.

Source review confirmed high native Z maps through throttle bucket 16 to Full throttle, so the virtual lever uses signed minimum for idle and maximum for full. This corrects an initial local adapter sign before the final build, without changing native throttle logic. The native engine may retain mission-start throttle until lever movement causes a bucket change: lever initial idle is not a direct command to overwrite the mission's starting speed.

Native host-config wrapper loads real config, sets Quest factory defaults, seeds current options only once if no selected device, saves through native host-config API, and writes a private `m8input1-bindings-v1` marker. Later custom bindings or deliberate device deselection are preserved. Existing selected devices are not overwritten. Failure is explicit, not silent fallback.

M8I_DEVICE, M8I_BINDINGS, M8I_INPUT_READY establish startup. M8I_SAMPLE (at most 1 Hz) records XR axes/buttons, virtual axes, pressed/released-window masks, selected device and raw state. M8I_NATIVE (at most 1 Hz) observes real native joystick-poll return, consumed axes/buttons and action codes; previous_native_keymods/controlmask/throttle describe the preceding native input processing, not current completed weapon effects. Markers prove routing, not successful gameplay; Quest physical validation remains required.

Pre-final-build source verification: 186/186 frozen source files unchanged; 33/33 design inventory sources unchanged. Initial CMake execution was sandbox-denied; the same authorized isolated build ran with approved tool execution permissions. Native build has no compiler/linker warnings or errors. Existing engine/dependency archives were linked without rebuilding them.

## Physical test plan — later authorization required

1. Install only input1 and provision its own assets with separate authorization; never overwrite test3/test4 or their pilots. Start capture before one launch.
2. Confirm existing menu navigation and create/select a private test pilot. Enter Combat Simulator → Single Player → Quick Skirmish → Flight manually.
3. Release all controls to arm. Move only the head: view changes, ship must not steer. Briefly deflect left X, left Y and right X individually; verify yaw/pitch/roll and return to neutral.
4. Push right Y briefly and release: throttle increases then holds. Pull back to decrease; check engine-native speed behavior rather than instant physics overrides.
5. A/Y cycle native targets. X cycles weapon settings. RT fires selected weapon and stops on release. Confirm weapon/ammo rules remain native. B opens original Options; no simultaneous fire/click or legacy Y flight exit.
6. Options → Controller Setup shows XWAQuest Touch Input1. Toggle both stick clicks for raw capture only when rebinding; alter a binding through native UI, leave/save and later verify persistence.
7. Pause/focus loss and return with trigger held: no firing until controls released/re-armed. Retained throttle resumes via original pipeline; verify no unexpected spike.
8. Compare XR → virtual → native logs and physical observations. Existing flicker, close UI and missing stars are known baseline issues, not addressed here.

STOP after local build/static audit. No Quest action in this phase.
