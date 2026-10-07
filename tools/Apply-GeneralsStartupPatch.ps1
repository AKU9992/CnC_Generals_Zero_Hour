[CmdletBinding()]
param([Parameter(Mandatory)][string]$SourcePath)
$ErrorActionPreference='Stop'
$expected=Join-Path (Split-Path $PSScriptRoot -Parent) '.build\community-reference-x64\GeneralsGameCode-b805c12ee1aedc0a4b241006803b8e04bbf288a6'
if([IO.Path]::GetFullPath($SourcePath).TrimEnd('\') -ne [IO.Path]::GetFullPath($expected).TrimEnd('\')){throw 'Only the pinned checkout may be patched.'}
$path=Join-Path $SourcePath 'Core\Libraries\Source\WWVegas\WW3D2\dx8wrapper.cpp'
$text=[IO.File]::ReadAllText($path)
if(-not $text.Contains('GENERALS_BORDERLESS')){
    $anchor='void DX8Wrapper::Resize_And_Position_Window()'+"`n"+'{'
    $addition=@'
    // Opt-in borderless presentation keeps the desktop display mode intact.
    char borderless[8] = {};
    if (IsWindowed && GetEnvironmentVariableA("GENERALS_BORDERLESS", borderless, sizeof(borderless)) && borderless[0] == '1') {
        SetWindowLongPtr(_Hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowLongPtr(_Hwnd, GWL_EXSTYLE, WS_EX_APPWINDOW);
        MONITORINFO monitor = {sizeof(MONITORINFO)};
        GetMonitorInfo(MonitorFromWindow(_Hwnd, MONITOR_DEFAULTTOPRIMARY), &monitor);
        SetWindowPos(_Hwnd, HWND_TOP, monitor.rcMonitor.left, monitor.rcMonitor.top,
            ResolutionWidth, ResolutionHeight, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        return;
    }
'@
    $text=$text.Replace("`r`n","`n")
    if(-not $text.Contains($anchor)){throw 'Window resize anchor missing.'}
    $text=$text.Replace($anchor,$anchor+"`n"+$addition)
    [IO.File]::WriteAllText($path,$text)
}
$path=Join-Path $SourcePath 'Generals\Code\GameEngine\Source\GameClient\GUI\GUICallbacks\Menus\MainMenu.cpp'
$text=[IO.File]::ReadAllText($path)
if(-not $text.Contains('Show the menu without requiring mouse movement')){
    $anchor='layout->bringForward();'+"`n"+"`t// set keyboard focus to main parent"
    $addition=@'
    // Show the menu without requiring mouse movement to end the showcase.
    if (dropDownWindows[DROPDOWN_MAIN]) {
        dropDownWindows[DROPDOWN_MAIN]->winHide(FALSE);
        TheTransitionHandler->setGroup("MainMenuFade", TRUE);
        TheTransitionHandler->setGroup("MainMenuDefaultMenu");
        TheMouse->setVisibility(TRUE);
        notShown = FALSE;
    }
'@
    $text=$text.Replace("`r`n","`n")
    if(-not $text.Contains($anchor)){throw 'Main menu anchor missing.'}
    $text=$text.Replace($anchor,$addition+"`n`t"+$anchor)
    [IO.File]::WriteAllText($path,$text)
}
