[CmdletBinding()]
param([switch]$BuildOnly)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$output=Join-Path $repo '.build/audio-tests'
$null=New-Item -ItemType Directory -Path $output -Force
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$include=Join-Path $repo '.build/community-reference-x64/GeneralsGameCode-b805c12ee1aedc0a4b241006803b8e04bbf288a6/Dependencies/Miles'
$exe=Join-Path $output 'X64AudioTests.exe'
$cmd=Join-Path $output 'build.cmd'
[IO.File]::WriteAllLines($cmd,@('@echo off',('call "'+$vs+'\VC\Auxiliary\Build\vcvars64.bat"'),'if errorlevel 1 exit /b %errorlevel%',('cl /nologo /EHsc /std:c++17 /I"'+$include+'" "'+$PSScriptRoot+'\native\X64AudioTests.cpp" /Fo"'+$output+'\tests.obj" /Fe"'+$exe+'"'),'exit /b %errorlevel%'),[Text.Encoding]::Default)
& cmd.exe /d /c $cmd
if($LASTEXITCODE -ne 0){throw 'Native audio test compilation failed.'}
if(-not $BuildOnly){Push-Location $output;try{& $exe 'E:\C&C ZH GPTMOD\CaCGZH\Data\Audio\Tracks\USA_01.mp3';if($LASTEXITCODE -ne 0){throw 'Native audio test failed.'}}finally{Pop-Location}}
