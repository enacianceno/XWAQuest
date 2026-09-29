param(
    [switch]$Execute,
    [string]$Serial,
    [string]$SdkRoot = "$env:LOCALAPPDATA/Android/Sdk"
)
$ErrorActionPreference = 'Stop'
if (!$Execute) {
    Write-Output 'PLAN ONLY: no ADB. Copy M7 private GameData to M8 private staging, verify all hashes, promote without overwrite. No APK install or launch.'
    Write-Output 'Future use AFTER explicit installation approval: .\m8\Provision-GameData.ps1 -Execute -Serial <authorized-device-serial>'
    return
}
if (!$Serial -or $Serial -notmatch '^[A-Za-z0-9._:-]+$') { throw 'Specify the explicitly selected device serial' }
# This question precedes EVERY adb invocation in this script, including connection checks.
# ASCII-only: Windows PowerShell 5.1 reads no-BOM scripts as ANSI, where byte 0x82
# becomes U+201A (a quote char) and breaks parsing. 'si' + [char]0xED == 'si' with accent.
$answer = Read-Host 'Ya encendiste el Quest 3S, lo conectaste a la PC y autorizaste la depuracion USB? Confirmame cuando este listo para continuar.'
if ($answer.Trim().ToLowerInvariant() -notin @('si', ('s' + [char]0xED))) { throw 'Physical readiness not confirmed; no ADB executed' }
$adb = "$SdkRoot/platform-tools/adb.exe"
$worker = Join-Path $PSScriptRoot 'provision-gamedata.sh'
$manifestHelper = Join-Path $PSScriptRoot 'evidence/gamedata/selected-manifest-worker.sh'
New-Item -ItemType Directory "$PSScriptRoot/evidence/gamedata" -Force | Out-Null
$combined = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'asset-selection.sh')) + "`n" + [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'asset-manifest.sh'))
[IO.File]::WriteAllText($manifestHelper, $combined.Replace("`r`n","`n"), [Text.UTF8Encoding]::new($false))
# A unique shell-owned worker; asset payload is never staged here.
$remote = '/data/local/tmp/xwaquest-m8-gamedata-' + [guid]::NewGuid().ToString('N') + '.sh'
& $adb -s $Serial get-state
if ($LASTEXITCODE) { throw 'Authorized Quest is not connected; stopping' }
& $adb -s $Serial push $worker $remote
if ($LASTEXITCODE) { throw 'Cannot transfer provisioning worker' }
New-Item -ItemType Directory "$PSScriptRoot/evidence/gamedata" -Force | Out-Null
& $adb -s $Serial push $manifestHelper ($remote + '.manifest.sh')
if ($LASTEXITCODE) { throw 'Cannot transfer manifest helper' }
& $adb -s $Serial shell -T sh $remote --approved-m8-copy ($remote + '.manifest.sh') 2>&1 | Tee-Object "$PSScriptRoot/evidence/gamedata/device-provision-v3.log"
if ($LASTEXITCODE) { throw 'Provisioning failed; inspect log/staging. No automatic deletion or retry.' }
Write-Output "Worker retained for audit: $remote. No install or application launch was performed."
