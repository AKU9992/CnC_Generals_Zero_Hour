[CmdletBinding()]
param([string]$GameDirectory = 'E:\C&C ZH GPTMOD\CaCGZH', [ValidateRange(5,60)][int]$Seconds = 20, [switch]$Installed, [switch]$ShellMap, [switch]$Fullscreen, [switch]$Graceful, [switch]$RequireAudio, [ValidateSet('Off','DLAA','DLSSQuality')][string]$NeuralMode='Off', [ValidateSet('ZeroHour','Generals')][string]$Edition='ZeroHour', [int]$Width=0, [int]$Height=0)
$ErrorActionPreference = 'Stop'
$repositoryPath = Split-Path $PSScriptRoot -Parent
$vanilla=$Edition -eq 'Generals'
if($vanilla -and -not $PSBoundParameters.ContainsKey('GameDirectory')){$GameDirectory='E:\C&C ZH GPTMOD\CaCG'}
$probePrefix=if($vanilla){'generals-x64-game-startup'}else{'x64-game-startup'}
$audioPrefix=if($vanilla){'audio-session-generals'}else{'audio-session'}
$exeName=if($vanilla){'generals-x64-test.exe'}else{'generalszh-x64-test.exe'}
$exePath = Join-Path $repositoryPath $(if($vanilla){'.build\game-generals-x64\Generals\generalsv.exe'}else{'.build\game-x64\GeneralsMD\generalszh.exe'})
$bridgePath = Join-Path $repositoryPath '.build\d3d12-bridge-x64\generals-d3d12.dll'
if ($Installed) {
    $exePath = Join-Path $GameDirectory ('GeneralsGPT-x64\'+$exeName)
    $bridgePath = Join-Path $GameDirectory 'GeneralsGPT-x64\generals-d3d12.dll'
}
foreach ($path in @($exePath, $bridgePath)) {
    $bytes = [IO.File]::ReadAllBytes($path)
    $peOffset = [BitConverter]::ToInt32($bytes, 0x3c)
    if ([BitConverter]::ToUInt16($bytes, $peOffset + 4) -ne 0x8664) { throw "Expected an AMD64 build: $path" }
}
if (-not (Test-Path -LiteralPath (Join-Path $GameDirectory 'game.dat'))) { throw 'Installed game assets not found.' }
if (-not $Installed) { Copy-Item -LiteralPath $bridgePath -Destination (Join-Path (Split-Path $exePath -Parent) 'generals-d3d12.dll') -Force }
if(-not $Installed -and $NeuralMode -ne 'Off') { & (Join-Path $PSScriptRoot 'Copy-StreamlineRuntime.ps1') -ExecutableDirectory (Split-Path $exePath -Parent) }
$neuralLogPath=Join-Path $GameDirectory 'GeneralsNeuralAA.log'
$neuralBeforeLength=if(Test-Path -LiteralPath $neuralLogPath) {[IO.File]::ReadAllText($neuralLogPath).Length} else {0}
$logPath = Join-Path $GameDirectory 'GeneralsD3D12.log'
$beforeLength = if (Test-Path -LiteralPath $logPath) { [IO.File]::ReadAllText($logPath).Length } else { 0 }
$previousCapture = $env:GENERALS_CAPTURE_FRAME
$previousBorderless = $env:GENERALS_BORDERLESS
$previousNativePresent = $env:GENERALS_NATIVE_PRESENT
$capturePath=Join-Path $repositoryPath ('.build/'+$probePrefix+'-'+$NeuralMode+'.bmp')
$framePath=if($vanilla){$capturePath+'.native.bmp'}else{$capturePath}
foreach($oldCapture in @($capturePath,$capturePath+'.native.bmp')){if(Test-Path -LiteralPath $oldCapture){Remove-Item -LiteralPath $oldCapture}}
$env:GENERALS_CAPTURE_FRAME=$capturePath
if($vanilla){$env:GENERALS_BORDERLESS='1';$env:GENERALS_NATIVE_PRESENT='1'}
$previousRenderer = $env:GENERALS_RENDERER
$previousFps = $env:GENERALS_RENDER_FPS
$previousTrace = $env:GENERALS_X64_STARTUP_TRACE
$previousNeural = $env:GENERALS_NEURAL_AA
$previousQuit = $env:GENERALS_TEST_QUIT_SECONDS
$env:GENERALS_TEST_QUIT_SECONDS=if($Graceful) {[string]$Seconds} else {$null}
$env:GENERALS_NEURAL_AA=$NeuralMode
$env:GENERALS_RENDERER = 'd3d12'
$env:GENERALS_RENDER_FPS = '0'
$env:GENERALS_X64_STARTUP_TRACE = '1'
$process = $null
try {
    $arguments = if ($ShellMap) { '-useCwd -win -nologo' } else { '-useCwd -win -quickstart' }
    if($Fullscreen) {$arguments=$arguments.Replace(' -win','')}
    if($Width -gt 0 -and $Height -gt 0){$arguments+=' -xres '+$Width+' -yres '+$Height}
    $process = Start-Process -FilePath $exePath -ArgumentList $arguments -WorkingDirectory $GameDirectory -PassThru
    $audioProbe=$null
    $audioVerified=$false
    if($RequireAudio){
        $audioProbeExe=Join-Path $repositoryPath '.build/audio-tests/X64AudioTests.exe'
        if(-not (Test-Path -LiteralPath $audioProbeExe)){throw 'Build Test-X64Audio.ps1 first.'}
        $audioProbe=Start-Process -FilePath $audioProbeExe -ArgumentList @('--watch',[string]$process.Id) -WorkingDirectory (Split-Path $audioProbeExe -Parent) -WindowStyle Hidden -RedirectStandardOutput (Join-Path $repositoryPath ('.build/'+$audioPrefix+'-'+$NeuralMode+'.log')) -RedirectStandardError (Join-Path $repositoryPath ('.build/'+$audioPrefix+'-'+$NeuralMode+'-error.log')) -PassThru
    }
    $exited = $process.WaitForExit($(if($Graceful) {60000} else {$Seconds * 1000}))
    $gracefulExit = $Graceful -and $exited -and $process.ExitCode -eq 0
    # Capture liveness before stopping this probe; release log handles before
    # collecting the result so GPU/plugin logging cannot race ReadAllText.
    if(-not $exited) { Stop-Process -Id $process.Id; $process.WaitForExit() }
    if($audioProbe){if(-not $audioProbe.WaitForExit(1000)){Stop-Process -Id $audioProbe.Id;$audioProbe.WaitForExit()}else{$audioVerified=$audioProbe.ExitCode -eq 0}}
    $log = if (Test-Path -LiteralPath $logPath) { [IO.File]::ReadAllText($logPath) } else { '' }
    $newLog = if ($log.Length -ge $beforeLength) { $log.Substring($beforeLength) } else { $log }
    $pidLog = ($newLog -split "`n" | Where-Object { $_.StartsWith("PID $($process.Id):") }) -join "`n"
    $neuralLog=if(Test-Path -LiteralPath $neuralLogPath) {[IO.File]::ReadAllText($neuralLogPath)} else {''}
    $neuralLog=if($neuralLog.Length -ge $neuralBeforeLength) {$neuralLog.Substring($neuralBeforeLength)} else {$neuralLog}
    $neuralLog=($neuralLog -split "`n" | Where-Object {$_.StartsWith("PID $($process.Id):")}) -join "`n"
    $renderedFrameVerified=$false
    $litPixelFraction=0.0
    if(Test-Path -LiteralPath $framePath){
        $frameBytes=[IO.File]::ReadAllBytes($framePath)
        $pixelOffset=[BitConverter]::ToInt32($frameBytes,10)
        $lit=0; $sampled=0
        for($index=$pixelOffset;$index+3 -lt $frameBytes.Length;$index+=64){
            $sampled++
            if($frameBytes[$index] -gt 16 -or $frameBytes[$index+1] -gt 16 -or $frameBytes[$index+2] -gt 16){$lit++}
        }
        if($sampled){$litPixelFraction=$lit/$sampled; $renderedFrameVerified=$litPixelFraction -gt 0.25}
    }
    $result = [ordered]@{
        architecture = 'AMD64'
        edition = $Edition
        requestedResolution = if($Width -gt 0 -and $Height -gt 0){[string]$Width+'x'+$Height}else{'Options.ini'}
        executable = $exePath
        executableSha256 = (Get-FileHash -LiteralPath $exePath).Hash
        bridgeSha256 = (Get-FileHash -LiteralPath $bridgePath).Hash
        seconds = $Seconds
        shellMapRequested = $ShellMap.IsPresent
        fullscreenRequested = $Fullscreen.IsPresent
        processId = $process.Id
        stayedAlive = -not $exited -or $gracefulExit
        gracefulExitVerified = [bool]$gracefulExit
        exitCode = if ($exited) { $process.ExitCode } else { $null }
        actualD3D12Device = $pidLog.Contains('Actual ID3D12Device verified: 0x00000000')
        successfulDraw = $pidLog.Contains('First successful D3D12 draw: 0x00000000')
        successfulPresent = $pidLog.Contains('First D3D12 presentation: 0x00000000')
        visualVerified = $false
        renderedFrameVerified = $renderedFrameVerified
        litPixelFraction = $litPixelFraction
        renderedFramePath = $framePath
        nativePresentVerified = $pidLog.Contains('32 native DXGI frames presented: 0x00000000')
        nativeOutputReadbackVerified = $pidLog.Contains('Native DXGI output readback: 0x00000000')
        audioOutputVerified = $audioVerified
        neuralAaIntegrated = $neuralLog.Contains('First actual game neural frame: 0x00000000')
        neuralHistoryVerified = $neuralLog.Contains('32 actual neural frames completed: 0x00000000')
        neuralMode = $NeuralMode
        neuralLog = $neuralLog
        bridgeLog = $pidLog
    }
    $json = $result | ConvertTo-Json
    $json | Set-Content -LiteralPath (Join-Path $repositoryPath ('.build\'+$probePrefix+'.json')) -Encoding UTF8
    $json | Set-Content -LiteralPath (Join-Path $repositoryPath ('.build\'+$probePrefix+'-'+$NeuralMode+'.json')) -Encoding UTF8
    Write-Output $json
} finally {
    $env:GENERALS_CAPTURE_FRAME=$previousCapture
    $env:GENERALS_BORDERLESS=$previousBorderless
    $env:GENERALS_NATIVE_PRESENT=$previousNativePresent
    if ($process -and -not $process.HasExited) {
        $running = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
        if ($running -and $running.Path -eq $exePath) { Stop-Process -Id $process.Id }
    }
    $env:GENERALS_RENDERER = $previousRenderer
    $env:GENERALS_RENDER_FPS = $previousFps
    $env:GENERALS_X64_STARTUP_TRACE = $previousTrace
    $env:GENERALS_NEURAL_AA = $previousNeural
    $env:GENERALS_TEST_QUIT_SECONDS = $previousQuit
}
if($Graceful -and (-not $result.gracefulExitVerified -or ($NeuralMode -ne 'Off' -and -not $result.neuralLog.Contains('Explicit neural shutdown completed')))) {throw 'Ordinary game shutdown failed or hung.'}
if($NeuralMode -ne 'Off' -and (-not $result.neuralAaIntegrated -or -not $result.neuralHistoryVerified -or $result.neuralLog.Contains('Neural frame failed') -or $result.neuralLog.Contains('[error]'))) {throw ('Actual game neural processing failed: '+$NeuralMode)}
if($RequireAudio -and -not $result.audioOutputVerified){throw 'The actual game audio output session remained silent.'}
if (-not $result.stayedAlive -or -not $result.actualD3D12Device -or -not $result.successfulDraw -or -not $result.successfulPresent) {
    throw 'The x64 game has not passed the startup/rendering check. Do not install it as a working game.'
}
