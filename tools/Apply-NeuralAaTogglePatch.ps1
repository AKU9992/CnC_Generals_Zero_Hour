[CmdletBinding()]
param([Parameter(Mandatory)][string]$SourcePath,[ValidateSet('Generals','GeneralsMD')][string]$GameEdition='GeneralsMD')
$ErrorActionPreference='Stop'
$repositoryPath=Split-Path $PSScriptRoot -Parent
$expected=Join-Path $repositoryPath '.build\community-reference-x64\GeneralsGameCode-b805c12ee1aedc0a4b241006803b8e04bbf288a6'
if([IO.Path]::GetFullPath($SourcePath).TrimEnd('\') -ne [IO.Path]::GetFullPath($expected).TrimEnd('\')){throw 'Only the pinned checkout may be patched.'}
$directory=Join-Path $SourcePath ($GameEdition+'\Code\GameEngine\Source\GameClient\GUI\GUICallbacks\Menus')
Copy-Item -LiteralPath (Join-Path $repositoryPath 'ui\NeuralAaToggle.inc') -Destination $directory -Force
Copy-Item -LiteralPath (Join-Path $repositoryPath 'ui\NeuralAaToggleTest.inc') -Destination $directory -Force
$path=Join-Path $directory 'OptionsMenu.cpp'
$text=[IO.File]::ReadAllText($path).Replace("`r`n","`n")
if(-not $text.Contains('#include "NeuralAaToggle.inc"')){
    $text=$text.Replace('#include "GameClient/Gadget.h"','#include "GameClient/Gadget.h"'+"`n"+'#include "GameClient/GadgetPushButton.h"')
    $anchor='static NameKeyType    comboBoxResolutionID'
    if(-not $text.Contains($anchor)){throw 'Missing declaration anchor.'}
    $text=$text.Replace($anchor,'#include "NeuralAaToggle.inc"'+"`n`n"+$anchor)
    $pattern='(?s)// antialiasing\s*// generals-mods neural AA selection:.*?(?=\s*//-+\s*// texture filter mode)'
    if([regex]::Matches($text,$pattern).Count -ne 1){throw 'Missing AA save anchor.'}
    $text=[regex]::Replace($text,$pattern,'// antialiasing'+"`n"+'    applyNeuralSelection(*pref);')
    $anchor='GadgetComboBoxSetSelectedPos(comboBoxAntiAliasing, pos);'
    $text=$text.Replace($anchor,$anchor+"`n"+'    createNeuralToggle((*pref)["NeuralAntiAliasingLastMode"]);')
    $anchor='lastAntiAliasingChoice = choice;'+"`n"+'                }'
    if(-not $text.Contains($anchor)){throw 'Missing dropdown event anchor.'}
    $text=$text.Replace($anchor,'lastAntiAliasingChoice = choice;'+"`n"+'                    updateNeuralToggle();'+"`n"+'                }')
    $anchor='if( controlID == buttonBack )'
    $text=$text.Replace($anchor,'if (controlID == TheNameKeyGenerator->nameToKey("OptionsMenu.wnd:ButtonNeuralToggle"))'+"`n"+'            { toggleNeuralSelection(); }'+"`n"+'            else '+$anchor)
    $anchor='void OptionsMenuShutdown( WindowLayout *layout, void *userData )'+"`n"+'{'
    $text=$text.Replace($anchor,$anchor+"`n"+'    neuralToggleButton = nullptr;')
}
[IO.File]::WriteAllText($path,$text)
if(-not $text.Contains('#include "NeuralAaToggleTest.inc"')) {
    [IO.File]::AppendAllText($path,"`n"+'#include "NeuralAaToggleTest.inc"'+"`n")
}
$path=Join-Path $SourcePath ($GameEdition+'\Code\GameEngine\Source\GameClient\GameClient.cpp')
$text=[IO.File]::ReadAllText($path)
if(-not $text.Contains('generalsTestNeuralToggleTick();')){
    $anchor='TheShell->UPDATE();'
    if(-not $text.Contains($anchor)){throw 'Missing game client update anchor.'}
    $text=$text.Replace($anchor,'extern void generalsTestNeuralToggleTick();'+"`n"+'        generalsTestNeuralToggleTick();'+"`n        "+$anchor)
    [IO.File]::WriteAllText($path,$text)
}
