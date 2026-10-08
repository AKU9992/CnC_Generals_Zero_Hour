[CmdletBinding()]
param([switch]$Test, [switch]$DebugLayer, [switch]$Neural)
$ErrorActionPreference='Stop'
$repositoryPath=Split-Path $PSScriptRoot -Parent
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if(-not (Test-Path -LiteralPath $vswhere)){throw 'MSVC Build Tools must be installed first.'}
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $installation){throw 'A complete MSVC AMD64 installation is required.'}
$cmake=Join-Path $installation 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ninja=Join-Path $installation 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
$build=Join-Path $repositoryPath '.build\native12'
$null=New-Item -ItemType Directory -Path $build -Force
$command=Join-Path $build 'build.cmd'
$sdkInclude=''
if($Neural){
    $sdkInclude=Join-Path $repositoryPath '.build\streamline-sdk-v2.14.1\include'
    if(-not (Test-Path -LiteralPath (Join-Path $sdkInclude 'sl.h'))){throw 'Run Get-StreamlineSdk.ps1 first.'}
}
[IO.File]::WriteAllLines($command,@(
    '@echo off',
    ('call "'+$installation+'\VC\Auxiliary\Build\vcvars64.bat"'),
    'if errorlevel 1 exit /b %errorlevel%',
    ('"'+$cmake+'" -S "'+$repositoryPath+'\renderer\native12" -B "'+$build+'" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="'+$ninja+'" -DSTREAMLINE_INCLUDE_DIR="'+$sdkInclude+'"'),
    'if errorlevel 1 exit /b %errorlevel%',
    ('"'+$cmake+'" --build "'+$build+'"'),
    'exit /b %errorlevel%'
),[Text.Encoding]::Default)
& cmd.exe /d /c $command
if($LASTEXITCODE -ne 0){throw 'Native D3D12 renderer build failed.'}
if($Neural){& (Join-Path $PSScriptRoot 'Copy-StreamlineRuntime.ps1') -ExecutableDirectory $build}
if($Test){
    $testNames=@('NativeRenderer12Tests')
    if($Neural){$testNames+=@('NativeDlss12Tests','NativeScene12Tests')}
    $results=@()
    Push-Location -LiteralPath $build
    try {
        foreach($name in $testNames){
            $testArguments=@()
            if($DebugLayer -and $name -eq 'NativeRenderer12Tests'){$testArguments+='--debug'}
            $exe=Join-Path $build ($name+'.exe')
            $clock=[Diagnostics.Stopwatch]::StartNew()
            & $exe @testArguments 2>&1 | Tee-Object -FilePath (Join-Path $build ($name+'.log'))
            $code=$LASTEXITCODE
            $clock.Stop()
            $results+=@{name=$name;exitCode=$code;elapsedSeconds=$clock.Elapsed.TotalSeconds;sha256=(Get-FileHash -LiteralPath $exe).Hash}
            @{tests=$results;neural=$Neural.IsPresent;debugLayer=$DebugLayer.IsPresent;gameIntegration=$false} |
                ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $build 'validation.json') -Encoding utf8
            if($code -ne 0){throw ($name+' GPU validation failed.')}
        }
    } finally {
        Pop-Location
    }
}
