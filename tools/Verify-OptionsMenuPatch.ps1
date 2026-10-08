[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$dist=Join-Path $repo 'dist/AKU9992-Native12-Fix5'
$stage=Join-Path $repo '.build/native12/fix5-update'
$manifest=[IO.File]::ReadAllLines((Join-Path $stage 'release.manifest'))
$release=Get-Content (Join-Path $dist 'update.json') -Raw|ConvertFrom-Json
if($release.version -ne 'Native12-Fix5' -or -not $release.archiveVerified -or -not $release.installerVerified){throw 'Fix5 package verification missing.'}
$native=Get-Content (Join-Path $repo '.build/native12/validation.json') -Raw|ConvertFrom-Json
if($native.tests.Count -ne 6 -or @($native.tests|Where-Object exitCode -ne 0).Count){throw 'Native tests failed.'}
$installed=@()
foreach($game in @('CaCG','CaCGZH')){
 $v=Get-Content (Join-Path $repo ('.build/native-client-release/'+$game+'-shortcut-validation.json')) -Raw|ConvertFrom-Json
 foreach($pair in @(@('generals.exe',$v.launcherSha256),@('Client/generals-client.exe',$v.executableSha256),@('Client/generals-native12.dll',$v.backendSha256))){
  $entry=@($manifest|Where-Object {($_.Split([char]9))[2] -eq ($game+'/'+$pair[0])})
  if($entry.Count -ne 1 -or $entry[0].Split([char]9)[0] -ne $pair[1]){throw 'Installed client differs from packaged client.'}
 }
 foreach($property in @('rendererVerified','neuralVerified','audioVerified','gracefulExitVerified','waterVerified','shadowVerified')){if(-not $v.$property){throw 'Installed game validation failed.'}}
 $installed+=@{edition=$game;testedHashesMatch=$true;renderer=$true;dlss=$true;audio=$true;water=$true;shadows=$true;gracefulExit=$true}
}
$options=@()
foreach($edition in @('Generals','ZeroHour')){
 $v=Get-Content (Join-Path $repo ('.build/performance/options/'+$edition+'-options.json')) -Raw|ConvertFrom-Json
 if(-not $v.initialized -or $v.samples -lt 100){throw 'Initialized options menu test missing.'}
 $options+=$v
}
$comparisons=@{}
foreach($phase in @('before','fixed')){
 $rows=Import-Csv (Join-Path $repo ('.build/performance/options-'+$phase+'-frames.csv'))
 $elapsed=0.0;$samples=@()
 foreach($row in $rows){$elapsed+=[double]$row.frame_ms;if($elapsed -gt 8000){$samples+=$row}}
 $gpuRows=@(Import-Csv (Join-Path $repo ('.build/performance/options-'+$phase+'-render.csv'))|Where-Object {[double]$_.elapsed_ms -gt 8000})
 $comparisons[$phase]=@{meanFps=1000/($samples.frame_ms|Measure-Object -Average).Average;cpuRecordMs=($gpuRows.cpu_record_ms|Measure-Object -Average).Average;gpuMs=($gpuRows.gpu_ms|Measure-Object -Average).Average}
}
$result=[ordered]@{publisher='AKU9992';version='Native12-Fix5';installed=$installed;nativeTests=$native.tests;optionsMenu=$options;matchedLayoutComparison=@{edition='ZeroHour';resolution='1920x1080';before=$comparisons.before;after=$comparisons.fixed;scope='Uninitialized layout on identical shell scene, uncapped; first 8 seconds excluded'};interactiveControlsManuallyTested=$false;privacyAudit=$release.privacyAudit}
$result|ConvertTo-Json -Depth 9|Set-Content (Join-Path $dist 'verification.json') -Encoding utf8
Copy-Item (Join-Path $repo 'docs/native12-fix5.md') (Join-Path $dist 'AKU9992-Validation.md') -Force
$hashes=foreach($file in @('AKU9992-Native12-Fix5-Patch.exe','AKU9992-Native12-Fix5-Update.zip')){(Get-FileHash (Join-Path $dist $file)).Hash+'  '+$file}
[IO.File]::WriteAllLines((Join-Path $dist 'SHA256SUMS.txt'),$hashes,[Text.UTF8Encoding]::new($false))
Write-Output 'PASS Fix5 package, installed hashes, native GPU tests and both initialized options menus.'
