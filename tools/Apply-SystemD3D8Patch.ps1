[CmdletBinding()]
param([Parameter(Mandatory)][string]$SourcePath)
$ErrorActionPreference = 'Stop'
$repositoryPath = Split-Path $PSScriptRoot -Parent
$expectedPath = Join-Path $repositoryPath '.build\community-reference\GeneralsGameCode-b805c12ee1aedc0a4b241006803b8e04bbf288a6'
$expectedX64Path = Join-Path $repositoryPath '.build\community-reference-x64\GeneralsGameCode-b805c12ee1aedc0a4b241006803b8e04bbf288a6'
if ([IO.Path]::GetFullPath($SourcePath).TrimEnd('\') -notin @([IO.Path]::GetFullPath($expectedPath).TrimEnd('\'), [IO.Path]::GetFullPath($expectedX64Path).TrimEnd('\'))) { throw 'Only the pinned reference checkout may be patched.' }
$wrapperDirectory = Join-Path $SourcePath 'Core\Libraries\Source\WWVegas\WW3D2'
$wrapperPath = Join-Path $wrapperDirectory 'dx8wrapper.cpp'
$wrapperText = [IO.File]::ReadAllText($wrapperPath)
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'native\SystemDirect3D8.h') -Destination (Join-Path $wrapperDirectory 'SystemDirect3D8.h') -Force
if (-not $wrapperText.Contains('loadSystemDirect3D8()')) {
    $loadAnchor = 'D3D8Lib = LoadLibrary("D3D8.DLL");'
    if ([regex]::Matches($wrapperText, [regex]::Escape($loadAnchor)).Count -ne 1) { throw 'Unexpected Direct3D8 loader layout.' }
    $wrapperText = $wrapperText.Replace($loadAnchor, 'D3D8Lib = loadSystemDirect3D8();')
    $wrapperText = '#include "SystemDirect3D8.h"' + "`r`n" + $wrapperText
    [IO.File]::WriteAllText($wrapperPath, $wrapperText)
}
Write-Output 'Experimental renderer loads the Windows Direct3D8 DLL by absolute system path.'
