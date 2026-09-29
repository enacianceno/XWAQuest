$ErrorActionPreference='Stop'
$m8=Split-Path $PSScriptRoot
$root=Split-Path (Split-Path $m8)
$cc="$m8/../m2/host/llvm-mingw-20260908-ucrt-x86_64/bin/clang.exe"
$env:TEMP="$m8/tmp"; $env:TMP=$env:TEMP
function Extract-Function([string]$text,[string]$name) {
    $match=[regex]::Match($text,'(?m)^(?:static )?[^\r\n;{}=]+\b'+$name+'\([^;]+?\)\s*\{')
    if(!$match.Success){throw "Cannot locate production function $name"}
    $start=$match.Index; $pos=$start+$match.Length; $depth=1
    while($depth -gt 0 -and $pos -lt $text.Length){
        if($text[$pos] -eq '{'){$depth++}; if($text[$pos] -eq '}'){$depth--}; $pos++
    }
    if($depth){throw 'Unbalanced production function'}
    return $text.Substring($start,$pos-$start)+"`n"
}
# Use exact production conversion bodies; no mock conversion/gameplay logic.
$mapping=[IO.File]::ReadAllText("$root/src/xwa_runtime/input/controller_mapping.c")
$aeron=[IO.File]::ReadAllText("$root/aeron/src/gamepad.c")
$gen="#include <string.h>`n#include `"xwa_runtime/input/controller_mapping.h`"`nenum {CONTROLLER_AXIS_RANGE=65535, CONTROLLER_AXIS_CENTER=32768};`n"
foreach($fn in @('Aeron_ControllerAxisDigitalDown','Aeron_ControllerDigitalSourceDown')){
    $gen+=Extract-Function $aeron $fn
}
foreach($fn in @('ControllerMapping_Profile','ControllerMapping_AxisValue','ControllerMapping_IsTrigger','ControllerMapping_CenteredAxis','ControllerMapping_TriggerAxis','ControllerMapping_Hat','ControllerMapping_PovDirection','ControllerMapping_HasPov','ControllerMapping_MapSnapshot','XwaControllerMapping_MapSnapshot')) {
    $gen+=Extract-Function $mapping $fn
}
[IO.File]::WriteAllText("$PSScriptRoot/native_mapping.generated.c",$gen)
& $cc -std=c99 -Wall -Wextra -Werror "$PSScriptRoot/touch_state_test.c" -o "$PSScriptRoot/touch_state_test.exe"
if($LASTEXITCODE){throw 'Touch compile failed'}
& "$PSScriptRoot/touch_state_test.exe" 2>&1 | Tee-Object "$m8/evidence/touch-state-test.log"
if($LASTEXITCODE){throw 'Touch test failed'}
& $cc -std=c99 -Wall -Wextra -Werror "-I$root/src" "-I$root/aeron/include" "$PSScriptRoot/native_mapping_test.c" "$PSScriptRoot/native_mapping.generated.c" -o "$PSScriptRoot/native_mapping_test.exe"
if($LASTEXITCODE){throw 'Native mapping compile failed'}
& "$PSScriptRoot/native_mapping_test.exe" 2>&1 | Tee-Object "$m8/evidence/native-mapping-test.log"
if($LASTEXITCODE){throw 'Native mapping test failed'}
Get-FileHash "$root/src/xwa_runtime/input/controller_mapping.c","$root/aeron/src/gamepad.c","$PSScriptRoot/native_mapping.generated.c" | Format-List | Out-File "$m8/evidence/native-mapping-source-hashes.log"
