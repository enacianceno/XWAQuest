# Research-only reader. Does not write to GameData or product directories.
# Layout source: C:/OpenXWA/aeron/tools/opt2gltf/opt.c.
param([string]$Asset='C:\Program Files (x86)\Steam\steamapps\common\Star Wars X-Wing Alliance\FLIGHTMODELS\XWINGCOCKPIT.OPT')
$ErrorActionPreference='Stop'
$buf=[IO.File]::ReadAllBytes($Asset)
function I([int]$o) { [BitConverter]::ToInt32($buf,$o) }
function F([int]$o) { [BitConverter]::ToSingle($buf,$o) }
function V([int]$o) { @( (F $o), (F ($o+4)), (F ($o+8)) ) }
$first=I 0; $hdr=8; if($first -gt 0){$hdr=4}
if((I ($hdr-4)) -ne $buf.Length-$hdr){throw 'OPT header size mismatch'}
$bias=(I $hdr)-$hdr; $count=I ($hdr+6); $table=(I ($hdr+10))-$bias
$items=[Collections.Generic.List[object]]::new()
function Walk([int]$o,[int]$root,[int]$depth) {
    if($depth -gt 40 -or $o -lt 0 -or $o+24 -gt $buf.Length){throw 'Invalid node'}
    $type=I ($o+4); $n=I ($o+8); $children=I ($o+12); $p1=I ($o+16); $p2=I ($o+20)
    $name=''; $np=I $o
    if($np){$at=$np-$bias;while($at -ge 0 -and $at -lt $buf.Length -and $buf[$at]){$name+=[char]$buf[$at];$at++}}
    $row=[ordered]@{root=$root;node_offset=$o;type=$type;name=$name;count=$p1}
    $d=$p2-$bias
    if($type -eq 25){$row.mesh_type=I $d;$row.span=V ($d+8);$row.center=V ($d+20);$row.bbox_min=V ($d+32);$row.bbox_max=V ($d+44)}
    if($type -eq 23){$row.pivot=V $d;$row.axis1=V ($d+12);$row.axis2=V ($d+24);$row.axis3=V ($d+36)}
    if($type -eq 22){$row.hardpoint_type=I $d;$row.position=V ($d+4)}
    if($type -in @(3,22,23,25)){$items.Add([pscustomobject]$row)}
    if($type -in @(0,21,24)){for($k=0;$k -lt $n;$k++){$p=I ($children-$bias+4*$k);if($p){Walk ($p-$bias) $root ($depth+1)}}}
}
for($root=0;$root -lt $count;$root++){Walk ((I ($table+4*$root))-$bias) $root 0}
[ordered]@{asset=$Asset;bytes=$buf.Length;sha256=(Get-FileHash -LiteralPath $Asset -Algorithm SHA256).Hash;version=-$first;root_count=$count;units_metres=1600.0/65536.0;limitation='Structural inventory, not a textured visual identification; local Steam asset not verified against Quest private asset';nodes=$items} | ConvertTo-Json -Depth 8
