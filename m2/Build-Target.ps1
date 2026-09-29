param([string]$SdkRoot = "$env:LOCALAPPDATA/Android/Sdk")
$ErrorActionPreference = 'Stop'
$m2Root = $PSScriptRoot.Replace('\','/')
$ndk = "$SdkRoot/ndk/28.2.13676358"
$cmake = "$SdkRoot/cmake/3.22.1/bin/cmake.exe"
# Required by Windows host shader tools invoked from the Android Ninja build.
$env:PATH = "$m2Root/host/bin;$m2Root/host/llvm-mingw-20260908-ucrt-x86_64/bin;$env:PATH"
& $cmake -S $m2Root -B "$m2Root/build-android" -G Ninja `
    "-DCMAKE_MAKE_PROGRAM=$SdkRoot/cmake/3.22.1/bin/ninja.exe" `
    "-DCMAKE_TOOLCHAIN_FILE=$ndk/build/cmake/android.toolchain.cmake" `
    '-DANDROID_ABI=arm64-v8a' '-DANDROID_PLATFORM=android-29' `
    '-DANDROID_STL=c++_shared' '-DCMAKE_BUILD_TYPE=RelWithDebInfo' `
    '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON' 2>&1 | Tee-Object "$m2Root/evidence/target-configure.log"
if ($LASTEXITCODE) { throw 'Android configure failed' }
& $cmake --build "$m2Root/build-android" --target xwa -j 8 2>&1 | Tee-Object "$m2Root/evidence/target-build.log"
if ($LASTEXITCODE) { throw 'Android build failed' }
$jni = "$m2Root/app/build/generated/engineJni/arm64-v8a"
$assets = "$m2Root/app/build/generated/engineAssets"
New-Item -ItemType Directory $jni,"$assets/shaders","$assets/resources" -Force | Out-Null
Copy-Item "$m2Root/build-android/openxwa/libOpenXWA.so","$m2Root/build-android/SDL3/libSDL3.so" $jni
Copy-Item "$m2Root/prefix/android-arm64/lib/*.so" $jni
Copy-Item "$ndk/toolchains/llvm/prebuilt/windows-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so" $jni
Copy-Item "$m2Root/build-android/shaders/*.spv" "$assets/shaders/"
Copy-Item "$m2Root/../../resources/*" "$assets/resources/" -Recurse -Force
New-Item -ItemType Directory "$assets/resources/aeron" -Force | Out-Null
Copy-Item "$m2Root/../../aeron/config/scene3d_defaults.yaml" "$assets/resources/aeron/"
