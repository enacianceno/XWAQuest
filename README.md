# XWAQuest

Experimental standalone Meta Quest port of X-Wing Alliance using OpenXWA,
SDL GPU/Vulkan and OpenXR. This repository preserves development milestones,
build/provisioning scripts, tests and project notes.

- Integrated M8 progress: [checkpoint](m8integrated1/CHECKPOINT-M8INTEGRATED1.md).
- Repository scope and external dependencies: [preparation notes](GITHUB_PREPARATION.md).
- Original game data, pilots, APKs and local build outputs are not included.
- M8 depends on a separate, locally modified OpenXWA checkout and generated
  libraries. This repository alone is not yet a reproducible build backup.
- Build results and runtime logs do not replace visual validation in the headset.

The original M1 instructions are retained below as milestone documentation.

## XWAQuest M1

Independent Android ARM64 SDL_GPU/Vulkan/OpenXR smoke test. No OpenXWA engine or original game data is linked or packaged. Package: org.openxwa.xwaquest. hello_xr remains unchanged.

## Reproducible inputs

- SDL3: 8f8ed757bc94d2e097aab8c4c0f58b57dcee9871, local ../third_party/SDL3. CMake verifies HEAD. Override with -PsdlRoot=<absolute path>.
- SDL test/testgpu_spinning_cube_xr.c adapted into app/src/main/cpp/main.c. Upstream sources are not modified. Original notice retained; SDL-LICENSE.txt applies to the derived sample. The modifications are the XWAQuest M1 adaptation, not an upstream SDL version.
- SDL Java sources and precompiled cube SPIR-V headers come from that same checkout. No shader compiler required for this milestone.
- Gradle 8.5; Android Gradle plugin 8.1.4; JDK 21 (tested with Temurin 21.0.12.101).
- NDK 28.2.13676358; CMake 3.22.1; compile SDK 35; target 34; min 29; arm64-v8a only.
- Khronos OpenXR loader Maven artifact 1.1.43 (Apache-2.0).

Set JAVA_HOME to JDK 21 and ANDROID_HOME to the SDK, or set sdk.dir in untracked local.properties. Run:

    .\gradlew.bat :app:assembleDebug --console=plain

Output: app/build/outputs/apk/debug/app-debug.apk

The default SDL path is relative to app/. This standalone project does not link its enclosing OpenXWA checkout; it needs only the pinned SDL dependency. The local build uses a pre-existing Gradle cache and downloads missing pinned SDK/dependency packages as needed.

## Scope and visual test

A single 0.5-metre colored cube rotates approximately 2 metres in front of the initial LOCAL origin, on a dark blue background. Stereo uses separate SDL GPU render passes and XR swapchains per eye. There is no engine, game data, audio, input action set or controller mapping.

Logs use tag XWAQuest: BOOT/BUILD, XR session transitions, formats, swapchains, and FRAME every 300 successfully submitted frames. Frame submissions prove runtime acceptance, not human visual correctness.

Check that the cube is solid, colored and stereoscopic; look around gently and check that it remains in world space. Open/close the system menu and check recovery. Confirm visually before closing M1. Do not start M2 without explicit authorization.

Local diagnostic outputs are in evidence/ and are not committed. M0 backup is ../milestones/M0-20260918; it excludes original game assets and APKs containing them.
