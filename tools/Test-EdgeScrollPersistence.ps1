[CmdletBinding()]
param([ValidateSet('Generals','ZeroHour')][string]$Edition='ZeroHour')
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$output=Join-Path $repo ('.build/performance/edge-fix6/'+$Edition)
$runRoot=Join-Path $output ("run-"+[Guid]::NewGuid().ToString("N"))
$old=[Environment]::GetEnvironmentVariable('GENERALS_TEST_EDGE_SCROLL')
try{
 $env:GENERALS_TEST_EDGE_SCROLL='1'
 & (Join-Path $PSScriptRoot 'Test-PerformanceScenarios.ps1') -Edition $Edition -Scenario Movement -FpsMode Standard -CustomFps 60 -Seconds 20 -OutputRoot $runRoot -LegacyMousePreferences | Out-Null
 $result=Get-ChildItem $runRoot -Filter 'AKU9992-edge-test.csv' -Recurse -File
 if($result.Count -ne 1){throw 'Interactive game edge test did not execute.'}
 $rows=@(Get-Content $result.FullName|ConvertFrom-Csv -Header stage,x,y,captured,scrollX,scrollY,moved)
 if($rows.Count -ne 30){throw 'Edge test sample count differs.'}
 foreach($stage in @('initial','legacy-settings','preferences')){
  $samples=@($rows|Where-Object stage -eq $stage)
  if($samples.Count -ne 10){throw 'Edge test stage missing.'}
  for($i=0;$i -lt 8;$i++){if(([double]$samples[$i].scrollX -eq 0 -and [double]$samples[$i].scrollY -eq 0) -or $samples[$i].moved -ne '1'){throw ('Camera failed to scroll: '+$stage+' case '+$i)}}
  for($i=8;$i -lt 10;$i++){if([double]$samples[$i].scrollX -ne 0 -or [double]$samples[$i].scrollY -ne 0 -or $samples[$i].moved -ne '0'){throw ('Camera moved in center/uncaptured case: '+$stage)}}
  foreach($corner in @(@(4,-1,-1),@(5,1,-1),@(6,-1,1),@(7,1,1))){
   $sample=$samples[$corner[0]]
   if([Math]::Sign([double]$sample.scrollX) -ne $corner[1] -or [Math]::Sign([double]$sample.scrollY) -ne $corner[2]){throw 'Corner direction incorrect.'}
  }
  if([double]$samples[0].scrollX -ge 0 -or [double]$samples[1].scrollX -le 0 -or [double]$samples[2].scrollY -ge 0 -or [double]$samples[3].scrollY -le 0){throw 'Edge direction incorrect.'}
 }
 $verification=[ordered]@{publisher='AKU9992';edition=$Edition;interactiveGame=$true;disabledLegacyPreferences=$true;stages=@('initial','legacy-settings','preferences');fourEdgesAndFourCorners=$true;centerAndUncapturedDoNotMove=$true;cameraPositionChanged=$true;samples=30;captureStateInjectedForTranslatorTest=$true;manualFocusAndMultipleMonitorTestsPerformed=$false}
 $verification|ConvertTo-Json -Depth 4|Set-Content (Join-Path $output 'verification.json') -Encoding utf8
 Write-Output ($Edition+': PASS camera motion at 8 edges/corners after 3 input-mode applications; center and uncaptured blocked.')
}finally{[Environment]::SetEnvironmentVariable('GENERALS_TEST_EDGE_SCROLL',$old)}
