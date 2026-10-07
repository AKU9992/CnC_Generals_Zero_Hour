[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installationPath = & $vswherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installationPath) { throw 'No MSVC x86/x64 toolset found.' }
$repositoryPath = Split-Path $PSScriptRoot -Parent
foreach ($gameName in @('Generals', 'GeneralsMD')) {
    foreach ($architecture in @('x86', 'x64')) {
        $outputDirectory = Join-Path $repositoryPath ('.build\timing-tests\' + $gameName + '-' + $architecture + '-' + [Guid]::NewGuid().ToString('N'))
        $null = New-Item -ItemType Directory -Path $outputDirectory
        $includePath = Join-Path $repositoryPath ($gameName + '\Code\GameEngine\Include')
        $environmentScript = Join-Path $installationPath ('VC\Auxiliary\Build\vcvars' + $(if ($architecture -eq 'x86') { '32' } else { '64' }) + '.bat')
        $sourcePath = Join-Path $PSScriptRoot 'native\FramePerformanceLogTests.cpp'
        $exePath = Join-Path $outputDirectory 'FramePerformanceLogTests.exe'
        $commandPath = Join-Path $outputDirectory 'build.cmd'
        $commands = @('@echo off', ('call "' + $environmentScript + '"'), 'if errorlevel 1 exit /b %errorlevel%',
            ('cl.exe /nologo /W4 /WX /D_CRT_SECURE_NO_WARNINGS /EHsc /std:c++17 /O2 /I"' + $includePath + '" "' + $sourcePath + '" /Fo"' + $outputDirectory + '\log.obj" /Fe"' + $exePath + '"'), 'exit /b %errorlevel%')
        [IO.File]::WriteAllLines($commandPath, $commands, [Text.Encoding]::Default)
        & cmd.exe /d /c $commandPath
        if ($LASTEXITCODE -ne 0) { throw "Profiler compile failed: $gameName / $architecture." }
        & $exePath $outputDirectory
        if ($LASTEXITCODE -ne 0) { throw "Profiler test failed: $gameName / $architecture, code $LASTEXITCODE." }

        # Compile the actual source expression, checking undefined/high-rate boundary cases.
        $engineSource = [IO.File]::ReadAllText((Join-Path $repositoryPath ($gameName + '\Code\GameEngine\Source\Common\GameEngine.cpp')))
        $intervalMatch = [regex]::Match($engineSource, '(?s)DWORD limit = (\(m_maxFPS > 0.*?);')
        if (-not $intervalMatch.Success) { throw 'Cannot locate the production frame-limit expression.' }
        $guardSourcePath = Join-Path $outputDirectory 'FrameLimitTests.cpp'
        $guardSource = '#include <windows.h>' + "`n" + 'DWORD interval(int m_maxFPS) { return ' + $intervalMatch.Groups[1].Value + '; }' + "`n" + @'
int main() {
    const int fps[] = { -1, 0, 1, 30, 60, 500, 1000, 2000 };
    const DWORD expected[] = { 0, 0, 999, 32, 15, 1, 0, 0 };
    for (unsigned i = 0; i < sizeof(fps)/sizeof(fps[0]); ++i)
        if (interval(fps[i]) != expected[i]) return 1;
    return 0;
}
'@
        [IO.File]::WriteAllText($guardSourcePath, $guardSource, [Text.Encoding]::ASCII)
        $guardExePath = Join-Path $outputDirectory 'FrameLimitTests.exe'
        $commands[3] = 'cl.exe /nologo /W4 /WX /EHsc /O2 "' + $guardSourcePath + '" /Fo"' + $outputDirectory + '\guard.obj" /Fe"' + $guardExePath + '"'
        [IO.File]::WriteAllLines($commandPath, $commands, [Text.Encoding]::Default)
        & cmd.exe /d /c $commandPath
        if ($LASTEXITCODE -ne 0) { throw 'Frame-limit guard compile failed.' }
        & $guardExePath
        if ($LASTEXITCODE -ne 0) { throw 'Frame-limit boundary test failed.' }
        Write-Output "PASS: frame-limit boundaries ($gameName / $architecture)."
    }
}
