param([string]$SdkRoot = "$env:LOCALAPPDATA/Android/Sdk")
$ErrorActionPreference = 'Stop'
$m2Root = $PSScriptRoot
$ndk = "$SdkRoot/ndk/28.2.13676358/toolchains/llvm/prebuilt/windows-x86_64"
$readelf = "$ndk/bin/llvm-readelf.exe"
$nm = "$ndk/bin/llvm-nm.exe"
$jni = "$m2Root/app/build/generated/engineJni/arm64-v8a"
$libs = @(Get-ChildItem $jni -Filter '*.so') + @(Get-ChildItem "$m2Root/app/build/generated/openxrJni/arm64-v8a" -Filter '*.so')
if ($libs.Count -ne 9) { throw "Expected 9 packaged DSOs, found $($libs.Count)" }
$systemDir = "$ndk/sysroot/usr/lib/aarch64-linux-android/29"
$systemNames = @('libc.so','libm.so','libdl.so','liblog.so','libandroid.so','libz.so','libOpenSLES.so','libGLESv1_CM.so','libGLESv2.so','libEGL.so','libvulkan.so','libjnigraphics.so')
$allNames = @($libs.Name) + $systemNames
$abi = foreach ($lib in $libs) {
    $headers = & $readelf -h -d $lib.FullName
    if ($LASTEXITCODE) { throw "readelf failed: $($lib.Name)" }
    $headers | Set-Content "$m2Root/evidence/$($lib.Name)-elf.txt"
    $joined = $headers -join "`n"
    if ($joined -notmatch 'Class:\s+ELF64' -or $joined -notmatch 'Machine:\s+AArch64') {
        throw "Wrong ABI: $($lib.Name)"
    }
    $needed = @([regex]::Matches($joined,'\(NEEDED\).*?\[(.*?)\]') | ForEach-Object { $_.Groups[1].Value })
    foreach ($dep in $needed) {
        if ($dep -notin $allNames -or $dep -match '\.dll|\.exe|[\\/]') { throw "Unpackaged/host dependency: $dep" }
    }
    [pscustomobject]@{Name=$lib.Name;ABI='ELF64 AArch64';Bytes=$lib.Length;Needed=($needed -join ',');SHA256=(Get-FileHash $lib.FullName).Hash}
}
$abi | Export-Csv "$m2Root/evidence/native-libraries.csv" -NoTypeInformation

# Check strong undefined dynamic symbols against packaged DSOs and API 29 stubs.
# A U entry is normal for a DSO; the error is a symbol with no provider.
$exports = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
$providers = @($libs.FullName) + @($systemNames | ForEach-Object { "$systemDir/$_" } | Where-Object { Test-Path $_ })
foreach ($provider in $providers) {
    $symbols = & $nm -D --defined-only --format=posix $provider 2>$null
    foreach ($line in $symbols) {
        if ($line -match '^(\S+)\s+\S\s') { [void]$exports.Add(($Matches[1] -split '@')[0]) }
    }
}
$unresolved = foreach ($lib in $libs) {
    $symbols = & $nm -D --undefined-only --format=posix $lib.FullName
    $symbols | Set-Content "$m2Root/evidence/$($lib.Name)-imports.txt"
    foreach ($line in $symbols) {
        if ($line -match '^(\S+)\s+U\s') {
            $symbol = ($Matches[1] -split '@')[0]
            if (!$exports.Contains($symbol)) { "$($lib.Name): $symbol" }
        }
    }
}
if ($unresolved) { $unresolved | Set-Content "$m2Root/evidence/unresolved.txt"; throw 'Unresolved strong dynamic symbols' }
'No unresolved strong dynamic symbols against packaged libraries and Android API 29.' | Set-Content "$m2Root/evidence/symbol-validation.txt"

$shaders = @(Get-ChildItem "$m2Root/app/build/generated/engineAssets/shaders" -File)
if (!$shaders.Count) { throw 'Missing shaders' }
New-Item -ItemType Directory "$m2Root/evidence/shader-reflection" -Force | Out-Null
foreach ($shader in $shaders) {
    $bytes = [IO.File]::ReadAllBytes($shader.FullName)
    if ($shader.Extension -ne '.spv' -or [BitConverter]::ToUInt32($bytes,0) -ne 0x07230203 -or ($bytes.Length % 4)) {
        throw "Invalid SPIR-V artifact: $($shader.Name)"
    }
    $entryName = $null
    $stage = $null
    for ($offset=20; $offset -lt $bytes.Length;) {
        $instruction = [BitConverter]::ToUInt32($bytes,$offset)
        $wordCount = $instruction -shr 16
        if (!$wordCount -or ($offset + 4*$wordCount) -gt $bytes.Length) { throw "Malformed SPIR-V: $($shader.Name)" }
        if (($instruction -band 65535) -eq 15) {
            $model = [BitConverter]::ToUInt32($bytes,$offset+4)
            $stage = switch ($model) { 0 { 'vertex' }; 4 { 'fragment' }; 5 { 'compute' }; default { throw 'Unsupported shader stage' } }
            $entryName = ([Text.Encoding]::UTF8.GetString($bytes,$offset+12,4*$wordCount-12) -split "`0")[0]
        }
        $offset += 4*$wordCount
    }
    if (!$entryName) { throw "SPIR-V has no entrypoint: $($shader.Name)" }
    & "$m2Root/host/bin/shadercross.exe" $shader.FullName --source SPIRV --dest JSON --stage $stage --entrypoint $entryName --output "$m2Root/evidence/shader-reflection/$($shader.Name).json"
    if ($LASTEXITCODE) { throw "SPIR-V reflection failed: $($shader.Name)" }
}
$shaders | Get-FileHash | Select-Object Path,Hash | Export-Csv "$m2Root/evidence/shaders.csv" -NoTypeInformation
$commands = Get-Content "$m2Root/build-android/compile_commands.json" -Raw | ConvertFrom-Json
foreach ($command in $commands) {
    if ($command.command -notmatch '--target=aarch64[^ ]*android29' -or $command.command -match 'XWA_OPT_PROBE|xwa_probe_stubs|host[/\\]llvm-mingw') {
        throw "Contaminated target compile command: $($command.file)"
    }
}
$archives = Get-ChildItem "$m2Root/build-android" -Recurse -Filter '*.a'
foreach ($archive in $archives) {
    $headers = (& $readelf -h $archive.FullName) -join "`n"
    if ($LASTEXITCODE) { throw "Unreadable archive: $($archive.FullName)" }
    foreach ($machine in [regex]::Matches($headers,'Machine:([^\r\n]+)')) {
        if ($machine.Groups[1].Value.Trim() -ne 'AArch64') { throw "Non-ARM64 archive: $($archive.FullName)" }
    }
}
Import-Csv "$m2Root/evidence/preserved-before.csv" | ForEach-Object {
    if ((Get-FileHash $_.Path).Hash -ne $_.Hash) { throw "Preserved file changed: $($_.Path)" }
}
"PASS: $($libs.Count) ARM64 DSOs; $($archives.Count) ARM64 archives; $($shaders.Count) SPIR-V shaders; $($commands.Count) target translation units; preserved M1/OPT hashes unchanged." | Tee-Object "$m2Root/evidence/validation-summary.txt"
