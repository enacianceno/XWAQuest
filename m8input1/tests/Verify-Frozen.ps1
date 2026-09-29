param([string]$Label='check')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$rows=Import-Csv "$root/evidence/frozen-source-before.csv"
$results=foreach($r in $rows) {
    $hash=(Get-FileHash -LiteralPath $r.Path -Algorithm SHA256).Hash
    [pscustomobject]@{Path=$r.Path;Unchanged=($hash -eq $r.SHA256);SHA256=$hash}
}
$results | Export-Csv "$root/evidence/frozen-$Label.csv" -NoTypeInformation
$bad=@($results | Where-Object {!$_.Unchanged})
"FROZEN_SOURCE_HASHES files=$($results.Count) changed=$($bad.Count)"
if($bad.Count) { $bad | Format-Table; throw 'Frozen source drift: investigate without restoring anything' }
