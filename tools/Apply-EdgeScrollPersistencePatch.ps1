[CmdletBinding()]
param([Parameter(Mandatory)][string]$SourcePath)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$expected=Join-Path $repo '.build/community-reference-x64/GeneralsGameCode-b805c12ee1aedc0a4b241006803b8e04bbf288a6'
if([IO.Path]::GetFullPath($SourcePath) -ne [IO.Path]::GetFullPath($expected)){throw 'Only isolated native sources may be patched.'}
$p=Join-Path $SourcePath 'Core/GameEngine/Source/Common/OptionPreferences.cpp'
$t=[IO.File]::ReadAllText($p)
foreach($name in @('getCursorCaptureEnabledInWindowedGame','getCursorCaptureEnabledInFullscreenGame','getScreenEdgeScrollEnabledInWindowedApp','getScreenEdgeScrollEnabledInFullscreenApp')){
 $marker='AKU9992 persistent native input '+$name
 if(-not $t.Contains($marker)){
  $pattern='(Bool OptionPreferences::'+$name+'\(\) const\s*\{)'
  if(-not [regex]::IsMatch($t,$pattern)){throw ('Input preference anchor changed: '+$name)}
  $replacement='$1'+[Environment]::NewLine+'    // '+$marker+[Environment]::NewLine+'    const char* renderer=getenv("GENERALS_RENDERER");'+[Environment]::NewLine+'    if(renderer && strcmp(renderer,"native12")==0)return TRUE;'
  $t=[regex]::Replace($t,$pattern,$replacement)
 }
}
if(-not $t.Contains('#include <cstdlib>')){$t='#include <cstdlib>'+[Environment]::NewLine+'#include <cstring>'+[Environment]::NewLine+$t}
[IO.File]::WriteAllText($p,$t)
foreach($entry in @(@('Core/GameEngine/Source/GameClient/Input/Mouse.cpp','void Mouse::setCursorCaptureMode','CursorCaptureMode','CursorCaptureMode_EnabledInWindowedGame | CursorCaptureMode_EnabledInFullscreenGame'),@('Core/GameEngine/Source/GameClient/MessageStream/LookAtXlat.cpp','void LookAtTranslator::setScreenEdgeScrollMode','ScreenEdgeScrollMode','ScreenEdgeScrollMode_EnabledInWindowedApp | ScreenEdgeScrollMode_EnabledInFullscreenApp'))){
 $p=Join-Path $SourcePath $entry[0];$t=[IO.File]::ReadAllText($p)
 $marker='AKU9992 normalize native input on every application'
 if(-not $t.Contains($marker)){
  $pattern='('+[regex]::Escape($entry[1])+'\('+ $entry[2]+' mode\)\s*\{)'
  if(-not [regex]::IsMatch($t,$pattern)){throw 'Input setter anchor changed.'}
  $replacement='$1'+[Environment]::NewLine+'    // '+$marker+[Environment]::NewLine+'    const char* renderer=getenv("GENERALS_RENDERER");'+[Environment]::NewLine+'    if(renderer && strcmp(renderer,"native12")==0)mode |= '+$entry[3]+';'
  $t=[regex]::Replace($t,$pattern,$replacement)
  [IO.File]::WriteAllText($p,$t)
 }
}

# Upgrade the earlier probe without removing neighboring gameplay hooks.
foreach($edition in @('Generals','GeneralsMD')){
 $p=Join-Path $SourcePath ($edition+'/Code/GameEngine/Source/Common/GameEngine.cpp')
 $t=[IO.File]::ReadAllText($p)
 if($t.Contains('akuEdgeProbeDone') -and -not $t.Contains('edges("legacy-settings")')){
  $pattern='(?s)        static bool akuEdgeProbeDone=false;.*?TheInGameUI->setInputEnabled\(input\);\s*\}'
  if(-not [regex]::IsMatch($t,$pattern)){throw 'Previous edge probe anchor changed.'}
  $probe=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'native/AKUClientEdgeProbe.inc'))
  $t=[regex]::Replace($t,$pattern,$probe)
  foreach($header in @('Common/OptionPreferences.h','GameClient/Mouse.h')){if(-not $t.Contains('#include "'+$header+'"')){$t='#include "'+$header+'"'+[Environment]::NewLine+$t}}
  [IO.File]::WriteAllText($p,$t)
 }
}
