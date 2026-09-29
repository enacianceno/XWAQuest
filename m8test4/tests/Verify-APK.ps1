$ErrorActionPreference='Stop'
$m8=Split-Path $PSScriptRoot
$sdk="$env:LOCALAPPDATA/Android/Sdk"
$llvm="$sdk/ndk/28.2.13676358/toolchains/llvm/prebuilt/windows-x86_64/bin"
$apk="$m8/app/build/outputs/apk/debug/app-debug.apk"
$out="$m8/evidence/apk-cs1-audit"
New-Item -ItemType Directory -Force $out | Out-Null
$badging=& "$sdk/build-tools/33.0.1/aapt.exe" dump badging $apk
if($LASTEXITCODE){throw 'aapt failed'}
$badging | Out-File "$out/badging.txt"
if(!($badging -match "package: name='org.openxwa.xwaquest.m8test4'.*versionName='0.8.2-test4'")){throw 'Wrong package/version'}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip=[IO.Compression.ZipFile]::OpenRead($apk)
try {
    $native=@($zip.Entries | Where-Object {$_.FullName -match '^lib/.+\.so$'})
    $native.FullName | Out-File "$out/native-entries.txt"
    if(!$native.Count -or @($native | Where-Object {$_.FullName -notmatch '^lib/arm64-v8a/'}).Count){throw 'Wrong ABI entries'}
    $entry=$zip.GetEntry('lib/arm64-v8a/libOpenXWAM8.so')
    if(!$entry){throw 'Missing CS1 library'}
    [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,"$out/libOpenXWAM8.so",$true)
} finally {$zip.Dispose()}
& "$llvm/llvm-strip.exe" --strip-unneeded -o "$out/libOpenXWAM8.expected.so" "$m8/build-android-t4/libOpenXWAM8.so"
if($LASTEXITCODE){throw 'strip failed'}
$actual=(Get-FileHash "$out/libOpenXWAM8.so").Hash
$expected=(Get-FileHash "$out/libOpenXWAM8.expected.so").Hash
if($actual -ne $expected){throw 'APK library differs from current native build after strip'}
$header=& "$llvm/llvm-readelf.exe" -h -d "$out/libOpenXWAM8.so"
if($LASTEXITCODE -or !($header -match 'AArch64')){throw 'Not ARM64 ELF'}
$header | Out-File "$out/elf.txt"
$symbols=& "$llvm/llvm-nm.exe" -D --defined-only "$out/libOpenXWAM8.so"
if($LASTEXITCODE){throw 'nm failed'}
$selected=$symbols | Select-String 'VrFlightBridge|VrFlightRenderer|__wrap_Mission_Init|VrConvert_SelfTest|M8_Memory|M8_Diag|__wrap_CombatSim|__wrap_MissionSetup|__wrap_Movie_Play|__wrap_FrontImage|__wrap_Mem_Alloc'
$selected | Out-File "$out/symbols.txt"
foreach($name in @('VrFlightBridge_CaptureAfterTick','VrFlightRenderer_BeginFrame','VrFlightRenderer_RenderEye','__wrap_Mission_Init','VrConvert_SelfTest')){
    if(!($symbols -match "\b$name$")){throw "Missing defined symbol $name"}
}
$strings=& "$llvm/llvm-strings.exe" "$out/libOpenXWAM8.so"
if($LASTEXITCODE){throw 'strings failed'}
$markers=@('M8_VR_FLIGHT_ENTER','M8_VR_PLAYER_READY','M8_VR_TARGET_READY','M8_VR_SNAPSHOT_READY','M8_VR_IMMERSIVE_ENTER','M8_VR_TARGET_DRAW_RECORDED','M8_VR_FIRST_STEREO_FRAME','M8_XR_FRAME_PRESENTED')
foreach($marker in $markers){if(!($strings -match $marker)){throw "Missing marker $marker"}}
$diagnostics=@('M8_MEM_APP_START','M8_MEM_PILOT','M8_MEM_COMBAT_SIM','M8_MEM_SINGLE_PLAYER_ENTER','M8_MEM_SINGLE_PLAYER_READY','M8_MEM_SKIRMISH_ENTER','M8_UI_FRAME_STATE','M8_UI_LOCATE_VIEWS','M8_UI_XR_END_RESULT','M8_LARGE_ALLOC')
foreach($marker in $diagnostics){if(!($strings -match $marker)){throw "Missing diagnostic $marker"}}
$t3=@('M8T3_PLAYER','M8T3_COCKPIT_DRAW_RECORDED','M8T3_COCKPIT_UNAVAILABLE','M8T3_STARS','M8T3_MISSION_GENERATION','M8T2_CLEANUP_ACQUIRE','M8T4_FRAME_TIMING','M8T4_MEMORY')
foreach($marker in $t3){if(!($strings -match $marker)){throw "Missing T3/baseline marker $marker"}}
if(!($badging -match "versionCode='5'")){throw 'Wrong T3 versionCode'}
$strings | Select-String 'M8T4_|M8T2_CLEANUP_ACQUIRE' | Out-File "$out/t3-markers.txt"
$strings | Select-String 'M8_MEM_|M8_UI_|M8_RESOURCE|M8_LARGE_ALLOC' | Out-File "$out/diagnostic-markers.txt"
$strings | Select-String 'M8_VR_|M8_XR_FRAME_PRESENTED|state_invalid|flags_not_ready|mesh_missing|not_runtime_opt|invalid_mesh/index_count|tick_mismatch|ensure_eye_failure|Compose_failure|Scene_Begin_failure|Scene_Render_failure' | Out-File "$out/markers.txt"
$result=@("APK_STATIC_AUDIT PASS",($badging | Select-String '^package:|^native-code:'),"PACKAGED_SO_SHA256=$actual","CURRENT_NATIVE_STRIPPED_SHA256=$expected","APK_SHA256=$((Get-FileHash $apk).Hash)","APK_BYTES=$((Get-Item $apk).Length)","RUNTIME=NOT_TESTED")
$result | Tee-Object "$out/result.txt"
