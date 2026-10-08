[CmdletBinding()]
param([ValidateSet('Generals','ZeroHour')][string]$Edition='ZeroHour',[int]$Seconds=25,[switch]$InitializeOptions)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$old=[Environment]::GetEnvironmentVariable('GENERALS_FPS_PROFILE')
try{
 $env:GENERALS_FPS_PROFILE='1'
 & (Join-Path $PSScriptRoot 'Test-NativeW3DGame.ps1') -Edition $Edition -GameDirectory (Join-Path $repo $(if($Edition -eq 'Generals'){'.build/native-client-release/install-check-fix2/CaCG'}else{'.build/native-client-release/install-check-fix2/CaCGZH'})) -BaseGameDirectory (Join-Path $repo '.build/native-client-release/install-check-fix2/CaCG') -GuiLayout 'Menus/OptionsMenu.wnd' -InitializeOptions:$InitializeOptions -Width 1920 -Height 1080 -Seconds $Seconds -NeuralMode Off | Out-Null
 $user=Join-Path $repo ('.build/native12/user-'+$Edition+'-Off')
 $elapsed=0.0;$samples=@()
 foreach($row in (Import-Csv (Join-Path $user 'FramePerformance.csv'))){$elapsed+=[double]$row.frame_ms;if($elapsed -gt 8000){$samples+=$row}}
 if($samples.Count -lt 100){throw 'Insufficient options menu samples.'}
 $gpu=@(Import-Csv (Join-Path $user 'AKU9992-render-timing.csv') | Where-Object {[double]$_.elapsed_ms -gt 8000})
 $result=[ordered]@{publisher='AKU9992';edition=$Edition;layout='Menus/OptionsMenu.wnd';initialized=$InitializeOptions.IsPresent;resolution='1920x1080';samples=$samples.Count;meanFps=1000/($samples.frame_ms|Measure-Object -Average).Average;cpuRecordMs=($gpu.cpu_record_ms|Measure-Object -Average).Average;gpuMs=($gpu.gpu_ms|Measure-Object -Average).Average;scope='Automatic layout rendering on shell scene; interactive controls need manual testing'}
 $out=Join-Path $repo '.build/performance/options';$null=New-Item -ItemType Directory $out -Force
 $result|ConvertTo-Json|Set-Content (Join-Path $out ($Edition+'-options.json')) -Encoding utf8
 $result
}finally{[Environment]::SetEnvironmentVariable('GENERALS_FPS_PROFILE',$old)}
