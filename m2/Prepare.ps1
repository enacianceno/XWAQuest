$ErrorActionPreference = 'Stop'
$m2Root = $PSScriptRoot
New-Item -ItemType Directory "$m2Root/downloads","$m2Root/deps","$m2Root/host","$m2Root/evidence" -Force | Out-Null
function Get-VerifiedArchive($Url, $Name, $Hash) {
    $path = Join-Path "$m2Root/downloads" $Name
    if (!(Test-Path $path)) { Invoke-WebRequest $Url -OutFile $path }
    if ((Get-FileHash $path -Algorithm SHA256).Hash -ne $Hash) { throw "SHA256 mismatch: $path" }
    return $path
}
function Get-PinnedSource($Url, $Name, $Revision) {
    $path = Join-Path "$m2Root/deps" $Name
    if (!(Test-Path "$path/.git")) {
        git init $path
        if ($LASTEXITCODE) { throw 'git init failed' }
        git -C $path remote add origin $Url
        git -C $path fetch --depth 1 origin $Revision
        if ($LASTEXITCODE) { throw "Fetch failed: $Name" }
        git -C $path checkout --detach FETCH_HEAD
    }
    if ((git -C $path rev-parse HEAD) -ne $Revision) { throw "Unexpected revision: $Name" }
}
$archive = Get-VerifiedArchive 'https://github.com/mstorsjo/llvm-mingw/releases/download/20260908/llvm-mingw-20260908-ucrt-x86_64.zip' 'llvm-mingw.zip' '1BCF74D06B724AEECAA6412CA85F5B26FB1DA770E7CDCEFA9263C9C5C3AD34B6'
if (!(Test-Path "$m2Root/host/llvm-mingw-20260908-ucrt-x86_64/bin/clang.exe")) {
    tar -xf $archive -C "$m2Root/host"
    if ($LASTEXITCODE) { throw 'LLVM extraction failed' }
}
$archive = Get-VerifiedArchive 'https://repo.msys2.org/msys/x86_64/make-4.4.1-3-x86_64.pkg.tar.zst' 'make.pkg.tar.zst' 'AF0BDBA17F06FE037F0194069ADAA31A8FE45F1A11381501896AEA1FAE37BD5D'
New-Item -ItemType Directory "$m2Root/host/msys-make" -Force | Out-Null
tar -xf $archive -C "$m2Root/host/msys-make"
if ($LASTEXITCODE) { throw 'make extraction failed' }
Get-PinnedSource 'https://github.com/libsdl-org/SDL_shadercross.git' 'SDL_shadercross' '1ff05bec573988a98ef9e0260b4da44f512b8367'
$shaderPatch = (Resolve-Path "$m2Root/../../packaging/common/shadercross-fsr3.patch").Path
git -C "$m2Root/deps/SDL_shadercross" apply --reverse --check $shaderPatch 2>$null
if ($LASTEXITCODE) {
    git -C "$m2Root/deps/SDL_shadercross" apply --check $shaderPatch
    if ($LASTEXITCODE) { throw 'Unexpected shadercross modifications' }
    git -C "$m2Root/deps/SDL_shadercross" apply $shaderPatch
    if ($LASTEXITCODE) { throw 'shadercross FSR3 patch failed' }
}
Get-PinnedSource 'https://github.com/facebook/zstd.git' 'zstd' 'f8745da6ff1ad1e7bab384bd1f9d742439278e99'
Get-PinnedSource 'https://github.com/FFmpeg/FFmpeg.git' 'ffmpeg' 'f46e514491172d15bd74b4abb1814cd2f05a763e'
git -C "$m2Root/deps/SDL_shadercross" submodule update --init --depth 1 external/SPIRV-Cross
if ($LASTEXITCODE) { throw 'SPIRV-Cross checkout failed' }
$archive = Get-VerifiedArchive 'https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2602/dxc_2026_02_20.zip' 'dxc.zip' 'A1E89031421CF3C1FCA6627766AB3020CA4F962AC7E2CAA7FAB2B33A8436151E'
$dxc = "$m2Root/deps/SDL_shadercross/external/DirectXShaderCompiler-binaries/windows"
New-Item -ItemType Directory $dxc -Force | Out-Null
tar -xf $archive -C $dxc
if ($LASTEXITCODE) { throw 'DXC extraction failed' }
