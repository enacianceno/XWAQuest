$ErrorActionPreference = 'Stop'
# Focused regression test: provision scripts must parse under Windows
# PowerShell 5.1 and must not touch ADB unless -Execute is passed.
# Root cause guarded: UTF-8 no-BOM bytes above 0x7F are read as ANSI by
# PowerShell 5.1, where byte 0x82 decodes to U+201A, a quote character
# that breaks string parsing ("missing terminator" downstream).
$m8 = Split-Path $PSScriptRoot
$scripts = @(
    (Join-Path $m8 'Provision-GameData-PS51.ps1'),
    (Join-Path $m8 'Provision-GameData.ps1')
)
foreach ($script in $scripts) {
    $bytes = [IO.File]::ReadAllBytes($script)
    $high = @($bytes | Where-Object { $_ -gt 127 }).Count
    if ($high -gt 0) { throw "Non-ASCII bytes in ${script}: $high (breaks PS 5.1 no-BOM parse)" }
    $tokens = $null
    $errors = $null
    [System.Management.Automation.Language.Parser]::ParseFile($script, [ref]$tokens, [ref]$errors) | Out-Null
    if ($errors.Count -gt 0) { throw "Parse errors in ${script}: $($errors[0].Message)" }
}
foreach ($script in Get-ChildItem (Join-Path $m8 '*.ps1'), (Join-Path $PSScriptRoot '*.ps1')) {
    $bytes = [IO.File]::ReadAllBytes($script.FullName)
    if (@($bytes | Where-Object { $_ -gt 127 }).Count -gt 0) { throw "Non-ASCII bytes in $($script.FullName)" }
}
$plan = & powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $scripts[0] 2>&1
if ($LASTEXITCODE) { throw "Plan-only run failed with exit $LASTEXITCODE" }
if (!($plan -match 'PLAN ONLY') -or !($plan -match 'no ADB')) { throw 'Plan-only run lost its no-ADB guard' }
Write-Output 'PROVISION_SCRIPT_TEST PASS'
