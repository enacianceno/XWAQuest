param([string]$SdkRoot = "$env:LOCALAPPDATA/Android/Sdk")
$ErrorActionPreference = 'Stop'
$m2Root = $PSScriptRoot.Replace('\','/')
$hostBin = "$m2Root/host/llvm-mingw-20260908-ucrt-x86_64/bin"
$env:PATH = "$hostBin;$env:PATH"
$cmake = "$SdkRoot/cmake/3.22.1/bin/cmake.exe"
& $cmake -S "$m2Root/host-tools" -B "$m2Root/build-host" -G Ninja `
    "-DCMAKE_MAKE_PROGRAM=$SdkRoot/cmake/3.22.1/bin/ninja.exe" `
    "-DCMAKE_C_COMPILER=$hostBin/clang.exe" `
    "-DCMAKE_CXX_COMPILER=$hostBin/clang++.exe" `
    '-DCMAKE_BUILD_TYPE=Release' 2>&1 | Tee-Object "$m2Root/evidence/host-configure.log"
if ($LASTEXITCODE) { throw 'Host configure failed' }
& $cmake --build "$m2Root/build-host" --target m2_host_runtime -j 8 2>&1 | Tee-Object "$m2Root/evidence/host-build.log"
if ($LASTEXITCODE) { throw 'Host build failed' }
Copy-Item "$hostBin/libc++.dll","$hostBin/libunwind.dll","$hostBin/libwinpthread-1.dll" "$m2Root/host/bin/"
