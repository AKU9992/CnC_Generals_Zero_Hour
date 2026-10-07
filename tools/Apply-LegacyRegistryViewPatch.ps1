[CmdletBinding()]
param([Parameter(Mandatory)][string]$SourcePath)
$ErrorActionPreference = 'Stop'
$expected = Join-Path (Split-Path $PSScriptRoot -Parent) '.build\community-reference-x64\GeneralsGameCode-b805c12ee1aedc0a4b241006803b8e04bbf288a6'
if ([IO.Path]::GetFullPath($SourcePath) -ne [IO.Path]::GetFullPath($expected)) { throw 'Only the isolated x64 source may be patched.' }
foreach ($edition in @('Generals','GeneralsMD')) {
    $path = Join-Path $SourcePath ($edition + '\Code\GameEngine\Source\Common\System\registry.cpp')
    $text = [IO.File]::ReadAllText($path)
    if ($text.Contains('// generals-mods legacy EA registry view')) { continue }
    $helper = @'
// generals-mods legacy EA registry view: retail installers remain 32-bit.
static REGSAM legacyEARegistryAccess(const AsciiString& path, REGSAM access)
{
#if defined(_WIN64)
    const char prefix[] = "SOFTWARE\\Electronic Arts\\";
    if (_strnicmp(path.str(), prefix, sizeof(prefix) - 1) == 0)
        access |= KEY_WOW64_32KEY;
#else
    (void)path;
#endif
    return access;
}

'@
    $text = $text.Replace('Bool  getStringFromRegistry(', $helper + 'Bool  getStringFromRegistry(')
    $text = $text.Replace('0, KEY_READ, &handle', '0, legacyEARegistryAccess(path, KEY_READ), &handle')
    $text = $text.Replace('REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr', 'REG_OPTION_NON_VOLATILE, legacyEARegistryAccess(path, KEY_WRITE), nullptr')
    [IO.File]::WriteAllText($path,$text)
}
