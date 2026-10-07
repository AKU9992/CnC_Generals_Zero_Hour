[CmdletBinding()]
param([Parameter(Mandatory)][string]$SourcePath, [ValidateSet('Generals','GeneralsMD')][string]$GameEdition='GeneralsMD')
$editionSourcePath = Join-Path $SourcePath $GameEdition
$ErrorActionPreference = 'Stop'
$repositoryPath = Split-Path $PSScriptRoot -Parent
$expectedPath = Join-Path $repositoryPath '.build\community-reference-x64\GeneralsGameCode-b805c12ee1aedc0a4b241006803b8e04bbf288a6'
if ([IO.Path]::GetFullPath($SourcePath).TrimEnd('\') -ne [IO.Path]::GetFullPath($expectedPath).TrimEnd('\')) { throw 'Only the pinned x64 checkout may be patched.' }
$displayPath = Join-Path $SourcePath 'Core\GameEngine\Include\GameClient\Display.h'
$display = [IO.File]::ReadAllText($displayPath)
if (-not $display.Contains('supportsNeuralAA')) {
    $anchor = 'virtual ~Display() override;'
    if ([regex]::Matches($display, [regex]::Escape($anchor)).Count -ne 1) { throw 'Unexpected display interface.' }
    $display = $display.Replace($anchor, $anchor + @'

    enum NeuralAAMode { NEURAL_AA_OFF, NEURAL_AA_DLSS_QUALITY, NEURAL_AA_DLAA };
    // Availability requires an active temporal renderer, beyond GPU support.
    virtual Bool supportsNeuralAA(NeuralAAMode) const { return FALSE; }
    virtual NeuralAAMode getNeuralAAMode() const { return NEURAL_AA_OFF; }
    virtual Bool setNeuralAAMode(NeuralAAMode mode) { return mode == NEURAL_AA_OFF; }
'@)
    [IO.File]::WriteAllText($displayPath, $display)
}
$menuPath = Join-Path $editionSourcePath 'Code\GameEngine\Source\GameClient\GUI\GUICallbacks\Menus\OptionsMenu.cpp'
$menu = [IO.File]::ReadAllText($menuPath)
$menu = $menu.Replace('elseif (controlID', 'else if (controlID')
$menu = $menu.Replace('L"NVIDIA DLSS Quality"', 'L"DLSS Quality"').Replace('L"NVIDIA DLAA"', 'L"DLAA"')
[IO.File]::WriteAllText($menuPath,$menu)
if ($menu.Contains('// generals-mods neural AA selection') -or $menu.Contains('#include "NeuralAaToggle.inc"')) { return }
$anchor = 'static GameWindow *   comboBoxAntiAliasing     = nullptr;'
if (-not $menu.Contains($anchor)) { throw 'Unexpected AA control declaration.' }
$menu = $menu.Replace($anchor, $anchor + @'

static Int lastAntiAliasingChoice = 0;
static Display::NeuralAAMode neuralModeForChoice(Int choice)
{
    return choice == OptionPreferences::AntiAliasingMode_Count
        ? Display::NEURAL_AA_DLSS_QUALITY : Display::NEURAL_AA_DLAA;
}
'@)
$match = [regex]::Match($menu, '(?s)// antialiasing\s*GadgetComboBoxGetSelectedPos.*?(?=\s*//-+\s*// texture filter mode)')
if (-not $match.Success) { throw 'Unexpected AA save code.' }
$code = @'
// antialiasing
    // generals-mods neural AA selection: DLSS Quality and DLAA are distinct modes.
    GadgetComboBoxGetSelectedPos(comboBoxAntiAliasing, &index);
    if (index >= OptionPreferences::AntiAliasingMode_Count && index < OptionPreferences::AntiAliasingMode_Count + 2)
    {
        const Display::NeuralAAMode mode = neuralModeForChoice(index);
        if (TheDisplay->supportsNeuralAA(mode) && TheDisplay->setNeuralAAMode(mode))
        {
            TheWritableGlobalData->m_antiAliasLevel = WW3D::MULTISAMPLE_MODE_NONE;
            (*pref)["AntiAliasing"] = "0";
            (*pref)["NeuralAntiAliasing"] = mode == Display::NEURAL_AA_DLAA ? "DLAA" : "DLSSQuality";
        }
        else
        {
            index = lastAntiAliasingChoice;
            GadgetComboBoxSetSelectedPos(comboBoxAntiAliasing, index);
        }
    }
    if (index >= 0 && index < OptionPreferences::AntiAliasingMode_Count && TheDisplay->setNeuralAAMode(Display::NEURAL_AA_OFF))
    {
        Int mode = index > 0 ? 1 << index : 0;
        TheWritableGlobalData->m_antiAliasLevel = mode;
        AsciiString value; value.format("%d", mode);
        (*pref)["AntiAliasing"] = value;
        (*pref)["NeuralAntiAliasing"] = "Off";
    }
'@
$menu = $menu.Remove($match.Index, $match.Length).Insert($match.Index, $code)
$anchor = 'Int val = atoi(selectedAliasingMode.str());'
if ([regex]::Matches($menu, [regex]::Escape($anchor)).Count -ne 1) { throw 'Unexpected AA population code.' }
$menu = $menu.Replace($anchor, @'
    for (Int neuralIndex = 0; neuralIndex < 2; ++neuralIndex)
    {
        const Display::NeuralAAMode mode = neuralModeForChoice(OptionPreferences::AntiAliasingMode_Count + neuralIndex);
        const Bool available = TheDisplay->supportsNeuralAA(mode);
        const wchar_t* label = neuralIndex == 0
            ? (available ? L"DLSS Quality" : L"NVIDIA DLSS Quality (unavailable)")
            : (available ? L"DLAA" : L"NVIDIA DLAA (unavailable)");
        GadgetComboBoxAddEntry(comboBoxAntiAliasing, UnicodeString(label), available ? color : GameMakeColor(128,128,128,255));
    }

'@ + "`n" + $anchor)
$anchor = 'GadgetComboBoxSetSelectedPos(comboBoxAntiAliasing, pos);'
$menu = $menu.Replace($anchor, @'
    const Display::NeuralAAMode activeMode = TheDisplay->getNeuralAAMode();
    if (activeMode != Display::NEURAL_AA_OFF && TheDisplay->supportsNeuralAA(activeMode))
        pos = OptionPreferences::AntiAliasingMode_Count + (activeMode == Display::NEURAL_AA_DLAA ? 1 : 0);
    lastAntiAliasingChoice = pos;
'@ + "`n" + $anchor)
$anchor = 'if (controlID == comboBoxDetailID)'
if ([regex]::Matches($menu, [regex]::Escape($anchor)).Count -ne 1) { throw 'Unexpected options event handler.' }
$menu = $menu.Replace($anchor, @'
if (controlID == comboBoxAntiAliasingID)
                {
                    Int choice = -1;
                    GadgetComboBoxGetSelectedPos(comboBoxAntiAliasing, &choice);
                    if (choice >= OptionPreferences::AntiAliasingMode_Count && choice < OptionPreferences::AntiAliasingMode_Count + 2 && !TheDisplay->supportsNeuralAA(neuralModeForChoice(choice)))
                        GadgetComboBoxSetSelectedPos(comboBoxAntiAliasing, lastAntiAliasingChoice);
                    else if (choice >= 0 && choice < OptionPreferences::AntiAliasingMode_Count + 2)
                        lastAntiAliasingChoice = choice;
                }
                else
'@ + "`n" + $anchor)
[IO.File]::WriteAllText($menuPath, $menu)
Write-Output 'Added separate DLSS Quality and DLAA choices with renderer availability checks.'
