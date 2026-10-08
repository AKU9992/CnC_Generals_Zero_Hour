[CmdletBinding()]
param([string]$GameDirectory,[ValidateSet('Generals','ZeroHour')][string]$Edition='Generals',[ValidateSet('Off','DLAA','DLSSQuality')][string]$NeuralMode='Off',[ValidateRange(5,40)][int]$Seconds=20,[int]$Width=1280,[int]$Height=720,[string]$ShellMap,[string]$BaseGameDirectory,[string]$GuiLayout,[switch]$InitializeOptions)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(-not $GameDirectory){$GameDirectory=Join-Path $root $(if($Edition -eq 'Generals'){'.build/original-client/CaCG'}else{'.build/original-client/CaCGZH'});if($Edition -eq 'ZeroHour' -and -not $BaseGameDirectory){$BaseGameDirectory=Join-Path $root '.build/original-client/CaCG'}}
if($BaseGameDirectory){$BaseGameDirectory=[IO.Path]::GetFullPath($BaseGameDirectory).TrimEnd('\','/')+'\';if(-not (Test-Path -LiteralPath $BaseGameDirectory)){throw 'Base Generals assets not found.'}}
# The automated probe supplies the base installation path to the game's archive
# loader, preserving expansion priority without modifying the registry or assets.
$exe=Join-Path $root $(if($Edition -eq 'Generals'){'.build/game-generals-native12/Generals/generalsv.exe'}else{'.build/game-zerohour-native12/GeneralsMD/generalszh.exe'})
$dll=Join-Path $root '.build/native12/generals-native12.dll'
if(-not (Test-Path (Join-Path $GameDirectory 'game.dat'))){throw 'Game assets not found.'}
foreach($path in @($exe,$dll)){
    $bytes=[IO.File]::ReadAllBytes($path);$pe=[BitConverter]::ToInt32($bytes,0x3c)
    if([BitConverter]::ToUInt16($bytes,$pe+4) -ne 0x8664){throw 'Native game requires AMD64 binaries.'}
}
Copy-Item -LiteralPath $dll -Destination (Split-Path $exe -Parent) -Force
if($NeuralMode -ne 'Off'){& (Join-Path $PSScriptRoot 'Copy-StreamlineRuntime.ps1') -ExecutableDirectory (Split-Path $exe -Parent)}
$userDirectory=Join-Path $root ('.build/native12/user-'+$Edition+'-'+$NeuralMode)
$null=New-Item -ItemType Directory -Path $userDirectory -Force
[IO.File]::WriteAllText((Join-Path $userDirectory 'Options.ini'),"StaticGameLOD = High`r`nIdealStaticGameLOD = High`r`nUseShadowVolumes = yes`r`nUseShadowDecals = yes`r`n")
$capture=Join-Path $root ('.build/native12/game-'+$Edition+'-'+$NeuralMode)
if(Test-Path -LiteralPath ($capture+'.native.bmp')){Remove-Item -LiteralPath ($capture+'.native.bmp')}
$variables=@{GENERALS_TEST_GUI_INITIALIZE=$(if($InitializeOptions){"1"}else{$null});GENERALS_TEST_GUI_LAYOUT=$GuiLayout;GENERALS_RENDERER='native12';GENERALS_NEURAL_AA=$NeuralMode;GENERALS_TEST_QUIT_SECONDS=[string]$Seconds;GENERALS_TEST_USER_DATA=$userDirectory+'\';GENERALS_TEST_BASE_GAME=$BaseGameDirectory;GENERALS_TEST_SHELL_MAP=$ShellMap;GENERALS_X64_STARTUP_TRACE='1';GENERALS_CAPTURE_FRAME=$capture;GENERALS_RENDER_FPS='0'}
$previous=@{};$process=$null
try{
    foreach($name in $variables.Keys){$previous[$name]=[Environment]::GetEnvironmentVariable($name);[Environment]::SetEnvironmentVariable($name,$variables[$name])}
    $arguments='-useCwd -win -nologo -xres '+$Width+' -yres '+$Height
    if($ShellMap){$arguments+=' -shellmap "'+$ShellMap+'"'}
    $process=Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $GameDirectory -WindowStyle Hidden -PassThru
    $watch=[Diagnostics.Stopwatch]::StartNew();$graphicsModules=@();$exited=$false
    while($watch.Elapsed.TotalSeconds -lt 55){
        if($process.WaitForExit(1000)){$exited=$true;break}
        try{$process.Refresh();$graphicsModules=@($graphicsModules+@($process.Modules | Where-Object { $_.ModuleName -match '^d3d(8|9|12)\.dll$|^generals-.*\.dll$' } | Select-Object -ExpandProperty ModuleName) | Sort-Object -Unique)}catch{}
    }
    if(-not $exited){Stop-Process -Id $process.Id;$process.WaitForExit()}
    $log=if(Test-Path (Join-Path $GameDirectory 'GeneralsNative12.log')){[IO.File]::ReadAllLines((Join-Path $GameDirectory 'GeneralsNative12.log')) | Where-Object { $_.StartsWith('PID '+$process.Id+':') }}else{@()}
    $joined=$log -join "`n"
    $textureLog=Join-Path $GameDirectory 'GeneralsTextureFailures.log'
    $missingTextures=if(Test-Path -LiteralPath $textureLog){@([IO.File]::ReadAllLines($textureLog) | Where-Object { $_.StartsWith('PID '+$process.Id+':') -and $_.Contains('Missing texture') })}else{@()}
    $result=[ordered]@{edition=$Edition;mode=$NeuralMode;processId=$process.Id;gracefulExit=($exited -and $process.ExitCode -eq 0);exitCode=$process.ExitCode;nativeDevice=$joined.Contains('Native AMD64 D3D12 device created');draw=$joined.Contains('First native W3D draw completed');present=$joined.Contains('First native W3D Present completed');water=$joined.Contains('Native water HLSL draw completed');stencilShadow=$joined.Contains('Native stencil shadow draw completed');neural=$joined.Contains('32 native W3D DLSS/DLAA world frames completed');graphicsModules=$graphicsModules;executableSha256=(Get-FileHash -LiteralPath $exe).Hash;backendSha256=(Get-FileHash -LiteralPath $dll).Hash;capture=$capture+'.native.bmp';log=$log}
    $result.missingTextureCount=$missingTextures.Count;$result.missingTextures=$missingTextures
    $json=$result | ConvertTo-Json -Depth 5;$json | Set-Content ($capture+'.json') -Encoding UTF8;Write-Output $json
    if($missingTextures.Count -gt 10){throw 'Game scene is missing its texture assets; verify the base game path.'}
    if(-not $result.gracefulExit -or -not $result.draw -or -not $result.present -or $joined.Contains('draw failed') -or -not $joined.Contains('Native runtime audit: D3D12 present, D3D8/D3D9 absent') -or -not (Test-Path -LiteralPath $result.capture) -or $graphicsModules -contains 'd3d8.dll' -or $graphicsModules -contains 'd3d9.dll' -or ($NeuralMode -ne 'Off' -and -not $result.neural)){throw 'Native W3D game validation failed.'}
}finally{
    if($process -and -not $process.HasExited){Stop-Process -Id $process.Id}
    foreach($name in $previous.Keys){[Environment]::SetEnvironmentVariable($name,$previous[$name])}
}
