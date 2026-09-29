param([string]$SdkRoot = "$env:LOCALAPPDATA/Android/Sdk")
$ErrorActionPreference = 'Stop'
$m4Root = $PSScriptRoot.Replace('\','/')
$m2Root = "$m4Root/../m2"
$ndk = "$SdkRoot/ndk/28.2.13676358"
$cmake = "$SdkRoot/cmake/3.22.1/bin/cmake.exe"
New-Item -ItemType Directory "$m4Root/evidence" -Force | Out-Null
& $cmake -S $m4Root -B "$m4Root/build-android" -G Ninja `
    "-DCMAKE_MAKE_PROGRAM=$SdkRoot/cmake/3.22.1/bin/ninja.exe" `
    "-DCMAKE_TOOLCHAIN_FILE=$ndk/build/cmake/android.toolchain.cmake" `
    '-DANDROID_ABI=arm64-v8a' '-DANDROID_PLATFORM=android-29' `
    '-DANDROID_STL=c++_shared' '-DCMAKE_BUILD_TYPE=RelWithDebInfo' `
    '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON' 2>&1 | Tee-Object "$m4Root/evidence/configure.log"
if ($LASTEXITCODE) { throw 'M4 configure failed' }
& $cmake --build "$m4Root/build-android" -j 8 2>&1 | Tee-Object "$m4Root/evidence/native-build.log"
if ($LASTEXITCODE) { throw 'M4 native build failed' }
$jni = "$m4Root/app/build/generated/engineJni/arm64-v8a"
New-Item -ItemType Directory $jni -Force | Out-Null
Copy-Item "$m4Root/build-android/libOpenXWAM4.so" $jni
Get-ChildItem "$m2Root/app/build/generated/engineJni/arm64-v8a/*.so" |
    Where-Object Name -ne 'libOpenXWA.so' | Copy-Item -Destination $jni
Copy-Item "$m2Root/app/build/generated/openxrJni/arm64-v8a/libopenxr_loader.so" $jni

