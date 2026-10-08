[CmdletBinding()]
param([Parameter(Mandatory)][string]$SourcePath)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$expected=Join-Path $repo '.build/community-reference-x64/GeneralsGameCode-b805c12ee1aedc0a4b241006803b8e04bbf288a6'
if([IO.Path]::GetFullPath($SourcePath) -ne [IO.Path]::GetFullPath($expected)){throw 'Only isolated native sources may be patched.'}
$p=Join-Path $SourcePath 'Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayString.cpp'
$t=[IO.File]::ReadAllText($p)
if(-not $t.Contains('AKU9992 keep unchanged hotkey text resources')){
 $pattern='(void W3DDisplayString::setUseHotkey\(\s*Bool useHotkey,\s*Color hotKeyColor\s*\)\s*\{)'
 if(-not [regex]::IsMatch($t,$pattern)){throw 'Hotkey setter anchor changed.'}
 $replacement=@'
$1
    // AKU9992 keep unchanged hotkey text resources across GUI frames.
    if(m_useHotKey==useHotkey && m_hotKeyColor==hotKeyColor)return;
'@
 $t=[regex]::Replace($t,$pattern,$replacement)
 [IO.File]::WriteAllText($p,$t)
}

foreach($edition in @('Generals','GeneralsMD')){
 $p=Join-Path $SourcePath ($edition+'/Code/GameEngine/Source/Common/GameEngine.cpp')
 $t=[IO.File]::ReadAllText($p)
 $a='            if (nativeGuiProbe) {nativeGuiProbe->hide(FALSE);nativeGuiProbe->bringForward();}'
 $b='            if (nativeGuiProbe) {if(getenv("GENERALS_TEST_GUI_INITIALIZE") && strcmp(getenv("GENERALS_TEST_GUI_INITIALIZE"),"1")==0)nativeGuiProbe->runInit(nullptr);nativeGuiProbe->hide(FALSE);nativeGuiProbe->bringForward();}'
 $t=$t.Replace($a,$b)
 [IO.File]::WriteAllText($p,$t)
}