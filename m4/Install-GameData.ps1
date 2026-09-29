param(
    [Parameter(Mandatory=$true)][string]$Source,
    [string]$SdkRoot = "$env:LOCALAPPDATA/Android/Sdk"
)
$ErrorActionPreference='Stop'
$sourceRoot=(Resolve-Path -LiteralPath $Source).Path.TrimEnd('\')
$adb="$SdkRoot/platform-tools/adb.exe"
# Relative to the app UID's private data directory; no device-specific path.
$destination='files/GameData'
$required=@('RESDATA.TXT','FLIGHTMODELS/SPACECRAFT0.LST','MISSIONS/MISSION.LST','MOVIES/PROLOGUE.SNM','MOVIES/BATTLE1.SNM','WAVE/FRONTEND/B1M1/N010101.WAV')
foreach($file in $required) {
    if(!(Test-Path -LiteralPath "$sourceRoot/$file" -PathType Leaf)){throw "Missing original data: $file"}
}
$inputs=@(Get-ChildItem -LiteralPath $sourceRoot -File | Where-Object Extension -in '.txt','.tab','.lst','.dat','.abp')
foreach($dir in @('FLIGHTMODELS','FRONTRES','MISSIONS','MOVIES','RESDATA','WAVE','SFX','FONTS')) {
    if(Test-Path -LiteralPath "$sourceRoot/$dir" -PathType Container){$inputs+=Get-Item -LiteralPath "$sourceRoot/$dir"}
}
$allFiles=@($inputs | ForEach-Object {if($_.PSIsContainer){Get-ChildItem -LiteralPath $_.FullName -File -Recurse}else{$_}})
if($allFiles | Where-Object Extension -in '.exe','.dll','.bat','.plt','.bak'){throw 'Unexpected executable or user-save file in selected data tree; inspect before copying'}
$allFiles | Select-Object @{n='RelativePath';e={$_.FullName.Substring($sourceRoot.Length+1)}},Length |
    Export-Csv "$PSScriptRoot/evidence/data-inventory.csv" -NoTypeInformation
& $adb shell am force-stop org.openxwa.xwaquest.m4
& $adb shell run-as org.openxwa.xwaquest.m4 mkdir -p $destination
if($LASTEXITCODE){throw 'Cannot create app-scoped data directory'}
# Stage as shell, then stream locally into run-as. Avoid host stdin encoding and
# avoid app-UID traversal of emulated-storage FUSE.
$staging="$PSScriptRoot/staging"
New-Item -ItemType Directory -Force $staging | Out-Null
$archive="$staging/gamedata.tar"
& tar.exe -cf $archive -C $sourceRoot @($inputs.Name)
if($LASTEXITCODE){throw 'Cannot archive selected original data'}
$remoteArchive='/data/local/tmp/xwaquest-m4-import.tar'
& $adb push $archive $remoteArchive
if($LASTEXITCODE){throw 'Archive transfer failed'}
& $adb shell "cat $remoteArchive | run-as org.openxwa.xwaquest.m4 tar -xf - -C $destination"
if($LASTEXITCODE){throw 'App-private extraction failed'}
$count=(& $adb shell "run-as org.openxwa.xwaquest.m4 find $destination -type f | wc -l").Trim()
if([int]$count -ne $allFiles.Count){throw "Expected $($allFiles.Count) files, device has $count"}
# Verify source/device contents, including case variations actually on disk.
$hashes=foreach($relative in $required) {
    $file=$allFiles | Where-Object { $_.FullName.Substring($sourceRoot.Length+1).Replace('\','/') -ieq $relative }
    if(@($file).Count -ne 1){throw "Missing or ambiguous inventory item $relative"}
    $actual=$file.FullName.Substring($sourceRoot.Length+1).Replace('\','/')
    $sourceHash=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    $deviceLine=& $adb shell run-as org.openxwa.xwaquest.m4 sha256sum "$destination/$actual"
    if($LASTEXITCODE){throw "Cannot hash $actual on Quest"}
    $deviceHash=($deviceLine -split '\s+')[0]
    if($deviceHash -ine $sourceHash){throw "Transfer checksum mismatch: $actual"}
    [pscustomobject]@{Path=$actual;SHA256=$sourceHash;Matched=$true}
}
$hashes | Export-Csv "$PSScriptRoot/evidence/data-hashes.csv" -NoTypeInformation
"Transferred and counted $count files using app UID into $destination" |
    Set-Content "$PSScriptRoot/evidence/data-transfer.txt"
& $adb shell rm $remoteArchive
if($LASTEXITCODE){throw 'Cannot remove device staging archive'}
Remove-Item -LiteralPath $archive

