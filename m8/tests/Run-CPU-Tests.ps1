$ErrorActionPreference='Stop'
$m8=Split-Path $PSScriptRoot
$root=Split-Path (Split-Path $m8)
$cc="$m8/../m2/host/llvm-mingw-20260908-ucrt-x86_64/bin/clang.exe"
$env:TEMP="$m8/tmp"; $env:TMP=$env:TEMP
& $cc -std=c99 -Wall -Wextra -Werror "$PSScriptRoot/eye_math_test.c" -o "$PSScriptRoot/eye_math_test.exe"
if($LASTEXITCODE){throw 'eye compile failed'}
& "$PSScriptRoot/eye_math_test.exe" 2>&1 | Tee-Object "$m8/evidence/eye-math-test.log"
if($LASTEXITCODE){throw 'eye test failed'}
# Extract exact production CPU function bodies, without editing Aeron. COFF's
# linker requires GPU symbols even from unused functions of scene3d.c.
# No implementations/stubs of GPU functions are supplied to this CPU test.
$source="$root/aeron/src/scene/scene3d.c"
$text=[IO.File]::ReadAllText($source)
$generated="#include <math.h>`n#include <string.h>`n#include `"aeron/scene/scene3d.h`"`n"
foreach($name in @('scene_quat_to_mat3','scene_mat4_perspective_reverse_z_xy','scene_mat4_view','scene_mat4_mul','AeronScene_ComputeViewProj')){
    $match=[regex]::Match($text,'(?m)^(?:static )?void '+$name+'\([^;]+?\)\s*\{')
    if(!$match.Success){throw "Cannot locate real function $name"}
    $start=$match.Index; $pos=$start+$match.Length; $depth=1
    while($depth -gt 0 -and $pos -lt $text.Length){
        if($text[$pos] -eq '{'){$depth++}; if($text[$pos] -eq '}'){$depth--}; $pos++
    }
    if($depth){throw 'Unbalanced production function'}
    $generated+=$text.Substring($start,$pos-$start)+"`n"
}
$generatedPath="$PSScriptRoot/scene_camera.generated.c"
[IO.File]::WriteAllText($generatedPath,$generated)
Get-FileHash $source,$generatedPath | Format-List | Out-File "$m8/evidence/convert-source-hashes.log"
& $cc -std=c99 -Wall -Wextra -Werror "-I$m8/../vr-probe" "-I$root/aeron/include" "-I$root/third_party/SDL3/include" "$PSScriptRoot/convert_test.c" "$m8/../vr-probe/vr_convert.c" $generatedPath -o "$PSScriptRoot/convert_test.exe" 2>&1 | Tee-Object "$m8/evidence/convert-test-build.log"
if($LASTEXITCODE){throw 'convert compile failed'}
& "$PSScriptRoot/convert_test.exe" 2>&1 | Tee-Object "$m8/evidence/convert-test.log"
if($LASTEXITCODE){throw 'convert test failed'}
