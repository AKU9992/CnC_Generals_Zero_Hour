[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$source=Join-Path $root '.build\community-reference-x64\GeneralsGameCode-b805c12ee1aedc0a4b241006803b8e04bbf288a6\Core\Libraries\Source\WWVegas'
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$command=Join-Path $root '.build\object-pool-test.cmd'
$exe=Join-Path $root '.build\ObjectPoolX64Tests.exe'
$includes=@(($source+'\WWLib'),$source,(Join-Path $source '..\..\..\..\Dependencies\Utility'),(Join-Path $source '..\..\..\..\Dependencies\Precompiled'),(Join-Path $source '..\..\Include')) | ForEach-Object {'/I"'+$_+'"'}
[IO.File]::WriteAllLines($command,@('@echo off',('call "'+$vs+'\VC\Auxiliary\Build\vcvars64.bat"'),('cl /nologo /EHsc /std:c++20 /Zc:__cplusplus /DNDEBUG '+($includes -join ' ')+' "'+$PSScriptRoot+'\native\ObjectPoolX64Tests.cpp" /Fo"'+$root+'\.build\ObjectPoolX64Tests.obj" /Fe"'+$exe+'"'),'exit /b %errorlevel%'))
& cmd.exe /d /c $command
if($LASTEXITCODE -ne 0) {throw 'Object pool test compilation failed.'}
& $exe
if($LASTEXITCODE -ne 0) {throw 'Object pool cleanup regression failed.'}
