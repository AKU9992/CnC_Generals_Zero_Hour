[CmdletBinding()]
param([string]$Before='.build/performance/before-repeat/results.json',[string]$After='.build/performance/after-matched/results.json',[string]$Gameplay='.build/performance/gameplay-valid',[string]$Output='docs/native12-fix4.md')
$ErrorActionPreference='Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
function N($v){if($null -eq $v){return '—'};([double]$v).ToString('0.00',[Globalization.CultureInfo]::InvariantCulture)}
$b=Get-Content $Before -Raw|ConvertFrom-Json;$a=Get-Content $After -Raw|ConvertFrom-Json
$c=@('| Сцена | Разрешение | FPS до → после | 1% low до → после | p99 мс до → после | CPU мс до → после | GPU мс после |','|---|---|---:|---:|---:|---:|---:|')
$d=@('| Сцена после | 0,1% low | Медиана / p95 / p99, мс | Пики >50 мс | RAM MiB | VRAM MiB | Тики/с |','|---|---:|---:|---:|---:|---:|---:|')
foreach($r in $a){$old=$b|Where-Object scenario -eq $r.scenario
$c+='| '+$r.scenario+' | '+$r.resolution+' | '+(N $old.meanFps)+' → '+(N $r.meanFps)+' | '+(N $old.onePercentLow)+' → '+(N $r.onePercentLow)+' | '+(N $old.p99Ms)+' → '+(N $r.p99Ms)+' | '+(N $old.cpuActiveMsPerFrame)+' → '+(N $r.cpuActiveMsPerFrame)+' | '+(N $r.gpuMs)+' |'
$d+='| '+$r.scenario+' | '+(N $r.pointOnePercentLow)+' | '+(N $r.medianMs)+' / '+(N $r.p95Ms)+' / '+(N $r.p99Ms)+' | '+$r.spikesOver50Ms+' | '+(N $r.peakWorkingSetMiB)+' | '+(N $r.vramMiB)+' | '+(N $r.logicHz)+' |'
}
$g=@('| Сценарий | Режим | Цель FPS | FPS | 1% low | p99 мс | Тики/с | Измерение с |','|---|---|---:|---:|---:|---:|---:|---:|')
foreach($p in (Get-ChildItem $Gameplay -Filter result.json -Recurse -File|Sort-Object FullName)){$r=Get-Content $p.FullName -Raw|ConvertFrom-Json;$g+='| '+$r.scenario+' | '+$r.fpsMode+' | '+(N $r.target_fps)+' | '+(N $r.meanFps)+' | '+(N $r.onePercentLow)+' | '+(N $r.p99Ms)+' | '+(N $r.logicHz)+' | '+(N $r.seconds)+' |'}
$t=[IO.File]::ReadAllText('docs/native12-fix4.template.md').Replace('@@COMPARE@@',($c -join [Environment]::NewLine)).Replace('@@DETAILS@@',($d -join [Environment]::NewLine)).Replace('@@GAMEPLAY@@',($g -join [Environment]::NewLine))
[IO.File]::WriteAllText([IO.Path]::GetFullPath($Output),$t,[Text.UTF8Encoding]::new($false))
Write-Output 'AKU9992 performance report generated.'
