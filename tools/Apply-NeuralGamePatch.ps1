[CmdletBinding()]
param([Parameter(Mandatory)][string]$SourcePath)
$ErrorActionPreference='Stop'
$repositoryPath=Split-Path $PSScriptRoot -Parent
$expectedPath=Join-Path $repositoryPath '.build\community-reference-x64\GeneralsGameCode-b805c12ee1aedc0a4b241006803b8e04bbf288a6'
if([IO.Path]::GetFullPath($SourcePath).TrimEnd('\') -ne [IO.Path]::GetFullPath($expectedPath).TrimEnd('\')) {throw 'Only the pinned x64 game may be patched.'}
$library=Join-Path $SourcePath 'Core\Libraries\Source\WWVegas\WW3D2'
Copy-Item -LiteralPath (Join-Path $repositoryPath 'renderer\NeuralBridgeClient.h') -Destination (Join-Path $library 'NeuralBridgeClient.h') -Force
$headerPath=Join-Path $SourcePath 'Core\GameEngineDevice\Include\W3DDevice\GameClient\W3DDisplay.h'
$text=[IO.File]::ReadAllText($headerPath)
if(-not $text.Contains('supportsNeuralAA')) {
    $anchor='virtual Bool setDisplayMode( UnsignedInt xres, UnsignedInt yres, UnsignedInt bitdepth, Bool windowed ) override;'
    if([regex]::Matches($text,[regex]::Escape($anchor)).Count -ne 1) {throw 'Unexpected W3D display interface.'}
    $text=$text.Replace($anchor,$anchor+@'

    Bool supportsNeuralAA(NeuralAAMode mode) const override;
    NeuralAAMode getNeuralAAMode() const override;
    Bool setNeuralAAMode(NeuralAAMode mode) override;
'@)
    [IO.File]::WriteAllText($headerPath,$text)
}
$path=Join-Path $SourcePath 'Core\GameEngineDevice\Source\W3DDevice\GameClient\W3DDisplay.cpp'
$text=[IO.File]::ReadAllText($path).Replace("`r`n","`n")
$text=$text.Replace('#include "NeuralBridgeClient.h"', '#include "WW3D2/NeuralBridgeClient.h"')
if(-not $text.Contains('// generals-mods reset neural history while loading')) {
    $anchor='static bool restoredNeuralPreference = false;'
    if($text.Contains($anchor)) {
        $text=$text.Replace($anchor,$anchor+"`n"+'    if (TheGlobalData->m_loadScreenRender) generals_mods::neuralResetHistory(DX8Wrapper::_Get_D3D_Device8()); // generals-mods reset neural history while loading')
    }
}
[IO.File]::WriteAllText($path,$text)
if(-not $text.Contains('// generals-mods game neural frames')) {
    $text=$text.Replace('#include <windows.h>','#include <windows.h>'+"`n"+'#include "WW3D2/NeuralBridgeClient.h"'+"`n"+'#include "Common/OptionPreferences.h"')
    if(-not $text.Contains('#include "WW3D2/NeuralBridgeClient.h"')) {throw 'Unexpected W3D precompiled include.'}
    $anchor='Bool W3DDisplay::setDisplayMode( UnsignedInt xres, UnsignedInt yres, UnsignedInt bitdepth, Bool windowed )'
    $code=@'
// generals-mods game neural frames: the HUD is rendered after reconstruction.
Bool W3DDisplay::supportsNeuralAA(NeuralAAMode mode) const
{
    return mode == NEURAL_AA_OFF || generals_mods::neuralAvailable(DX8Wrapper::_Get_D3D_Device8());
}
Display::NeuralAAMode W3DDisplay::getNeuralAAMode() const
{
    return static_cast<NeuralAAMode>(generals_mods::neuralMode(DX8Wrapper::_Get_D3D_Device8()));
}
Bool W3DDisplay::setNeuralAAMode(NeuralAAMode mode)
{
    if (!supportsNeuralAA(mode)) return FALSE;
    if (mode != NEURAL_AA_OFF && WW3D::Get_MSAA_Mode() != WW3D::MULTISAMPLE_MODE_NONE)
    {
        const auto previous = WW3D::Get_MSAA_Mode();
        WW3D::Set_MSAA_Mode(WW3D::MULTISAMPLE_MODE_NONE);
        if (!setDisplayMode(getWidth(), getHeight(), getBitDepth(), getWindowed()))
        {
            WW3D::Set_MSAA_Mode(previous);
            return FALSE;
        }
    }
    return generals_mods::neuralSetMode(DX8Wrapper::_Get_D3D_Device8(), static_cast<UINT>(mode));
}

'@
    $text=$text.Replace($anchor,$code+"`n"+$anchor)
    $anchor='void W3DDisplay::draw()'
    $pattern=[regex]::Escape($anchor)+'\s*\{'
    if([regex]::Matches($text,$pattern).Count -ne 1) {throw 'Unexpected W3D draw entry.'}
    $code=@'
void W3DDisplay::draw()
{
    static bool restoredNeuralPreference = false;
    if (!restoredNeuralPreference && DX8Wrapper::_Get_D3D_Device8())
    {
        restoredNeuralPreference = true;
        OptionPreferences preferences;
        AsciiString requested = preferences["NeuralAntiAliasing"];
        char overrideMode[32] = {};
        if (GetEnvironmentVariableA("GENERALS_NEURAL_AA", overrideMode, sizeof(overrideMode))) requested = overrideMode;
        const auto mode = requested == "DLAA" ? NEURAL_AA_DLAA : requested == "DLSSQuality" ? NEURAL_AA_DLSS_QUALITY : NEURAL_AA_OFF;
        if (mode != NEURAL_AA_OFF && setNeuralAAMode(mode)) TheWritableGlobalData->m_antiAliasLevel = 0;
    }
    if (TheGlobalData->m_loadScreenRender) generals_mods::neuralResetHistory(DX8Wrapper::_Get_D3D_Device8()); // generals-mods reset neural history while loading
'@
    $text=[regex]::Replace($text,$pattern,[System.Text.RegularExpressions.MatchEvaluator]{param($m) $code})
    $anchor='static Bool couldRender = true;'
    $text=$text.Replace($anchor,$anchor+@'

            const bool neuralScene = !TheGlobalData->m_loadScreenRender && !TheGlobalData->m_disableRender && !TheGlobalData->m_breakTheMovie &&
                DX8Wrapper::_Get_D3D_Device8() && DX8Wrapper::_Get_D3D_Device8()->TestCooperativeLevel() == D3D_OK &&
                generals_mods::neuralBegin(DX8Wrapper::_Get_D3D_Device8(), reinterpret_cast<uintptr_t>(TheTerrainRenderObject));
'@)
    $anchor="drawViews();`n`n`t`t`t`t// draw the user interface"
    if([regex]::Matches($text,[regex]::Escape($anchor)).Count -ne 1) {throw 'Unexpected world/HUD boundary.'}
    $text=$text.Replace($anchor,"drawViews();`n                if (neuralScene) generals_mods::neuralEnd(DX8Wrapper::_Get_D3D_Device8());`n`n                // draw the user interface")
    $anchor='if (couldRender)'
    $text=$text.Replace($anchor,'if (neuralScene) generals_mods::neuralAbort(DX8Wrapper::_Get_D3D_Device8());'+"`n"+'                '+$anchor)
    [IO.File]::WriteAllText($path,$text)
}
$path=Join-Path $library 'dx8renderer.cpp'
$text=[IO.File]::ReadAllText($path).Replace("`r`n","`n")
if(-not $text.Contains('generals_mods::neuralObject')) {
    $text='#include "NeuralBridgeClient.h"'+"`n"+$text
    $anchor='SNAPSHOT_SAY(("mesh = %s",mesh->Get_Name()));'
    if([regex]::Matches($text,[regex]::Escape($anchor)).Count -ne 1) {throw 'Unexpected mesh draw identity.'}
    $text=$text.Replace($anchor,$anchor+"`n"+'        generals_mods::neuralObject(DX8Wrapper::_Get_D3D_Device8(), reinterpret_cast<uintptr_t>(mesh));')
    $anchor='PolyRenderTaskClass * next_prt = prt->Get_Next_Visible();'
    if([regex]::Matches($text,[regex]::Escape($anchor)).Count -ne 1) {throw 'Unexpected mesh draw end.'}
    $text=$text.Replace($anchor,'generals_mods::neuralObject(DX8Wrapper::_Get_D3D_Device8(), 0);'+"`n"+'        '+$anchor)
    [IO.File]::WriteAllText($path,$text)
}
$path=Join-Path $library 'dx8wrapper.cpp'
$text=[IO.File]::ReadAllText($path)
if(-not $text.Contains('// generals-mods apply MSAA changes on reset')) {
    $anchor='DX8CALL_HRES(Reset(&_PresentParameters),hr)'
    if([regex]::Matches($text,[regex]::Escape($anchor)).Count -ne 1) {throw 'Unexpected device reset parameters.'}
    $text=$text.Replace($anchor,'_PresentParameters.MultiSampleType = MultiSampleAntiAliasing; // generals-mods apply MSAA changes on reset'+"`n"+'            '+$anchor)
    [IO.File]::WriteAllText($path,$text)
}
