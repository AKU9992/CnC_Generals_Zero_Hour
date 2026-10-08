[CmdletBinding()]
param([string]$BinaryRoot,[string]$AssetRoot,[string]$OutputRoot,[int]$Seconds=20,[string]$FpsMode='Diagnostic',[double]$CustomFps=80,[switch]$Extended,[string]$Scenario,[string]$Map,[ValidateSet('Generals','ZeroHour')][string]$Edition='ZeroHour',[string]$LoadSave,[string]$LoadReplay,[switch]$LegacyMousePreferences)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
if(-not $AssetRoot){$AssetRoot=Join-Path $repo '.build/native-client-release/install-check-fix2'}
if(-not $OutputRoot){$OutputRoot=Join-Path $repo '.build/performance/current'}
$OutputRoot=[IO.Path]::GetFullPath($OutputRoot);$AssetRoot=[IO.Path]::GetFullPath($AssetRoot);if($BinaryRoot){$BinaryRoot=[IO.Path]::GetFullPath($BinaryRoot)}
$null=New-Item -ItemType Directory $OutputRoot -Force
function Get-Quantile($Sorted,[double]$P){return $Sorted[[Math]::Max(0,[Math]::Min($Sorted.Count-1,[Math]::Ceiling($P*$Sorted.Count)-1))]}
function Get-TailLow($Sorted,[double]$P){$n=[Math]::Max(1,[Math]::Ceiling($Sorted.Count*$P));$sum=0.0;for($i=$Sorted.Count-$n;$i -lt $Sorted.Count;$i++){$sum+=$Sorted[$i]};return 1000*$n/$sum}
$cases=@(@{Id='Generals-Lake-Native';Game='CaCG';Mode='Off';Map='maps\Tournament Lake\Tournament Lake.map';Width=1920;Height=1080},@{Id='ZeroHour-Battle-Native';Game='CaCGZH';Mode='Off';Map=$null;Width=1920;Height=1080},@{Id='ZeroHour-Battle-DLSS';Game='CaCGZH';Mode='DLSSQuality';Map=$null;Width=1920;Height=1080},@{Id='ZeroHour-Battle-DLAA-4K';Game='CaCGZH';Mode='DLAA';Map=$null;Width=3840;Height=2160})
if($Scenario){$cases=@(@{Id=($Edition+'-'+$Scenario+'-'+$FpsMode+'-'+$CustomFps);Game=if($Edition -eq 'Generals'){'CaCG'}else{'CaCGZH'};Mode='Off';Map=$null;Width=1920;Height=1080})}
if($LoadSave -or $LoadReplay){$cases=@(@{Id=if($LoadSave){'LoadSave'}else{'LoadReplay'};Game=if($Edition -eq 'Generals'){'CaCG'}else{'CaCGZH'};Mode='Off';Map=$null;Width=1920;Height=1080})}
$variables=@('AKU_TEST_SCENARIO','AKU_TEST_MAP','AKU_TEST_SAVE','GENERALS_VSYNC','GENERALS_RENDERER','GENERALS_RENDER_FPS','GENERALS_FPS_MODE','GENERALS_CUSTOM_FPS','GENERALS_FPS_PROFILE','GENERALS_TEST_QUIT_SECONDS','GENERALS_TEST_USER_DATA','GENERALS_TEST_BASE_GAME','GENERALS_BASE_GAME','GENERALS_TEST_SHELL_MAP','GENERALS_NEURAL_AA','GENERALS_CAPTURE_FRAME','GENERALS_TEST_GUI_LAYOUT','GENERALS_BORDERLESS')
$previous=@{};foreach($v in $variables){$previous[$v]=[Environment]::GetEnvironmentVariable($v)}
$results=@()
try{
 foreach($case in $cases){
  $user=Join-Path $OutputRoot $case.Id;$null=New-Item -ItemType Directory $user -Force
  [IO.File]::WriteAllText((Join-Path $user 'Options.ini'),"StaticGameLOD = High`r`nIdealStaticGameLOD = High`r`nUseShadowVolumes = yes`r`nUseShadowDecals = yes`r`n")
  if($LegacyMousePreferences){[IO.File]::AppendAllText((Join-Path $user 'Options.ini'),"CursorCaptureEnabledInWindowedGame = no"+[Environment]::NewLine+"CursorCaptureEnabledInFullscreenGame = no"+[Environment]::NewLine+"ScreenEdgeScrollEnabledInWindowedApp = no"+[Environment]::NewLine+"ScreenEdgeScrollEnabledInFullscreenApp = no"+[Environment]::NewLine)}
  $asset=Join-Path $AssetRoot $case.Game
  $exe=if($BinaryRoot){Join-Path $BinaryRoot ($case.Game+'/Client/generals-client.exe')}elseif($case.Game -eq 'CaCG'){Join-Path $repo '.build/game-generals-native12/Generals/generalsv.exe'}else{Join-Path $repo '.build/game-zerohour-native12/GeneralsMD/generalszh.exe'}
  if(-not $BinaryRoot){Copy-Item (Join-Path $repo '.build/native12/generals-native12.dll') (Split-Path $exe -Parent) -Force}
  $env:AKU_TEST_SCENARIO=if($Scenario){$Scenario}else{$null};$env:AKU_TEST_MAP=$Map;$env:GENERALS_VSYNC='0';$env:GENERALS_RENDERER='native12';$env:GENERALS_RENDER_FPS=if($FpsMode -eq 'Diagnostic'){'0'}else{$null};$env:GENERALS_FPS_MODE=$FpsMode;$env:GENERALS_CUSTOM_FPS=[string]$CustomFps
  $env:GENERALS_FPS_PROFILE='1';$env:GENERALS_TEST_QUIT_SECONDS=[string]$Seconds;$env:GENERALS_TEST_USER_DATA=$user+'\'
  $env:GENERALS_TEST_BASE_GAME=if($case.Game -eq 'CaCGZH'){(Join-Path $AssetRoot 'CaCG')+'\'}else{$null};$env:GENERALS_BASE_GAME=$env:GENERALS_TEST_BASE_GAME
  $env:GENERALS_TEST_SHELL_MAP=$case.Map;$env:GENERALS_NEURAL_AA=$case.Mode;$env:GENERALS_CAPTURE_FRAME=$null;$env:GENERALS_TEST_GUI_LAYOUT=$null;$env:GENERALS_BORDERLESS='0'
  $args='-useCwd -win -nologo -xres '+$case.Width+' -yres '+$case.Height
if($LoadSave){$args+=' -loadsave "'+[IO.Path]::GetFullPath($LoadSave)+'"'}
  if($LoadReplay){$args+=' -loadreplay "'+[IO.Path]::GetFullPath($LoadReplay)+'"'}
  $process=Start-Process $exe -ArgumentList $args -WorkingDirectory $asset -WindowStyle Hidden -PassThru
  $null=$process.Handle;$clock=[Diagnostics.Stopwatch]::StartNew();$peak=0L;$memory=@();$cpuStart=$null;$cpuEnd=$null
  while(-not $process.WaitForExit(200)){
   if($clock.Elapsed.TotalSeconds -gt $Seconds+60){$process.Kill();throw 'Scenario timeout.'}
   $process.Refresh();$peak=[Math]::Max($peak,$process.WorkingSet64)
   if($clock.Elapsed.TotalSeconds -gt 5){if($null -eq $cpuStart){$cpuStart=$process.TotalProcessorTime.TotalMilliseconds};$cpuEnd=$process.TotalProcessorTime.TotalMilliseconds;$memory+=@{seconds=$clock.Elapsed.TotalSeconds;workingSetBytes=$process.WorkingSet64;privateBytes=$process.PrivateMemorySize64}}
  }
  if($process.ExitCode -ne 0){throw ('Scenario exited unsuccessfully: '+$case.Id)}
  if($Scenario){$probe=Join-Path $user 'AKU9992-scenario.log';if(-not (Test-Path $probe) -or (Get-Content $probe -Raw) -notmatch 'Skirmish active'){throw 'Gameplay scenario did not enter skirmish.'}}
  $rows=Import-Csv (Join-Path $user 'FramePerformance.csv');$samples=@();$elapsed=0.0
  foreach($row in $rows){$elapsed+=[double]$row.frame_ms;if($elapsed -gt 5000 -and [double]$row.frame_ms -gt 0){$samples+=$row}}
if($Scenario){
   $lastReset=0;for($j=1;$j -lt $samples.Count;$j++){if([long]$samples[$j].logic_frame -lt [long]$samples[$j-1].logic_frame){$lastReset=$j}}
   $samples=@($samples[$lastReset..($samples.Count-1)] | Where-Object {[long]$_.logic_frame -ge 150})
  }
  if($samples.Count -lt 30){throw 'Insufficient timing samples.'}
  $ms=@($samples|ForEach-Object {[double]$_.frame_ms}|Sort-Object);$mean=($ms|Measure-Object -Average).Average
  $logicDelta=[long]$samples[-1].logic_frame-[long]$samples[0].logic_frame;$time=($ms|Measure-Object -Sum).Sum
  $result=[ordered]@{scenario=$case.Id;neural=$case.Mode;resolution=($case.Width.ToString()+'x'+$case.Height);fpsMode=$FpsMode;customFps=$CustomFps;seconds=$time/1000;samples=$ms.Count;meanFps=1000/$mean;onePercentLow=(Get-TailLow $ms .01);pointOnePercentLow=(Get-TailLow $ms .001);medianMs=(Get-Quantile $ms .5);p95Ms=(Get-Quantile $ms .95);p99Ms=(Get-Quantile $ms .99);spikesOver50Ms=@($ms|Where-Object {$_ -gt 50}).Count;workMs=($samples|ForEach-Object {[double]$_.update_ms}|Measure-Object -Average).Average;limiterMs=($samples|ForEach-Object {[double]$_.limiter_ms}|Measure-Object -Average).Average;logicHz=$logicDelta/(($time-[double]$samples[0].frame_ms)/1000);cpuActiveMsPerFrame=if($null -ne $cpuStart -and -not $Scenario){($cpuEnd-$cpuStart)/$ms.Count}else{$null};peakWorkingSetMiB=$peak/1MB;gpuMs=$null;vramMiB=$null;sceneKind=if($LoadSave){'Loaded current x64 save'}elseif($LoadReplay){'Rendered current x64 replay'}elseif($Scenario){'Playable fixed-seed skirmish'}else{'Scripted shell map; not a playable skirmish'}}
  $gpuPath=Join-Path $user 'AKU9992-render-timing.csv'
  if(Test-Path $gpuPath){
   $gpu=@(Import-Csv $gpuPath | Where-Object {[double]$_.elapsed_ms -gt 5000 -and [long]$_.gpu_serial -gt 0})
   if($gpu.Count){$result.gpuMs=($gpu|ForEach-Object {[double]$_.gpu_ms}|Measure-Object -Average).Average;$result.vramMiB=($gpu|ForEach-Object {[double]$_.local_usage_mib}|Measure-Object -Maximum).Maximum
    foreach($field in @('cpu_record_ms','fence_wait_ms','submit_ms','present_ms')){$result[$field]=($gpu|ForEach-Object {[double]$_.$field}|Measure-Object -Average).Average}
   }
  }
  foreach($field in @('logic_ms','audio_ms','client_ms','messages_ms','network_ms','draw_ms','drawables_ms','ai_ms','pathfind_ms','target_fps')){if($samples[0].PSObject.Properties.Name -contains $field){$result[$field]=($samples|ForEach-Object {[double]$_.$field}|Measure-Object -Average).Average}}
  $result|ConvertTo-Json -Depth 5|Set-Content (Join-Path $user 'result.json') -Encoding utf8
  $memory|ConvertTo-Json -Depth 4|Set-Content (Join-Path $user 'memory.json') -Encoding utf8
  $results+=$result;$results|ConvertTo-Json -Depth 6|Set-Content (Join-Path $OutputRoot 'results.json') -Encoding utf8
  Write-Output ($case.Id+': FPS '+[Math]::Round($result.meanFps,2)+', logic Hz '+[Math]::Round($result.logicHz,2))
 }
}finally{foreach($v in $variables){[Environment]::SetEnvironmentVariable($v,$previous[$v])}}