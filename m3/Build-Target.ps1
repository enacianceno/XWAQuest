param([string]$SdkRoot = "$env:LOCALAPPDATA/Android/Sdk")
$ErrorActionPreference = 'Stop'
$m3Root = $PSScriptRoot.Replace('\','/')
$m2Root = "$m3Root/../m2"
$ndk = "$SdkRoot/ndk/28.2.13676358"
$cmake = "$SdkRoot/cmake/3.22.1/bin/cmake.exe"
New-Item -ItemType Directory "$m3Root/evidence" -Force | Out-Null
& $cmake -S $m3Root -B "$m3Root/build-android" -G Ninja `
    "-DCMAKE_MAKE_PROGRAM=$SdkRoot/cmake/3.22.1/bin/ninja.exe" `
    "-DCMAKE_TOOLCHAIN_FILE=$ndk/build/cmake/android.toolchain.cmake" `
    '-DANDROID_ABI=arm64-v8a' '-DANDROID_PLATFORM=android-29' `
    '-DANDROID_STL=c++_shared' '-DCMAKE_BUILD_TYPE=RelWithDebInfo' `
    '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON' 2>&1 | Tee-Object "$m3Root/evidence/configure.log"
if ($LASTEXITCODE) { throw 'M3 configure failed' }
& $cmake --build "$m3Root/build-android" -j 8 2>&1 | Tee-Object "$m3Root/evidence/native-build.log"
if ($LASTEXITCODE) { throw 'M3 native build failed' }
$jni = "$m3Root/app/build/generated/engineJni/arm64-v8a"
New-Item -ItemType Directory $jni -Force | Out-Null
Copy-Item "$m3Root/build-android/libOpenXWAM3.so" $jni
Get-ChildItem "$m2Root/app/build/generated/engineJni/arm64-v8a/*.so" |
    Where-Object Name -ne 'libOpenXWA.so' | Copy-Item -Destination $jni
Copy-Item "$m2Root/app/build/generated/openxrJni/arm64-v8a/libopenxr_loader.so" $jni
