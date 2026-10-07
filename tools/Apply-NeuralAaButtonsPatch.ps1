[CmdletBinding()]
param([Parameter(Mandatory)][string]$SourcePath)
$ErrorActionPreference = 'Stop'
$repositoryPath = Split-Path $PSScriptRoot -Parent
$expectedPath = Join-Path $repositoryPath '.build\community-reference-x64\GeneralsGameCode-b805c12ee1aedc0a4b241006803b8e04bbf288a6'
if ([IO.Path]::GetFullPath($SourcePath).TrimEnd('\') -ne [IO.Path]::GetFullPath($expectedPath).TrimEnd('\')) { throw 'Only the pinned x64 menu may be patched.' }
$path = Join-Path $SourcePath 'GeneralsMD\Code\GameEngine\Source\GameClient\GUI\GUICallbacks\Menus\OptionsMenu.cpp'
$text = [IO.File]::ReadAllText($path)
# Migrate existing builds back to the full-width AA dropdown.
$text = [regex]::Replace($text, '(?s)// generals-mods separate neural AA buttons.*?(?=static Display::NeuralAAMode)', '')
$text = [regex]::Replace($text, '\s*createNeuralAAButtons\(\);', '')
$text = [regex]::Replace($text, '(?s)if \(controlID == TheNameKeyGenerator->nameToKey\("OptionsMenu.wnd:ButtonDLSS"\).*?else\s+(?=if \(controlID == comboBoxAntiAliasingID\))', '')
if ($text.Contains('ButtonDLSS') -or $text.Contains('ButtonDLAA')) { throw 'Unexpected obsolete AA button code.' }
[IO.File]::WriteAllText($path, $text)
