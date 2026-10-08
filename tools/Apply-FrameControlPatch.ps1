[CmdletBinding()]
param([Parameter(Mandatory)][string]$SourcePath)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$expected=Join-Path $repo '.build/community-reference-x64/GeneralsGameCode-b805c12ee1aedc0a4b241006803b8e04bbf288a6'
if([IO.Path]::GetFullPath($SourcePath) -ne [IO.Path]::GetFullPath($expected)){throw 'Only isolated native sources may be patched.'}
foreach($name in @('FramePerformanceLog.h','AkuCpuPhases.h')){Copy-Item (Join-Path $repo ('GeneralsMD/Code/GameEngine/Include/Common/'+$name)) (Join-Path $SourcePath ('Core/GameEngine/Include/Common/'+$name)) -Force}
$p=Join-Path $SourcePath 'Core/GameEngine/Source/Common/FramePacer.cpp';$t=[IO.File]::ReadAllText($p)
if(-not $t.Contains('generalsNativeWaitFrame')){
 $a='m_updateTime = m_frameRateLimit.wait(maxFps);'
 $b=@'
    typedef double(__cdecl* NativeWaitFn)();
    static NativeWaitFn nativeWait=reinterpret_cast<NativeWaitFn>(GetProcAddress(GetModuleHandleW(L"generals-native12.dll"),"generalsNativeWaitFrame"));
    m_updateTime = nativeWait ? static_cast<Real>(nativeWait()) : m_frameRateLimit.wait(maxFps);
'@
 if(-not $t.Contains($a)){throw 'Frame pacer wait anchor changed.'};$t=$t.Replace($a,$b)
 $t=$t.Replace('m_logicFramePhase = timeScale;','m_logicFramePhase = min(1.0f,timeScale);')
 $t=$t.Replace('return min(1.0f, (Real)getActualLogicTimeScaleFps(flags) / getUpdateFps());','const Int fps=getActualLogicTimeScaleFps(flags);if(fps>=RenderFpsPreset::UncappedFpsValue)return 1.0f;return GetModuleHandleW(L"generals-native12.dll") ? (Real)fps/getUpdateFps() : min(1.0f,(Real)fps/getUpdateFps());')
 $a='m_frameRateLimit.reset();'
 $b=@'
    typedef void(__cdecl* ResetFn)();
    static ResetFn nativeReset=reinterpret_cast<ResetFn>(GetProcAddress(GetModuleHandleW(L"generals-native12.dll"),"generalsNativeResetFrameClock"));
    if(nativeReset)nativeReset();
    m_frameRateLimit.reset();
'@
 $t=$t.Replace($a,$b);[IO.File]::WriteAllText($p,$t)
}
foreach($edition in @('Generals','GeneralsMD')){
 $p=Join-Path $SourcePath ($edition+'/Code/GameEngine/Source/Common/GameEngine.cpp');$t=[IO.File]::ReadAllText($p)
 if(-not $t.Contains('AKU9992 fixed simulation catchup')){
  $t=$t.Replace('const Int maxRenderFps = TheFramePacer->getActualFramesPerSecondLimit();','const Int maxRenderFps = GetModuleHandleW(L"generals-native12.dll") ? RenderFpsPreset::UncappedFpsValue : TheFramePacer->getActualFramesPerSecondLimit();')
  $t=$t.Replace('m_logicTimeAccumulator += min(TheFramePacer->getUpdateTime(), targetFrameTime);','m_logicTimeAccumulator += TheFramePacer->getUpdateTime();')
  $pattern='(?s)if \(canUpdateGameLogic\(FramePacer::IgnoreFrozenTime\)\)\s*\{\s*TheGameLogic->UPDATE\(\);\s*if \(!TheFramePacer->isTimeFrozen\(\)\)\s*\{\s*TheGameClient->step\(\);\s*\}\s*\}'
  if(-not [regex]::IsMatch($t,$pattern)){throw 'Logic update anchor changed.'}
  $block=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'native/AKUFixedLogic.inc'))
  $t=[regex]::Replace($t,$pattern,$block)
  $a='FramePerformanceLog performanceLog(TheGlobalData->getPath_UserData().str());'
  $t=$t.Replace($a,$a+[Environment]::NewLine+'    if(GetModuleHandleW(L"generals-native12.dll")){TheFramePacer->setLogicTimeScaleFps(LOGICFRAMES_PER_SECOND);TheFramePacer->enableLogicTimeScale(TRUE);TheFramePacer->enableFramesPerSecondLimit(FALSE);TheWritableGlobalData->m_useFpsLimit=FALSE;TheFramePacer->reset();}')
  foreach($entry in @(@('TheAudio->UPDATE();','AkuAudio'),@('TheGameClient->UPDATE();','AkuClient'),@('TheMessageStream->propagateMessages();','AkuMessages'),@('TheNetwork->UPDATE();','AkuNetwork'))){$t=$t.Replace($entry[0],'akuProfileCall('+$entry[1]+',[&]{'+$entry[0]+'});')}
  $t='#include "Common/AkuCpuPhases.h"'+[Environment]::NewLine+$t
  [IO.File]::WriteAllText($p,$t)
 }
 $p=Join-Path $SourcePath ($edition+'/Code/GameEngine/Source/GameClient/GameClient.cpp');$t=[IO.File]::ReadAllText($p)
 if(-not $t.Contains('AKU9992 particles follow simulation ticks')){
  $t=$t.Replace('if( !freezeTime && TheGameLogic->hasUpdated() )','if( !freezeTime && TheGameLogic->hasUpdated() && !GetModuleHandleW(L"generals-native12.dll") )')
  $pattern='(void GameClient::step\(\)\s*\{)'
  $block=@'
$1
    // AKU9992 particles follow simulation ticks, including catchup.
    if(GetModuleHandleW(L"generals-native12.dll")) {
        TheParticleSystemManager->setLocalPlayerIndex(rts::getObservedOrLocalPlayer()->getPlayerIndex());
        TheParticleSystemManager->UPDATE();
    }
'@
  $t=[regex]::Replace($t,$pattern,$block)
  $t=$t.Replace('TheDisplay->DRAW();','akuProfileCall(AkuDraw,[&]{TheDisplay->DRAW();});')
  $t=$t.Replace('Drawable* draw = firstDrawable();','AkuCpuScope drawablesScope(AkuDrawables);'+[Environment]::NewLine+'        Drawable* draw = firstDrawable();')
  $t='#include "Common/AkuCpuPhases.h"'+[Environment]::NewLine+$t
  [IO.File]::WriteAllText($p,$t)
 }
 $p=Join-Path $SourcePath ($edition+'/Code/GameEngine/Source/GameLogic/System/GameLogic.cpp');$t=[IO.File]::ReadAllText($p)
 if(-not $t.Contains('akuProfileCall(AkuAI')){$t=$t.Replace('TheAI->UPDATE();','akuProfileCall(AkuAI,[&]{TheAI->UPDATE();});');$t='#include "Common/AkuCpuPhases.h"'+[Environment]::NewLine+$t;[IO.File]::WriteAllText($p,$t)}
}

