[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repositoryPath = Split-Path $PSScriptRoot -Parent
$outputPath = Join-Path $repositoryPath '.build\resource-interop'
$null = New-Item -ItemType Directory -Path $outputPath -Force
$vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installationPath = & $vswherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$environmentScript = Join-Path $installationPath 'VC\Auxiliary\Build\vcvars64.bat'
$sourcePath = Join-Path $PSScriptRoot 'native\D3D9On12ResourcesTests.cpp'
$exePath = Join-Path $outputPath 'D3D9On12ResourcesTests.exe'
$commandPath = Join-Path $outputPath 'build.cmd'
[IO.File]::WriteAllLines($commandPath, @('@echo off', ('call "' + $environmentScript + '"'), 'if errorlevel 1 exit /b %errorlevel%', ('cl.exe /nologo /W4 /WX /EHsc /std:c++17 "' + $sourcePath + '" /Fo"' + $outputPath + '\test.obj" /Fe"' + $exePath + '" /link d3d9.lib d3d12.lib user32.lib'), 'exit /b %errorlevel%'), [Text.Encoding]::Default)
& cmd.exe /d /c $commandPath
if ($LASTEXITCODE -ne 0) { throw 'Resource interoperability test compilation failed.' }
& $exePath
if ($LASTEXITCODE -ne 0) { throw "Resource interoperability test failed: $LASTEXITCODE" }
