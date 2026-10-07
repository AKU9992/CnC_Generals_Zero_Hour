[CmdletBinding()]
param([Parameter(Mandatory)][string]$SourcePath)
$ErrorActionPreference = 'Stop'
$repositoryPath = Split-Path $PSScriptRoot -Parent
$expectedPath = Join-Path $repositoryPath '.build\community-reference\GeneralsGameCode-b805c12ee1aedc0a4b241006803b8e04bbf288a6'
if ([IO.Path]::GetFullPath($SourcePath).TrimEnd('\') -ne [IO.Path]::GetFullPath($expectedPath).TrimEnd('\')) { throw 'Only the pinned reference checkout may be patched.' }
$displayPath = Join-Path $SourcePath 'Core\GameEngine\Include\GameClient\Display.h'
$displayText = [IO.File]::ReadAllText($displayPath)
if (-not $displayText.Contains('supportsDlaa()')) {
    $anchor = 'virtual ~Display() override;'
    if ([regex]::Matches($displayText, [regex]::Escape($anchor)).Count -ne 1) { throw 'Unexpected display interface.' }
    $displayText = $displayText.Replace($anchor, $anchor + @'

	// DLAA is available only when an active renderer supplies real temporal inputs.
	// W3D/DX8 inherits the unavailable defaults. Hardware capability alone is insufficient.
	virtual Bool supportsDlaa() const { return FALSE; }
	virtual Bool isDlaaEnabled() const { return FALSE; }
	virtual Bool setDlaaEnabled(Bool enabled) { return !enabled; }
'@)
    [IO.File]::WriteAllText($displayPath, $displayText)
}
$menuPath = Join-Path $SourcePath 'GeneralsMD\Code\GameEngine\Source\GameClient\GUI\GUICallbacks\Menus\OptionsMenu.cpp'
$menuText = [IO.File]::ReadAllText($menuPath)
if ($menuText.Contains('// generals-mods DLAA selection')) { return }
$newline = if ($menuText.Contains("`r`n")) { "`r`n" } else { "`n" }
$staticAnchor = 'static GameWindow *   comboBoxAntiAliasing     = nullptr;'
$menuText = $menuText.Replace($staticAnchor, $staticAnchor + $newline + 'static Int lastAntiAliasingChoice = 0;')
$saveMatch = [regex]::Match($menuText, '(?s)// antialiasing\s*GadgetComboBoxGetSelectedPos.*?(?=\s*//-+\s*// texture filter mode)')
if (-not $saveMatch.Success) { throw 'Unexpected antialiasing save code.' }
$saveCode = @'
// antialiasing
    // generals-mods DLAA selection. Separate preference; never encode DLAA as MSAA.
    GadgetComboBoxGetSelectedPos(comboBoxAntiAliasing, &index);
    if (index == OptionPreferences::AntiAliasingMode_Count)
    {
        if (TheDisplay->supportsDlaa() && TheDisplay->setDlaaEnabled(TRUE))
        {
            TheWritableGlobalData->m_antiAliasLevel = WW3D::MULTISAMPLE_MODE_NONE;
            (*pref)["AntiAliasing"] = "0";
            (*pref)["NeuralAntiAliasing"] = "DLAA";
        }
        else
        {
            // Preserve the user's last conventional AA choice if unavailable.
            index = lastAntiAliasingChoice;
            GadgetComboBoxSetSelectedPos(comboBoxAntiAliasing, index);
        }
    }
    if (index >= 0 && index < OptionPreferences::AntiAliasingMode_Count)
    {
        if (TheDisplay->setDlaaEnabled(FALSE))
        {
            Int mode = (index > 0) ? 1 << index : 0;
            TheWritableGlobalData->m_antiAliasLevel = mode;
            AsciiString prefString;
            prefString.format("%d", mode);
            (*pref)["AntiAliasing"] = prefString;
            (*pref)["NeuralAntiAliasing"] = "Off";
        }
    }
'@ -replace "`r?`n", $newline
$menuText = $menuText.Remove($saveMatch.Index, $saveMatch.Length).Insert($saveMatch.Index, $saveCode)
$populateAnchor = 'Int val = atoi(selectedAliasingMode.str());'
if ([regex]::Matches($menuText, [regex]::Escape($populateAnchor)).Count -ne 1) { throw 'Unexpected antialiasing population code.' }
$populateCode = @'
    const Bool dlaaAvailable = TheDisplay->supportsDlaa();
    UnicodeString dlaaLabel = TheGameText->FETCH_OR_SUBSTITUTE(
        dlaaAvailable ? "GUI:NVIDIA_DLAA" : "GUI:NVIDIA_DLAA_Unavailable",
        dlaaAvailable ? L"NVIDIA DLAA" : L"NVIDIA DLAA (unavailable)");
    GadgetComboBoxAddEntry(comboBoxAntiAliasing, dlaaLabel,
        dlaaAvailable ? color : GameMakeColor(128, 128, 128, 255));

'@ -replace "`r?`n", $newline
$menuText = $menuText.Replace($populateAnchor, $populateCode + $populateAnchor)
$selectionAnchor = 'GadgetComboBoxSetSelectedPos(comboBoxAntiAliasing, pos);'
$menuText = $menuText.Replace($selectionAnchor, 'lastAntiAliasingChoice = pos;' + $newline +
    '    if (TheDisplay->supportsDlaa() && TheDisplay->isDlaaEnabled()) pos = OptionPreferences::AntiAliasingMode_Count;' + $newline + $selectionAnchor)
$eventAnchor = 'if (controlID == comboBoxDetailID)'
if ([regex]::Matches($menuText, [regex]::Escape($eventAnchor)).Count -ne 1) { throw 'Unexpected options event handler.' }
$eventCode = @'
if (controlID == comboBoxAntiAliasingID)
                {
                    Int choice = -1;
                    GadgetComboBoxGetSelectedPos(comboBoxAntiAliasing, &choice);
                    if (choice == OptionPreferences::AntiAliasingMode_Count && !TheDisplay->supportsDlaa())
                        GadgetComboBoxSetSelectedPos(comboBoxAntiAliasing, lastAntiAliasingChoice);
                    else if (choice >= 0 && choice < OptionPreferences::AntiAliasingMode_Count)
                        lastAntiAliasingChoice = choice;
                }
                else
'@ -replace "`r?`n", $newline
$menuText = $menuText.Replace($eventAnchor, $eventCode + $eventAnchor)
[IO.File]::WriteAllText($menuPath, $menuText)
Write-Output 'DLAA menu integration prepared; DX8 displays an unavailable entry and preserves MSAA.'
