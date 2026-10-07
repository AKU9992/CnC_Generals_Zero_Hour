[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repositoryPath = Split-Path $PSScriptRoot -Parent
$sdkPath = Join-Path $repositoryPath '.build\streamline-sdk-v2.14.1'
if (-not (Test-Path -LiteralPath (Join-Path $sdkPath 'include\sl.h'))) { throw 'Run Get-StreamlineSdk.ps1 first.' }
$signature = Get-AuthenticodeSignature -LiteralPath (Join-Path $sdkPath 'bin\x64\sl.interposer.dll')
if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'NVIDIA') { throw 'Invalid NVIDIA interposer signature.' }
$vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installationPath = & $vswherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$environmentScript = Join-Path $installationPath 'VC\Auxiliary\Build\vcvars64.bat'
$outputPath = Join-Path $repositoryPath '.build\dlaa-readiness'
$null = New-Item -ItemType Directory -Path $outputPath -Force
$exePath = Join-Path $outputPath 'DlaaReadiness.exe'
$sourcePath = Join-Path $PSScriptRoot 'native\DlaaReadiness.cpp'
$commandPath = Join-Path $outputPath 'build.cmd'
[IO.File]::WriteAllLines($commandPath, @('@echo off', ('call "' + $environmentScript + '"'), 'if errorlevel 1 exit /b %errorlevel%', ('cl.exe /nologo /W4 /WX /EHsc /std:c++17 /I"' + $sdkPath + '\include" "' + $sourcePath + '" /Fo"' + $outputPath + '\test.obj" /Fe"' + $exePath + '" /link d3d12.lib d3d9.lib d3dcompiler.lib user32.lib dxgi.lib'), 'exit /b %errorlevel%'), [Text.Encoding]::Default)
& cmd.exe /d /c $commandPath
if ($LASTEXITCODE -ne 0) { throw 'DLAA readiness probe compilation failed.' }
Push-Location $outputPath
try {
    & $exePath $sdkPath
    if ($LASTEXITCODE -ne 0) { throw "DLAA readiness probe failed: $LASTEXITCODE" }
} finally { Pop-Location }
