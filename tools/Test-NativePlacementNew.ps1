[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repositoryPath = Split-Path $PSScriptRoot -Parent
$vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installationPath = & $vswherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installationPath) { throw 'MSVC tools are required.' }
foreach ($gameName in @('Generals', 'GeneralsMD')) {
    $headerPath = Join-Path $repositoryPath ($gameName + '\Code\GameEngine\Include\Common\GameMemory.h')
    $headerText = [IO.File]::ReadAllText($headerPath)
    $declarationBlock = [regex]::Match($headerText, '(?s)#ifndef _OPERATOR_NEW_DEFINED_.*?(?=#ifdef MEMORYPOOL_DEBUG_CUSTOM_NEW)')
    if (-not $declarationBlock.Success) { throw 'Cannot find placement-new declaration block.' }
    foreach ($architecture in @('x86', 'x64')) {
        $outputPath = Join-Path $repositoryPath ('.build\placement-tests\' + $gameName + '-' + $architecture)
        $null = New-Item -ItemType Directory -Path $outputPath -Force
        $testSourcePath = Join-Path $outputPath 'PlacementNew.cpp'
        $testExePath = Join-Path $outputPath 'PlacementNew.exe'
        $testSource = "#include <new.h>`n" + $declarationBlock.Value + @'

int main() {
    alignas(int) unsigned char storage[sizeof(int) * 2];
    int* values = new (storage) int[2];
    values[0] = 17; values[1] = 29;
    return reinterpret_cast<void*>(values) != storage || values[0] != 17 || values[1] != 29;
}
'@
        [IO.File]::WriteAllText($testSourcePath, $testSource, [Text.Encoding]::ASCII)
        $environmentScript = Join-Path $installationPath ('VC\Auxiliary\Build\vcvars' + $(if ($architecture -eq 'x86') { '32' } else { '64' }) + '.bat')
        $commandPath = Join-Path $outputPath 'build.cmd'
        [IO.File]::WriteAllLines($commandPath, @('@echo off', ('call "' + $environmentScript + '"'), 'if errorlevel 1 exit /b %errorlevel%', ('cl.exe /nologo /W4 /WX /EHsc /std:c++17 "' + $testSourcePath + '" /Fo"' + $outputPath + '\test.obj" /Fe"' + $testExePath + '"'), 'exit /b %errorlevel%'), [Text.Encoding]::Default)
        & cmd.exe /d /c $commandPath
        if ($LASTEXITCODE -ne 0) { throw "Placement-new compilation failed: $gameName / $architecture" }
        & $testExePath
        if ($LASTEXITCODE -ne 0) { throw "Placement-new behavior failed: $gameName / $architecture" }
        Write-Output "PASS: $gameName / $architecture placement array allocation"
    }
}
