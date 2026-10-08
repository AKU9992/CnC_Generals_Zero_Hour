param([Parameter(Mandatory)][string]$SourcePath)
$ErrorActionPreference='Stop'
foreach($edition in @('Generals','GeneralsMD')){
$p=Join-Path $SourcePath ($edition+'/Code/GameEngine/Source/Common/System/SaveGame/GameStateMap.cpp')
$t=[IO.File]::ReadAllText($p)
if($t.Contains('AKU9992: keep cleanup inside Save')){continue}
$a=$t.IndexOf('void GameStateMap::clearScratchPadMaps()')
if($a -lt 0){throw 'Scratch map cleanup anchor missing'}
$open=$t.IndexOf('{',$a);$depth=1;$end=$open+1
while($depth -gt 0 -and $end -lt $t.Length){if($t[$end] -eq '{'){$depth++};if($t[$end] -eq '}'){$depth--};$end++}
if($depth -ne 0){throw 'Scratch map cleanup body not found'}
$body=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'native/AKUScratchMapCleanup.inc'))
[IO.File]::WriteAllText($p,$t.Substring(0,$a)+$body+$t.Substring($end))
}

$p=Join-Path $SourcePath 'Core/GameEngine/Source/GameNetwork/GameInfo.cpp';$t=[IO.File]::ReadAllText($p);if(-not $t.Contains('AKU9992: slot IPs are not serialized')){$block=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'native/AKULocalSavePlayer.inc'));$anchor='xfer->xferUnsignedInt(&m_localIP);';$t=$t.Replace($anchor,$anchor+[Environment]::NewLine+$block);[IO.File]::WriteAllText($p,$t)}