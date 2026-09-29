param([string]$SdkRoot = "$env:LOCALAPPDATA/Android/Sdk")
$ErrorActionPreference = 'Stop'
$probeRoot = $PSScriptRoot.Replace('\','/')
$m2Root = "$probeRoot/../m2"
$ndk = "$SdkRoot/ndk/28.2.13676358"
$cmake = "$SdkRoot/cmake/3.22.1/bin/cmake.exe"
New-Item -ItemType Directory "$probeRoot/evidence" -Force | Out-Null
& $cmake -S $probeRoot -B "$probeRoot/build-android" -G Ninja `
    "-DCMAKE_MAKE_PROGRAM=$SdkRoot/cmake/3.22.1/bin/ninja.exe" `
    "-DCMAKE_TOOLCHAIN_FILE=$ndk/build/cmake/android.toolchain.cmake" `
    '-DANDROID_ABI=arm64-v8a' '-DANDROID_PLATFORM=android-29' `
    '-DANDROID_STL=c++_shared' '-DCMAKE_BUILD_TYPE=RelWithDebInfo' `
    '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON' 2>&1 | Tee-Object "$probeRoot/evidence/configure.log"
if ($LASTEXITCODE) { throw 'VRPROBE configure failed' }
& $cmake --build "$probeRoot/build-android" -j 8 2>&1 | Tee-Object "$probeRoot/evidence/native-build.log"
if ($LASTEXITCODE) { throw 'VRPROBE native build failed' }

# Deploy minimal shaders to m2 engineAssets so they get packaged by Gradle
$m2Shaders = "$m2Root/build-android/shaders"
$m2Assets = "$m2Root/app/build/generated/engineAssets/shaders"
New-Item -ItemType Directory $m2Shaders -Force | Out-Null
New-Item -ItemType Directory $m2Assets -Force | Out-Null
Copy-Item "$probeRoot/minimal.vert.spv" $m2Shaders -Force
Copy-Item "$probeRoot/minimal.frag.spv" $m2Shaders -Force
Copy-Item "$probeRoot/minimal.vert.spv" $m2Assets -Force
Copy-Item "$probeRoot/minimal.frag.spv" $m2Assets -Force

$jni = "$probeRoot/app/build/generated/engineJni/arm64-v8a"
New-Item -ItemType Directory $jni -Force | Out-Null
Copy-Item "$probeRoot/build-android/libOpenXWAVRP.so" $jni
Get-ChildItem "$m2Root/app/build/generated/engineJni/arm64-v8a/*.so" |
    Where-Object Name -ne 'libOpenXWA.so' | Copy-Item -Destination $jni
Copy-Item "$m2Root/app/build/generated/openxrJni/arm64-v8a/libopenxr_loader.so" $jni
