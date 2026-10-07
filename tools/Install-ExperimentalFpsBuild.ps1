[CmdletBinding(SupportsShouldProcess)]
param([string]$GameDirectory = 'E:\C&C ZH GPTMOD\CaCGZH')
$ErrorActionPreference = 'Stop'
$repositoryPath = Split-Path $PSScriptRoot -Parent
$sourceExe = Join-Path $repositoryPath '.build\community-baseline-ninja\GeneralsMD\generalszh.exe'
if (-not (Test-Path -LiteralPath $sourceExe -PathType Leaf)) { throw 'The experimental executable has not been built.' }
$binaryText = [Text.Encoding]::GetEncoding(28591).GetString([IO.File]::ReadAllBytes($sourceExe))
if ($binaryText.Contains('NeuralAntiAliasing')) {
    throw 'This is the unfinished DLAA menu development build. Keep the verified FPS executable installed.'
}
if (-not $binaryText.Contains('GENERALS_RENDER_FPS') -or -not $binaryText.Contains('GENERALS_FPS_PROFILE')) {
    throw 'This executable does not contain our experimental FPS and profiling hooks.'
}
if (-not (Test-Path -LiteralPath (Join-Path $GameDirectory 'game.dat') -PathType Leaf)) { throw 'Select the installed Zero Hour game directory.' }
if (Get-Process -Name 'generalszh-fps-test' -ErrorAction SilentlyContinue) { throw 'Close the experimental game before installing a new build.' }
$destinationExe = Join-Path $GameDirectory 'generalszh-fps-test.exe'
if ($PSCmdlet.ShouldProcess($GameDirectory, 'Install separate experimental executable and two test launchers')) {
    $filesToWrite = @($destinationExe, (Join-Path $GameDirectory 'Test-FPS-Uncapped.cmd'), (Join-Path $GameDirectory 'Test-FPS-30.cmd'))
    foreach ($destinationPath in $filesToWrite) {
        if (Test-Path -LiteralPath $destinationPath) {
            Copy-Item -LiteralPath $destinationPath -Destination ($destinationPath + '.' + [Guid]::NewGuid().ToString('N') + '.bak')
        }
    }
    Copy-Item -LiteralPath $sourceExe -Destination $destinationExe -Force
    foreach ($renderLimit in @(0,30)) {
        $launcherName = if ($renderLimit -eq 0) { 'Test-FPS-Uncapped.cmd' } else { 'Test-FPS-30.cmd' }
        $launcher = @('@echo off', 'cd /d "%~dp0"', 'set "GENERALS_FPS_PROFILE=1"', ('set "GENERALS_RENDER_FPS=' + $renderLimit + '"'), '"%~dp0generalszh-fps-test.exe"', 'if errorlevel 1 pause')
        [IO.File]::WriteAllLines((Join-Path $GameDirectory $launcherName), $launcher, [Text.Encoding]::ASCII)
    }
    $manifest = [ordered]@{
        referenceRepository = 'https://github.com/TheSuperHackers/GeneralsGameCode'
        referenceCommit = 'b805c12ee1aedc0a4b241006803b8e04bbf288a6'
        experimentalExe = $destinationExe
        sha256 = (Get-FileHash -LiteralPath $destinationExe -Algorithm SHA256).Hash
        graphicsApi = 'DirectX 8'
        simulationTargetHz = 30
        runtimeVerified = $false
    }
    $manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $repositoryPath '.build\experimental-install.json') -Encoding UTF8
    Write-Output ('Installed: ' + $destinationExe)
}
