[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installationPath = & $vswherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installationPath) { throw 'No MSVC x86/x64 toolset found.' }
$environmentScript = Join-Path $installationPath 'VC\Auxiliary\Build\vcvars32.bat'
$repositoryPath = Split-Path $PSScriptRoot -Parent
$codePath = Join-Path $repositoryPath 'GeneralsMD\Code'
$outputDirectory = Join-Path $repositoryPath '.build\legacy-probe'
$null = New-Item -ItemType Directory -Path $outputDirectory -Force
$projectText = [IO.File]::ReadAllText((Join-Path $codePath 'RTS.dsp'))
$cppLine = [regex]::Match($projectText, '(?m)^# ADD CPP .*').Value
if (-not $cppLine) { throw 'No compiler configuration found in RTS.dsp.' }
$includeFlags = foreach ($includeMatch in [regex]::Matches($cppLine, '/I "([^"]+)"')) {
    '/I"' + (Join-Path $codePath $includeMatch.Groups[1].Value) + '"'
}
$commandPath = Join-Path $outputDirectory 'probe.cmd'
$command = 'cl.exe /nologo /c /EHsc /W3 /DWIN32 /D_WINDOWS /D_MBCS /DNDEBUG /D_RELEASE /D_CRT_SECURE_NO_WARNINGS ' + ($includeFlags -join ' ') + ' "' + (Join-Path $codePath 'Main\WinMain.cpp') + '" /Fo"' + $outputDirectory + '\WinMain.obj"'
[IO.File]::WriteAllLines($commandPath, @('@echo off', ('call "' + $environmentScript + '"'), 'if errorlevel 1 exit /b %errorlevel%', $command, 'exit /b %errorlevel%'), [Text.Encoding]::Default)
& cmd.exe /d /c $commandPath
if ($LASTEXITCODE -ne 0) {
    throw 'Legacy source probe failed. This is a compile-only diagnostic, not a complete game build.'
}
Write-Output 'The Zero Hour entry point compiled; full game link and dependency checks are still required.'
