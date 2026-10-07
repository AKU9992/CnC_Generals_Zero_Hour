[CmdletBinding()]
param([string[]]$DdsSamples = @())
$ErrorActionPreference = 'Stop'
$repositoryPath = Split-Path $PSScriptRoot -Parent
$outputPath = Join-Path $repositoryPath '.build\d3d12-bridge-x64'
$sdkPath = Join-Path $repositoryPath '.build\game-x64\dx8-sdk'
$gameSourcePath = Join-Path $repositoryPath '.build\community-reference-x64\GeneralsGameCode-b805c12ee1aedc0a4b241006803b8e04bbf288a6'
$gameIncludes = ' /I"' + $gameSourcePath + '\GeneralsMD\Code\Libraries\Source\WWVegas\WW3D2" /I"' + $gameSourcePath + '\Core\Libraries\Source\WWVegas" /I"' + $gameSourcePath + '\Dependencies\Utility" /I"' + $gameSourcePath + '\Core\Libraries\Include" /I"' + $gameSourcePath + '\Dependencies\Precompiled"'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$sourcePath = Join-Path $PSScriptRoot 'native\D3DX8CompatibilityTests.cpp'
$compatPath = Join-Path $repositoryPath 'renderer\D3DX8CompatibilityX64.cpp'
$exePath = Join-Path $outputPath 'D3DX8CompatibilityTests.exe'
$commandPath = Join-Path $outputPath 'compat-test-build.cmd'
[IO.File]::WriteAllLines($commandPath, @('@echo off', ('call "' + $installation + '\VC\Auxiliary\Build\vcvars64.bat"'), 'if errorlevel 1 exit /b %errorlevel%',
    ('cl.exe /nologo /W4 /WX /EHsc /std:c++20 /Zc:__cplusplus /DNOMINMAX /I"' + $sdkPath + '"' + $gameIncludes + ' "' + $sourcePath + '" "' + $compatPath + '" /Fo"' + $outputPath.Replace('\','/') + '/" /Fe"' + $exePath + '" /link user32.lib'), 'exit /b %errorlevel%'), [Text.Encoding]::Default)
& cmd.exe /d /c $commandPath
if ($LASTEXITCODE -ne 0) { throw 'D3DX compatibility test compilation failed.' }
Push-Location $outputPath
try {
    & $exePath (Join-Path $outputPath 'generals-d3d12.dll') @DdsSamples
    if ($LASTEXITCODE -ne 0) { throw 'D3DX compatibility GPU test failed.' }
} finally { Pop-Location }
