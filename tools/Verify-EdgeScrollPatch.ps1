[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$dist=Join-Path $repo 'dist/AKU9992-Native12-Fix6'
$stage=Join-Path $repo '.build/native12/fix6-update'
$manifest=[IO.File]::ReadAllLines((Join-Path $stage 'release.manifest'))
$release=Get-Content (Join-Path $dist 'update.json') -Raw|ConvertFrom-Json
if($release.version -ne 'Native12-Fix6' -or -not $release.archiveVerified -or -not $release.installerVerified){throw 'Fix6 package verification missing.'}
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
$edges=@()
foreach($edition in @('Generals','ZeroHour')){
 $v=Get-Content (Join-Path $repo ('.build/performance/edge-fix6/'+$edition+'/verification.json')) -Raw|ConvertFrom-Json
 if(-not $v.interactiveGame -or -not $v.fourEdgesAndFourCorners -or -not $v.centerAndUncapturedDoNotMove -or $v.samples -ne 30){throw 'Edge scrolling regression test missing.'}
 $edges+=$v
}
$result=[ordered]@{publisher='AKU9992';version='Native12-Fix6';installed=$installed;nativeTests=$native.tests;edgeScrolling=$edges;privacyAudit=$release.privacyAudit}
$result|ConvertTo-Json -Depth 9|Set-Content (Join-Path $dist 'verification.json') -Encoding utf8
Copy-Item (Join-Path $repo 'docs/native12-fix6.md') (Join-Path $dist 'AKU9992-Validation.md') -Force
$hashes=foreach($file in @('AKU9992-Native12-Fix6-Patch.exe','AKU9992-Native12-Fix6-Update.zip')){(Get-FileHash (Join-Path $dist $file)).Hash+'  '+$file}
[IO.File]::WriteAllLines((Join-Path $dist 'SHA256SUMS.txt'),$hashes,[Text.UTF8Encoding]::new($false))
Write-Output 'PASS Fix6 package, installed hashes, native tests and persistent edge scrolling in both games.'
