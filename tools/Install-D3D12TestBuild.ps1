[CmdletBinding(SupportsShouldProcess)]
param([string]$GameDirectory = 'E:\C&C ZH GPTMOD\CaCGZH')
$ErrorActionPreference = 'Stop'
$repositoryPath = Split-Path $PSScriptRoot -Parent
$sourceExe = Join-Path $repositoryPath '.build\community-baseline-ninja\GeneralsMD\generalszh.exe'
$sourceDll = Join-Path $repositoryPath '.build\d3d12-bridge\generals-d3d12.dll'
foreach ($path in @($sourceExe, $sourceDll)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw ('Missing build: ' + $path) }
}
$bytes = [IO.File]::ReadAllBytes($sourceExe)
if (-not [Text.Encoding]::Unicode.GetString($bytes).Contains('GENERALS_RENDERER')) { throw 'Game executable lacks the selectable D3D12 loader.' }
if ([Text.Encoding]::GetEncoding(28591).GetString($bytes).Contains('NeuralAntiAliasing')) { throw 'Do not install the unfinished DLAA menu preview.' }
if (-not (Test-Path -LiteralPath (Join-Path $GameDirectory 'game.dat') -PathType Leaf)) { throw 'Select the installed Zero Hour directory.' }
if (Get-Process -Name 'generalszh-d3d12-test' -ErrorAction SilentlyContinue) { throw 'Close the DX12 test game before installing.' }
$destinationExe = Join-Path $GameDirectory 'generalszh-d3d12-test.exe'
if ($PSCmdlet.ShouldProcess($GameDirectory, 'Install separate DirectX12 bridge test executable, DLL, license and launcher')) {
    $fileMap = @{
        $destinationExe = $sourceExe
        (Join-Path $GameDirectory 'generals-d3d12.dll') = $sourceDll
        (Join-Path $GameDirectory 'd3d8to9-LICENSE.md') = (Join-Path $repositoryPath '.build\d3d12-bridge\d3d8to9-LICENSE.md')
    }
    foreach ($destination in $fileMap.Keys) {
        if (Test-Path -LiteralPath $destination) { Copy-Item -LiteralPath $destination -Destination ($destination + '.' + [Guid]::NewGuid().ToString('N') + '.bak') }
        Copy-Item -LiteralPath $fileMap[$destination] -Destination $destination -Force
    }
    $launcherPath = Join-Path $GameDirectory 'Test-DX12.cmd'
    if (Test-Path -LiteralPath $launcherPath) { Copy-Item -LiteralPath $launcherPath -Destination ($launcherPath + '.' + [Guid]::NewGuid().ToString('N') + '.bak') }
    [IO.File]::WriteAllLines($launcherPath, @('@echo off', 'cd /d "%~dp0"', 'set "GENERALS_RENDERER=d3d12"', 'set "GENERALS_RENDER_FPS=0"', 'set "GENERALS_FPS_PROFILE=1"', '"%~dp0generalszh-d3d12-test.exe"', 'if errorlevel 1 pause'), [Text.Encoding]::ASCII)
    [ordered]@{
        executable = $destinationExe
        executableSha256 = (Get-FileHash -LiteralPath $destinationExe).Hash
        bridgeSha256 = (Get-FileHash -LiteralPath (Join-Path $GameDirectory 'generals-d3d12.dll')).Hash
        bridgeSource = 'https://github.com/crosire/d3d8to9'
        bridgeCommit = '255338f698c8270b537f0a91a13f795f4f988250'
        renderer = 'D3D8 -> D3D9On12 -> D3D12 compatibility bridge'
        nativeEnginePortComplete = $false
        dlaaIntegrated = $false
        gameRuntimeVerified = $false
    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $repositoryPath '.build\d3d12-install.json') -Encoding UTF8
    Write-Output ('Installed separate DX12 test launcher: ' + $launcherPath)
}
