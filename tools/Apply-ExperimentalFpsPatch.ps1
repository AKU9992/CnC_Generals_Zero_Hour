[CmdletBinding()]
param([Parameter(Mandatory)][string]$SourcePath)
$ErrorActionPreference = 'Stop'
$repositoryPath = Split-Path $PSScriptRoot -Parent
$expectedPath = Join-Path $repositoryPath '.build\community-reference\GeneralsGameCode-b805c12ee1aedc0a4b241006803b8e04bbf288a6'
$expectedX64Path = Join-Path $repositoryPath '.build\community-reference-x64\GeneralsGameCode-b805c12ee1aedc0a4b241006803b8e04bbf288a6'
if ([IO.Path]::GetFullPath($SourcePath).TrimEnd('\') -notin @([IO.Path]::GetFullPath($expectedPath).TrimEnd('\'), [IO.Path]::GetFullPath($expectedX64Path).TrimEnd('\'))) {
    throw 'Only the pinned reference checkout may be patched.'
}
$enginePath = Join-Path $SourcePath 'GeneralsMD\Code\GameEngine\Source\Common\GameEngine.cpp'
$engineText = [IO.File]::ReadAllText($enginePath)
if ($engineText.Contains('// generals-mods experimental FPS hook')) {
    $fixedText = $engineText.Replace('FramePerformanceLog performanceLog(TheGlobalData->m_userDataDir.str());', 'FramePerformanceLog performanceLog(TheGlobalData->getPath_UserData().str());')
    if ($fixedText -ne $engineText) { [IO.File]::WriteAllText($enginePath, $fixedText) }
    return
}
$newline = if ($engineText.Contains("`r`n")) { "`r`n" } else { "`n" }
$executeMatch = [regex]::Match($engineText, '(?s)void GameEngine::execute\(\)\s*\{.*?(?=/\*\* .*?Factory for the message stream)')
if (-not $executeMatch.Success) { throw 'Unexpected GameEngine::execute layout.' }
$executeText = $executeMatch.Value
$entryAnchor = "void GameEngine::execute()${newline}{"
$loopAnchor = "while( !m_quitting )${newline}`t{"
$updateAnchor = "`t`t`t`t`tupdate();"
$endAnchor = "#endif${newline}${newline}`t}${newline}}"
foreach ($anchor in @($entryAnchor, $loopAnchor, $updateAnchor, $endAnchor)) {
    if ([regex]::Matches($executeText, [regex]::Escape($anchor)).Count -ne 1) { throw 'Reference source anchor changed.' }
}
$entryHook = @'

	// generals-mods experimental FPS hook. Opt-in for this executable only.
	FramePerformanceLog performanceLog(TheGlobalData->getPath_UserData().str());
	char renderFpsValue[16] = {};
	const DWORD renderFpsLength = GetEnvironmentVariableA("GENERALS_RENDER_FPS", renderFpsValue, sizeof(renderFpsValue));
	if (renderFpsLength > 0 && renderFpsLength < sizeof(renderFpsValue))
	{
		char* end = nullptr;
		const long renderFps = strtol(renderFpsValue, &end, 10);
		if (end != renderFpsValue && *end == '\0' && (renderFps == 0 || (renderFps >= 30 && renderFps <= 1000)))
		{
			TheFramePacer->setLogicTimeScaleFps(LOGICFRAMES_PER_SECOND);
			TheFramePacer->enableLogicTimeScale(TRUE);
			TheFramePacer->setFramesPerSecondLimit(renderFps == 0 ? RenderFpsPreset::UncappedFpsValue : (Int)renderFps);
			TheFramePacer->enableFramesPerSecondLimit(renderFps != 0);
			TheWritableGlobalData->m_useFpsLimit = (renderFps != 0);
		}
	}
'@ -replace "`r?`n", $newline
$executeText = $executeText.Replace($entryAnchor, $entryAnchor + $newline + $entryHook)
$executeText = $executeText.Replace($loopAnchor, $loopAnchor + $newline + "`t`tperformanceLog.beginFrame();")
$executeText = $executeText.Replace($updateAnchor, $updateAnchor + $newline + "`t`t`t`t`tperformanceLog.endUpdate();")
$executeText = $executeText.Replace($endAnchor, "#endif${newline}${newline}`t`tperformanceLog.endFrame(TheGameLogic->getFrame(), TheGameLogic->isInGame());${newline}`t}${newline}}")
$engineText = $engineText.Remove($executeMatch.Index, $executeMatch.Length).Insert($executeMatch.Index, $executeText)
$engineText = '#include "Common/FramePerformanceLog.h"' + $newline + $engineText
$profilerPath = Join-Path $repositoryPath 'GeneralsMD\Code\GameEngine\Include\Common\FramePerformanceLog.h'
Copy-Item -LiteralPath $profilerPath -Destination (Join-Path $SourcePath 'Core\GameEngine\Include\Common\FramePerformanceLog.h')
[IO.File]::WriteAllText($enginePath, $engineText)
Write-Output 'Experimental FPS hook applied: opt-in rendering limit with 30 Hz logic and optional CSV profiling.'
