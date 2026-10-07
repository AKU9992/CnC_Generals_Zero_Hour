[CmdletBinding()]
param([string]$DumpPath, [ValidateSet('x86','x64')][string]$Architecture = 'x86')
$ErrorActionPreference = 'Stop'
$repositoryPath = Split-Path $PSScriptRoot -Parent
if (-not $DumpPath) {
    $dumpDirectory = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Command and Conquer Generals Zero Hour Data\CrashDumps'
    $DumpPath = (Get-ChildItem -LiteralPath $dumpDirectory -Filter 'CrashMZ-*.dmp' | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName
}
if (-not $DumpPath) { throw 'No Zero Hour mini dump found.' }
$vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installationPath = & $vswherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$outputPath = Join-Path $repositoryPath ('.build\crash-inspector-' + $Architecture)
$null = New-Item -ItemType Directory -Path $outputPath -Force
$exePath = Join-Path $outputPath 'InspectCrashDump.exe'
$commandPath = Join-Path $outputPath 'build.cmd'
$sourcePath = Join-Path $PSScriptRoot 'native\InspectCrashDump.cpp'
$environmentName = if ($Architecture -eq 'x64') { 'vcvars64.bat' } else { 'vcvars32.bat' }
$environmentScript = Join-Path $installationPath ('VC\Auxiliary\Build\' + $environmentName)
[IO.File]::WriteAllLines($commandPath, @('@echo off', ('call "' + $environmentScript + '"'), 'if errorlevel 1 exit /b %errorlevel%', ('cl.exe /nologo /W4 /WX /EHsc /std:c++17 "' + $sourcePath + '" /Fo"' + $outputPath + '\dump.obj" /Fe"' + $exePath + '"'), 'exit /b %errorlevel%'), [Text.Encoding]::Default)
& cmd.exe /d /c $commandPath
if ($LASTEXITCODE -ne 0) { throw 'Crash inspector compilation failed.' }
Write-Output ('Dump: ' + $DumpPath)
$symbolDirectory = if ($Architecture -eq 'x64') { '.build\game-x64\GeneralsMD' } else { '.build\community-baseline-ninja\GeneralsMD' }
& $exePath $DumpPath (Join-Path $repositoryPath $symbolDirectory)
if ($LASTEXITCODE -ne 0) { throw "Crash inspector failed: $LASTEXITCODE" }
