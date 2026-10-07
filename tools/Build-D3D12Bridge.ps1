[CmdletBinding()]
param([ValidateSet('x86','x64')][string]$Architecture = 'x86')
$ErrorActionPreference = 'Stop'
$repositoryPath = Split-Path $PSScriptRoot -Parent
$commit = '255338f698c8270b537f0a91a13f795f4f988250'
$referenceRoot = if ($Architecture -eq 'x64') { '.build\d3d8to9-reference-x64' } else { '.build\d3d8to9-reference' }
$sourcePath = Join-Path $repositoryPath ($referenceRoot + '\d3d8to9-' + $commit)
$archivePath = Join-Path $repositoryPath '.build\d3d8to9.zip'
if (-not (Test-Path -LiteralPath (Join-Path $sourcePath 'CMakeLists.txt'))) {
    if (-not (Test-Path -LiteralPath $archivePath)) { Invoke-WebRequest ('https://codeload.github.com/crosire/d3d8to9/zip/' + $commit) -OutFile $archivePath }
    Expand-Archive -LiteralPath $archivePath -DestinationPath (Split-Path $sourcePath -Parent) -Force
}
Copy-Item -LiteralPath (Join-Path $repositoryPath 'renderer\D3D9On12Bridge.h') -Destination (Join-Path $sourcePath 'source\D3D9On12Bridge.h') -Force
$patches = @(
    @{ File = 'source\d3d8to9.cpp'; Old = 'IDirect3D9 *const d3d = Direct3DCreate9(D3D_SDK_VERSION);'; New = 'IDirect3D9 *const d3d = createD3D9On12();' },
    @{ File = 'source\d3d8to9.cpp'; Old = 'LoadLibrary(TEXT("d3dx9_43.dll"))'; New = 'LoadLibraryExW(L"d3dx9_43.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)' },
    @{ File = 'source\d3d8to9_base.cpp'; Old = '*ppReturnedDeviceInterface = new Direct3DDevice8('; New = "const HRESULT nativeResult = validateD3D12Device(DeviceInterface);`n`tif (FAILED(nativeResult)) { DeviceInterface->Release(); return nativeResult; }`n`t*ppReturnedDeviceInterface = new Direct3DDevice8(" },
    @{ File = 'source\d3d8to9_device.cpp'; Old = 'return ProxyInterface->Present(pSourceRect, pDestRect, hDestWindowOverride, nullptr);'; New = 'return bridgePresent(ProxyInterface, pSourceRect, pDestRect, hDestWindowOverride);' },
    @{ File = 'CMakeLists.txt'; Old = 'OUTPUT_NAME "d3d8"'; New = 'OUTPUT_NAME "generals-d3d12"' }
)
foreach ($patch in $patches) {
    $path = Join-Path $sourcePath $patch.File
    $content = [IO.File]::ReadAllText($path)
    if (-not $content.Contains($patch.New)) {
        if ([regex]::Matches($content, [regex]::Escape($patch.Old)).Count -ne 1) { throw ('Unexpected bridge source: ' + $patch.File) }
        $content = $content.Replace($patch.Old, $patch.New)
    }
    if ($patch.File.EndsWith('.cpp') -and -not $content.Contains('#include "D3D9On12Bridge.h"')) {
        $content = '#include "D3D9On12Bridge.h"' + "`n" + $content
    }
    [IO.File]::WriteAllText($path, $content)
}
$drawPath = Join-Path $sourcePath 'source\d3d8to9_device.cpp'
$drawContent = [IO.File]::ReadAllText($drawPath)
if (-not $drawContent.Contains('return recordBridgeDraw(')) {
    $drawPattern = 'ProxyInterface->(Draw(?:Indexed)?Primitive(?:UP)?\([^;]+\));\s*return D3D_OK;'
    if ([regex]::Matches($drawContent, $drawPattern).Count -ne 4) { throw 'Unexpected primitive draw layout.' }
    $drawContent = [regex]::Replace($drawContent, $drawPattern, 'return recordBridgeDraw(ProxyInterface->$1);')
    [IO.File]::WriteAllText($drawPath, $drawContent)
}
if ($Architecture -eq 'x64') { & (Join-Path $PSScriptRoot 'Apply-X64BridgePatch.ps1') -SourcePath $sourcePath }
if ($Architecture -eq 'x64') { & (Join-Path $PSScriptRoot 'Apply-NeuralBridgePatch.ps1') -SourcePath $sourcePath }
$outputPath = Join-Path $repositoryPath $(if ($Architecture -eq 'x64') { '.build\d3d12-bridge-x64' } else { '.build\d3d12-bridge' })
$null = New-Item -ItemType Directory -Path $outputPath -Force
$cachePath = Join-Path $outputPath 'CMakeCache.txt'
if (Test-Path -LiteralPath $cachePath) {
    $cache = [IO.File]::ReadAllText($cachePath)
    $cachedSource = [regex]::Match($cache, '(?m)^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$').Groups[1].Value.Trim()
    if ($cachedSource -and [IO.Path]::GetFullPath($cachedSource) -ne [IO.Path]::GetFullPath($sourcePath)) {
        Remove-Item -LiteralPath $cachePath
    }
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$cmake = Join-Path $installation 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ninja = Join-Path $installation 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
$commandPath = Join-Path $outputPath 'build.cmd'
$environmentScript = Join-Path $installation $(if ($Architecture -eq 'x64') { 'VC\Auxiliary\Build\vcvars64.bat' } else { 'VC\Auxiliary\Build\vcvars32.bat' })
[IO.File]::WriteAllLines($commandPath, @('@echo off', ('call "' + $environmentScript + '"'), 'if errorlevel 1 exit /b %errorlevel%',
    ('"' + $cmake + '" -S "' + $sourcePath + '" -B "' + $outputPath + '" -G Ninja -DCMAKE_MAKE_PROGRAM="' + $ninja + '" -DCMAKE_BUILD_TYPE=Release'), 'if errorlevel 1 exit /b %errorlevel%',
    ('"' + $cmake + '" --build "' + $outputPath + '" --clean-first --parallel 4'), 'exit /b %errorlevel%'), [Text.Encoding]::Default)
& cmd.exe /d /c $commandPath
if ($LASTEXITCODE -ne 0) { throw 'D3D12 bridge build failed.' }
Copy-Item -LiteralPath (Join-Path $sourcePath 'LICENSE.md') -Destination (Join-Path $outputPath 'd3d8to9-LICENSE.md') -Force
Write-Output ('Built separate ' + $Architecture + ' D3D8 -> D3D9On12 -> D3D12 bridge.' + $(if($Architecture -eq 'x64') {' Includes the DLSS/DLAA temporal renderer.'} else {' Neural antialiasing requires x64.'}))
