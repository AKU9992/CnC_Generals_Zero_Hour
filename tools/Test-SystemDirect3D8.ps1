[CmdletBinding()]
param([string]$GameDirectory = 'E:\C&C ZH GPTMOD\CaCGZH')
$ErrorActionPreference = 'Stop'
$repositoryPath = Split-Path $PSScriptRoot -Parent
$vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installationPath = & $vswherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
# This build and the Windows Direct3D8 runtime used by it are Win32.
foreach ($architecture in @('x86')) {
    $outputPath = Join-Path $repositoryPath ('.build\system-d3d8-tests\' + $architecture)
    $null = New-Item -ItemType Directory -Path $outputPath -Force
    $exePath = Join-Path $outputPath 'SystemDirect3D8Tests.exe'
    $sourcePath = Join-Path $PSScriptRoot 'native\SystemDirect3D8Tests.cpp'
    $environmentScript = Join-Path $installationPath ('VC\Auxiliary\Build\vcvars' + $(if ($architecture -eq 'x86') { '32' } else { '64' }) + '.bat')
    $commandPath = Join-Path $outputPath 'build.cmd'
    [IO.File]::WriteAllLines($commandPath, @('@echo off', ('call "' + $environmentScript + '"'), 'if errorlevel 1 exit /b %errorlevel%', ('cl.exe /nologo /W4 /WX /EHsc /std:c++17 "' + $sourcePath + '" /Fo"' + $outputPath + '\test.obj" /Fe"' + $exePath + '"'), 'exit /b %errorlevel%'), [Text.Encoding]::Default)
    & cmd.exe /d /c $commandPath
    if ($LASTEXITCODE -ne 0) { throw "System Direct3D8 loader compilation failed: $architecture" }
    & $exePath $GameDirectory
    if ($LASTEXITCODE -ne 0) { throw "System Direct3D8 loader test failed: $architecture / $LASTEXITCODE" }
}
$gameExePath = Join-Path $repositoryPath '.build\community-baseline-ninja\GeneralsMD\generalszh.exe'
$importsPath = Join-Path $outputPath 'game-dependencies.txt'
$commandPath = Join-Path $outputPath 'imports.cmd'
[IO.File]::WriteAllLines($commandPath, @('@echo off', ('call "' + $environmentScript + '"'), 'if errorlevel 1 exit /b %errorlevel%', ('dumpbin.exe /nologo /dependents "' + $gameExePath + '" > "' + $importsPath + '"'), 'exit /b %errorlevel%'), [Text.Encoding]::Default)
& cmd.exe /d /c $commandPath
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect the game import table.' }
if ([IO.File]::ReadAllText($importsPath) -match '(?im)^\s*d3d8\.dll\s*$') { throw 'The game still statically imports the local Direct3D8 proxy.' }
Write-Output 'PASS: game executable has no static d3d8.dll import.'
