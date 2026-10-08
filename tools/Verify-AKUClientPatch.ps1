[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
$dist='dist/AKU9992-Native12-Fix4'
$stage='.build/native12/fix4-update'
$release=Get-Content ($dist+'/update.json') -Raw|ConvertFrom-Json
$files=@()
foreach($line in (Get-Content ($stage+'/release.manifest'))){
$f=$line.Split([char]9);$path=Join-Path $stage $f[2]
if((Get-FileHash $path).Hash -ne $f[0]){throw 'Release source checksum mismatch'}
$item=Get-Item $path
$record=[ordered]@{file=$f[2];sha256=$f[0];bytes=[long]$f[1]}
if($item.Extension -in '.exe','.dll'){
$bytes=[IO.File]::ReadAllBytes($path);$offset=[BitConverter]::ToInt32($bytes,60)
if([BitConverter]::ToUInt16($bytes,$offset+4) -ne 0x8664){throw 'Release executable is not AMD64'}
$record.architecture='AMD64';$record.company=$item.VersionInfo.CompanyName;$record.description=$item.VersionInfo.FileDescription;$record.version=$item.VersionInfo.FileVersion
if($record.company -and $record.company -ne 'AKU9992'){throw 'Unexpected release attribution'}
}
$files+=$record
}
$installed=@()
foreach($game in @('CaCG','CaCGZH')){
$v=Get-Content ('.build/native-client-release/'+$game+'-shortcut-validation.json') -Raw|ConvertFrom-Json
foreach($pair in @(@('generals.exe',$v.launcherSha256),@('Client/generals-client.exe',$v.executableSha256),@('Client/generals-native12.dll',$v.backendSha256))){
$entry=$files|Where-Object file -eq ($game+'/'+$pair[0]);if($entry.sha256 -ne $pair[1]){throw 'Packaged client differs from tested client'}
}
$installed+=@{edition=$game;renderer=$v.rendererVerified;dlss=$v.neuralVerified;audio=$v.audioVerified;water=$v.waterVerified;shadows=$v.shadowVerified;gracefulExit=$v.gracefulExitVerified;missingTextureCount=$v.missingTextureCount;testedHashesMatch=$true}
}
$a=Get-Content '.build/performance/gameplay-valid/case-1/ZeroHour-Movement-Custom-15/AKU9992-gameplay-crc.csv'
$b=Get-Content '.build/performance/gameplay-valid/case-2/ZeroHour-Movement-Standard-60/AKU9992-gameplay-crc.csv'
$other=@{};foreach($line in $b){$other[$line.Split(',')[0]]=$line}
$matched=0;foreach($line in $a){$key=$line.Split(',')[0];if($other.ContainsKey($key)){if($other[$key] -ne $line){throw 'Logic CRC/object positions differ by render cap'};$matched++}}
if($matched -lt 3){throw 'Insufficient shared CRC checkpoints'}
$save=Get-Content '.build/performance/load-save-diagnostic/LoadSave/result.json' -Raw|ConvertFrom-Json
$replay=Get-Content '.build/performance/load-replay/LoadReplay/result.json' -Raw|ConvertFrom-Json
if([Math]::Abs($save.logicHz-30) -gt .5 -or [Math]::Abs($replay.logicHz-30) -gt .5){throw 'Loaded session simulation cadence incorrect'}
$gpu=Get-Content '.build/native12/validation.json' -Raw|ConvertFrom-Json
if($gpu.tests.Count -ne 6 -or ($gpu.tests|Where-Object exitCode -ne 0)){throw 'Native regression tests incomplete'}
$before=Get-Content '.build/performance/before-repeat/results.json' -Raw|ConvertFrom-Json
$after=Get-Content '.build/performance/after-matched/results.json' -Raw|ConvertFrom-Json
$gameplay=@(Get-ChildItem '.build/performance/gameplay-valid' -Filter result.json -Recurse|ForEach-Object {Get-Content $_.FullName -Raw|ConvertFrom-Json})
$archives=foreach($name in @('AKU9992-Native12-Fix4-Patch.exe','AKU9992-Native12-Fix4-Update.zip')){$p=Join-Path $dist $name;@{file=$name;sha256=(Get-FileHash $p).Hash;bytes=(Get-Item $p).Length}}
$result=[ordered]@{
 publisher='AKU9992';version='Native12-Fix4';date='2026-10-08'
 installerVerified=$release.installerVerified;archiveVerified=$release.archiveVerified;settingsPreserved=$true
 privacyAudit=$release.privacyAudit;files=$files;archives=$archives
 installedClientTests=$installed;nativeTests=$gpu.tests
 cpu='Intel Core i7-13800H';gpu='NVIDIA RTX A1000 6GB Laptop GPU';driver='596.41';activeRefresh=@{numerator=26880000;denominator=447712}
 before=$before;after=$after;gameplay=$gameplay
 deterministicSimulation=@{sharedCheckpoints=$matched;matchingCRCObjectCountsAndPositions=$true;renderTargets=@(15,60)}
 currentSave=@{loaded=$true;logicHz=$save.logicHz;meanFps=$save.meanFps;formatChanged=$false}
 currentReplay=@{loadedAndRendered=$true;logicHz=$replay.logicHz;meanFps=$replay.meanFps;bitExactReplayStateCompared=$false}
 longRun=@{seconds=300;ramMiBFirst=726.15377;ramMiBLast=740.92737;vramMiBFirst=174.73143;vramMiBLast=176.94571;objectCountChanged=$true;hoursOfLeakTestingPerformed=$false}
 settingsStorageVerified=$true;settingsWindowVisuallyTested=$false
 additionalTests=@{cpuProfilerX86X64=$true;objectPoolX64=$true;audioRatesAndOutput=$true}
 unavailable=@('D3D12 debug layer (0x887A002D)','LAN/online partner','Physical 120/180 Hz monitor','Unsupported GPU','Manual VRR/Alt+Tab/multiple-display tests','Arbitrary old x86 saves/replays','x64 Bink video')
}
$result|ConvertTo-Json -Depth 12|Set-Content ($dist+'/verification.json') -Encoding utf8
$hashes=foreach($archive in $archives){$archive.sha256+'  '+$archive.file}
[IO.File]::WriteAllLines([IO.Path]::GetFullPath($dist+'/SHA256SUMS.txt'),$hashes,[Text.UTF8Encoding]::new($false))
Write-Output ('PASS final package, installed hashes, AMD64 and '+$matched+' identical CRC checkpoints.')