# Repair include placement after the engine declarations.
foreach($edition in @('Generals','GeneralsMD')){
 $p=Join-Path $SourcePath ($edition+'/Code/GameEngine/Source/Common/GameEngine.cpp');$v=[IO.File]::ReadAllText($p)
 if($v.StartsWith('#include "Common/AKUGameplayProbe.h"')){$v=$v.Substring($v.IndexOf("`n")+1);$v=$v.Replace('Bool GameEngine::canUpdateGameLogic','#include "Common/AKUGameplayProbe.h"'+[Environment]::NewLine+'Bool GameEngine::canUpdateGameLogic');[IO.File]::WriteAllText($p,$v)}
}
# Isolated, opt-in real skirmish/CRC probes; no input injection into other programs.
$p=Join-Path $SourcePath 'Core/GameEngine/Include/Common/AKUGameplayProbe.h'
Copy-Item (Join-Path $PSScriptRoot 'native/AKUGameplayProbe.inc') $p -Force
foreach($edition in @('Generals','GeneralsMD')){
 $p=Join-Path $SourcePath ($edition+'/Code/GameEngine/Source/Common/GameEngine.cpp');$t=[IO.File]::ReadAllText($p)
 if(-not $t.Contains('akuGameplayProbe().start')){
  $t=$t.Replace('Bool GameEngine::canUpdateGameLogic', '#include "Common/AKUGameplayProbe.h"'+[Environment]::NewLine+'Bool GameEngine::canUpdateGameLogic')
  $t=$t.Replace('        performanceLog.beginFrame();','        akuGameplayProbe().start(timeGetTime()-quitTestStart);'+[Environment]::NewLine+'        performanceLog.beginFrame();')
  $t=$t.Replace('akuProfileCall(AkuLogic,[&]{TheGameLogic->UPDATE();});','akuProfileCall(AkuLogic,[&]{TheGameLogic->UPDATE();});'+[Environment]::NewLine+'                akuGameplayProbe().step();')
  $t=$t.Replace('quitSeconds <= 60','quitSeconds <= 86400')
  [IO.File]::WriteAllText($p,$t)
 }
}
# A legacy game-speed slider controls simulation speed, never presentation FPS.
$p=Join-Path $SourcePath 'Core/GameEngine/Source/GameLogic/System/GameLogicDispatch.cpp';$t=[IO.File]::ReadAllText($p)
if(-not $t.Contains('AKU9992 game speed is independent')){
 $a='TheFramePacer->setFramesPerSecondLimit(maxFPS);'
 $b='// AKU9992 game speed is independent of the display target.'+[Environment]::NewLine+'        if(GetModuleHandleW(L"generals-native12.dll")){TheFramePacer->setLogicTimeScaleFps(maxFPS);TheFramePacer->enableLogicTimeScale(TRUE);}else '+$a
 $t=$t.Replace($a,$b);[IO.File]::WriteAllText($p,$t)
}
foreach($edition in @('Generals','GeneralsMD')){
 $p=Join-Path $SourcePath ($edition+'/Code/GameEngine/Source/Common/GameEngine.cpp');$t=[IO.File]::ReadAllText($p);$t=$t.Replace('if(!canUpdateGameLogic(FramePacer::IgnoreFrozenTime))break;','if(!canUpdateNetworkGameLogic())break;'+[Environment]::NewLine+'                    TheGameLogic->preUpdate();');[IO.File]::WriteAllText($p,$t)
}
foreach($edition in @('Generals','GeneralsMD')){
 $p=Join-Path $SourcePath ($edition+'/Code/GameEngine/Source/Common/GameEngine.cpp');$t=[IO.File]::ReadAllText($p);if(-not $t.Contains('AKU9992 refresh network readiness per tick')){$t=$t.Replace('if(!canUpdateNetworkGameLogic())break;','// AKU9992 refresh network readiness per tick.'+[Environment]::NewLine+'                    akuProfileCall(AkuNetwork,[&]{TheNetwork->UPDATE();});'+[Environment]::NewLine+'                    if(!canUpdateNetworkGameLogic())break;');[IO.File]::WriteAllText($p,$t)}
}