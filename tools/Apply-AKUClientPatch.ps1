[CmdletBinding()]
param([Parameter(Mandatory)][string]$SourcePath)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$expected=Join-Path $root '.build/community-reference-x64/GeneralsGameCode-b805c12ee1aedc0a4b241006803b8e04bbf288a6'
if([IO.Path]::GetFullPath($SourcePath) -ne [IO.Path]::GetFullPath($expected)){throw 'Only isolated build sources may be patched.'}
$p=Join-Path $SourcePath 'Core/GameEngine/Source/Common/version.cpp'
$t=[IO.File]::ReadAllText($p)
$t=$t.Replace('m_buildUser = user;','m_buildUser = "AKU9992";').Replace('m_buildLocation = location;','m_buildLocation = AsciiString::TheEmptyString;')
$t=$t.Replace('m_buildUser = AsciiString::TheEmptyString;','m_buildUser = "AKU9992";')
foreach($type in @('AsciiString','UnicodeString')){
 $prefix=if($type -eq 'AsciiString'){'Ascii'}else{'Unicode'}
 $literal=if($type -eq 'AsciiString'){'"AKU9992"'}else{'L"AKU9992"'}
 foreach($suffix in @('BuildUser','BuildUserOrGitCommitAuthorName')){
  $name='get'+$prefix+$suffix
  $t=[regex]::Replace($t,'(?ms)('+$type+' Version::'+$name+'\([^)]*\)\s*(?:const)?\s*\{).*?^}','$1'+[Environment]::NewLine+'    return '+$literal+';'+[Environment]::NewLine+'}')
 }
 $name='get'+$prefix+'BuildLocation'
 $t=[regex]::Replace($t,'(?ms)('+$type+' Version::'+$name+'\([^)]*\)\s*(?:const)?\s*\{).*?^}','$1'+[Environment]::NewLine+'    return '+$type+'::TheEmptyString;'+[Environment]::NewLine+'}')
}
foreach($name in @('getUnicodeProductTitle','getUnicodeProductAuthor','getUnicodeProductString','getUnicodeProductVersionHashString')){
 $t=[regex]::Replace($t,'(?ms)(UnicodeString Version::'+$name+'\([^)]*\)\s*(?:const)?\s*\{).*?^}','$1'+[Environment]::NewLine+'    return L"AKU9992";'+[Environment]::NewLine+'}')
}
$t=[regex]::Replace($t,'(?ms)(const char\* Version::getGitCommitAuthorName\([^)]*\)\s*\{).*?^}','$1'+[Environment]::NewLine+'    return "AKU9992";'+[Environment]::NewLine+'}')
[IO.File]::WriteAllText($p,$t)
$p=Join-Path $SourcePath 'resources/gitinfo/gitinfo.cpp.in'
$t=[IO.File]::ReadAllText($p)
$t=[regex]::Replace($t,'(GitCommitAuthorName\s*(?:\[\])?\s*=\s*)[^;]+;','$1"AKU9992";')
[IO.File]::WriteAllText($p,$t)
$p=Join-Path $SourcePath 'Core/GameEngine/Source/GameClient/MessageStream/LookAtXlat.cpp'
$t=[IO.File]::ReadAllText($p)
if(-not $t.Contains('AKU9992 native window edge scrolling')){
 $lines=@('m_screenEdgeScrollMode = prefs.getScreenEdgeScrollMode();','    // AKU9992 native window edge scrolling.','    const char* renderer = getenv("GENERALS_RENDERER");','    if (renderer && strcmp(renderer,"native12")==0)','        m_screenEdgeScrollMode |= ScreenEdgeScrollMode_EnabledInWindowedApp;')
 $t=$t.Replace('m_screenEdgeScrollMode = prefs.getScreenEdgeScrollMode();',($lines -join [Environment]::NewLine))
 $t='#include <cstdlib>'+[Environment]::NewLine+'#include <cstring>'+[Environment]::NewLine+$t
 [IO.File]::WriteAllText($p,$t)
}
$p=Join-Path $SourcePath 'Core/GameEngine/Source/GameClient/Input/Mouse.cpp'
$t=[IO.File]::ReadAllText($p)
if(-not $t.Contains('AKU9992 native game cursor capture')){
 $lines=@('m_cursorCaptureMode = prefs.getCursorCaptureMode();','    // AKU9992 native game cursor capture; focus blockers still apply.','    const char* renderer = getenv("GENERALS_RENDERER");','    if (renderer && strcmp(renderer,"native12")==0)','        m_cursorCaptureMode |= CursorCaptureMode_EnabledInWindowedGame;')
 $t=$t.Replace('m_cursorCaptureMode = prefs.getCursorCaptureMode();',($lines -join [Environment]::NewLine))
 $t='#include <cstdlib>'+[Environment]::NewLine+'#include <cstring>'+[Environment]::NewLine+$t
 [IO.File]::WriteAllText($p,$t)
}
# Runtime diagnostics and default LAN identity never use account/host names.
foreach($relative in @('Core/Libraries/Source/debug/debug_io_flat.cpp','Core/Libraries/Source/debug/debug_io_net.cpp','Core/GameEngine/Source/GameNetwork/LANAPI.cpp')){
 $p=Join-Path $SourcePath $relative
 $t=[IO.File]::ReadAllText($p)
 $t=[regex]::Replace($t,'Get(?:Computer|User)NameA?\(\s*(\w+)\s*,\s*&(\w+)\s*\)','($2=8,strcpy($1,"AKU9992"),TRUE)')
 if(-not $t.Contains('#include <cstring>')){$t='#include <cstring>'+[Environment]::NewLine+$t}
 [IO.File]::WriteAllText($p,$t)
}
$p=Join-Path $SourcePath 'Core/GameEngine/Source/GameClient/Input/Mouse.cpp'
$t=[IO.File]::ReadAllText($p)
if(-not $t.Contains('GENERALS_TEST_CAPTURED')){
 $a='Bool Mouse::isCursorCaptured()'
 $index=$t.IndexOf($a)
 $brace=$t.IndexOf('{',$index)
 $probe=[Environment]::NewLine+'    if (getenv("GENERALS_TEST_QUIT_SECONDS") && getenv("GENERALS_TEST_EDGE_SCROLL")) {'+[Environment]::NewLine+'        const char* value=getenv("GENERALS_TEST_CAPTURED");'+[Environment]::NewLine+'        if (value) return value[0]==''1'';'+[Environment]::NewLine+'    }'
 $t=$t.Insert($brace+1,$probe)
 [IO.File]::WriteAllText($p,$t)
}# Reproducible test probe, enabled only by the isolated test environment.
foreach($edition in @('Generals','GeneralsMD')) {
 $p=Join-Path $SourcePath ($edition+'/Code/GameEngine/Source/Common/GameEngine.cpp')
 $t=[IO.File]::ReadAllText($p)
 if(-not $t.Contains('akuEdgeProbeDone')) {
  $probe=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'native/AKUClientEdgeProbe.inc'))
  $t=$t.Replace('performanceLog.beginFrame();',$probe+[Environment]::NewLine+'performanceLog.beginFrame();')
  $t='#include "Common/OptionPreferences.h"'+[Environment]::NewLine+$t
  foreach($header in @('LookAtXlat','View','InGameUI','Display','Mouse')) {$t='#include "GameClient/'+$header+'.h"'+[Environment]::NewLine+$t}
  [IO.File]::WriteAllText($p,$t)
 }
}
# Descriptions shown by Windows also use the single release identity.
foreach($edition in @('Generals','GeneralsMD')) {
 $p=Join-Path $SourcePath ($edition+'/Code/Main/RTS.RC')
 $t=[IO.File]::ReadAllText($p)
 if(-not $t.Contains('AKU9992_VERSION')) {
  $t += [Environment]::NewLine+[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'native/AKUVersion.rc.inc'))
  [IO.File]::WriteAllText($p,$t)
 }
}

foreach($edition in @('Generals','GeneralsMD')){
 $p=Join-Path $SourcePath ($edition+'/Code/Main/RTS.RC');$t=[IO.File]::ReadAllText($p).Replace('1,0,0,3','1,0,0,4').Replace('1.0.0.3','1.0.0.4');[IO.File]::WriteAllText($p,$t)
}