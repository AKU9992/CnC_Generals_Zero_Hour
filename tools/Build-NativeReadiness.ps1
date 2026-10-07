[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswherePath)) { throw 'MSVC Build Tools are not installed yet.' }
$installationPath = & $vswherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installationPath) { throw 'No complete MSVC x86/x64 toolset installation found.' }
$environmentScript = Join-Path $installationPath 'VC\Auxiliary\Build\vcvars64.bat'
$repositoryPath = Split-Path $PSScriptRoot -Parent
$outputDirectory = Join-Path $repositoryPath '.build\native-readiness'
$null = New-Item -ItemType Directory -Path $outputDirectory -Force
$sourcePath = Join-Path $PSScriptRoot 'native\D3D12Capabilities.cpp'
$exePath = Join-Path $outputDirectory 'D3D12Capabilities.exe'
$objectPath = Join-Path $outputDirectory 'D3D12Capabilities.obj'
$commandPath = Join-Path $outputDirectory 'build.cmd'
$commands = @(
    '@echo off',
    ('call "' + $environmentScript + '"'),
    'if errorlevel 1 exit /b %errorlevel%',
    ('cl.exe /nologo /W4 /WX /EHsc /std:c++17 /O2 "' + $sourcePath + '" /Fo"' + $objectPath + '" /Fe"' + $exePath + '" /link d3d12.lib dxgi.lib'),
    'exit /b %errorlevel%'
)
[IO.File]::WriteAllLines($commandPath, $commands, [Text.Encoding]::Default)
& cmd.exe /d /c $commandPath
if ($LASTEXITCODE -ne 0) { throw "Readiness probe build failed with code $LASTEXITCODE." }
Write-Output "Built: $exePath"
& $exePath
if ($LASTEXITCODE -ne 0) { throw "Readiness probe failed with code $LASTEXITCODE." }
