[CmdletBinding()]
param([ValidateSet('x86','x64')][string]$Architecture = 'x86',[switch]$Neural)
$ErrorActionPreference = 'Stop'
$repositoryPath = Split-Path $PSScriptRoot -Parent
$sourcePath = Join-Path $PSScriptRoot 'native\D3D12BridgeTests.cpp'
$includePath = Join-Path $repositoryPath '.build\d3d8to9-reference\d3d8to9-255338f698c8270b537f0a91a13f795f4f988250\source'
$outputPath = Join-Path $repositoryPath $(if ($Architecture -eq 'x64') { '.build\d3d12-bridge-x64' } else { '.build\d3d12-bridge' })
$dllPath = Join-Path $outputPath 'generals-d3d12.dll'
if (-not (Test-Path -LiteralPath $dllPath)) { throw 'Build the bridge first.' }
if($Neural) {
    if($Architecture -ne 'x64') {throw 'NVIDIA neural renderer requires x64.'}
    & (Join-Path $PSScriptRoot 'Copy-StreamlineRuntime.ps1') -ExecutableDirectory $outputPath
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$exePath = Join-Path $outputPath 'D3D12BridgeTests.exe'
$commandPath = Join-Path $outputPath 'test-build.cmd'
$environmentScript = Join-Path $installation $(if ($Architecture -eq 'x64') { 'VC\Auxiliary\Build\vcvars64.bat' } else { 'VC\Auxiliary\Build\vcvars32.bat' })
[IO.File]::WriteAllLines($commandPath, @('@echo off', ('call "' + $environmentScript + '"'), 'if errorlevel 1 exit /b %errorlevel%',
    ('cl.exe /nologo /W4 /WX /EHsc /std:c++17 /I"' + $includePath + '" "' + $sourcePath + '" /Fo"' + $outputPath + '\tests.obj" /Fe"' + $exePath + '" /link user32.lib'), 'exit /b %errorlevel%'), [Text.Encoding]::Default)
& cmd.exe /d /c $commandPath
if ($LASTEXITCODE -ne 0) { throw 'Bridge test compilation failed.' }
Push-Location $outputPath
try {
    if($Neural) {& $exePath $dllPath 'neural'} else {& $exePath $dllPath}
    if ($LASTEXITCODE -ne 0) { throw ('D3D12 bridge test failed: ' + $LASTEXITCODE) }
    if($Neural) {
        [ordered]@{bridgeSha256=(Get-FileHash -LiteralPath $dllPath).Hash; resolutions=@('3440x1440','3840x2160'); modes=@('DLSSQuality','DLAA'); switchingVerified=$true; edgeCoverageVerified=$true; partialViewportAspectVerified=$true; nativeHudReadbackVerified=$true; deviceReleaseVerified=$true} |
            ConvertTo-Json | Set-Content -LiteralPath (Join-Path $repositoryPath '.build\neural-renderer-validation.json') -Encoding UTF8
    }
} finally { Pop-Location }
