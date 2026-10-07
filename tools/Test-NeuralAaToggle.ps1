[CmdletBinding()]
param([string]$GameDirectory='E:\C&C ZH GPTMOD\CaCG')
$ErrorActionPreference='Stop'
$repositoryPath=Split-Path $PSScriptRoot -Parent
$optionsPath=Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Command and Conquer Generals Data\Options.ini'
$original=[IO.File]::ReadAllBytes($optionsPath)
$originalHash=(Get-FileHash -LiteralPath $optionsPath).Hash
$logPath=Join-Path $GameDirectory 'GeneralsNeuralToggleTest.log'
$beforeLength=if(Test-Path -LiteralPath $logPath){[IO.File]::ReadAllText($logPath).Length}else{0}
$previousTest=$env:GENERALS_TEST_NEURAL_TOGGLE
try {
    $env:GENERALS_TEST_NEURAL_TOGGLE='1'
    & (Join-Path $PSScriptRoot 'Test-X64GameStartup.ps1') -Edition Generals -GameDirectory $GameDirectory -ShellMap -Graceful -RequireAudio -Seconds 25 -Width 3440 -Height 1440
    $probe=Get-Content -LiteralPath (Join-Path $repositoryPath '.build\generals-x64-game-startup.json') -Raw|ConvertFrom-Json
    $log=[IO.File]::ReadAllText($logPath)
    $log=$log.Substring($beforeLength)
    $pidLog=($log -split "`n"|Where-Object{$_.StartsWith("PID $($probe.processId):")}) -join "`n"
    $pidLog | Set-Content -LiteralPath (Join-Path $repositoryPath '.build\neural-aa-toggle-test.log') -Encoding UTF8
    if($pidLog.Contains('FAIL:') -or -not $pidLog.Contains('PASS: toggle integration completed')){throw ('Neural AA menu toggle test failed: '+$pidLog)}
    foreach($check in @('standard MSAA restored','reopened menu restores standard AA and remembered DLSS','dropdown open above settings panels')) {
        if(-not $pidLog.Contains('PASS: '+$check)){throw ('Missing menu regression check: '+$check)}
    }
    if(-not $probe.renderedFrameVerified -or -not $probe.nativeOutputReadbackVerified){throw 'The menu output frame was not verified.'}
    $menuFrame=Join-Path $repositoryPath '.build\neural-aa-toggle-menu.bmp'
    Copy-Item -LiteralPath $probe.renderedFramePath -Destination $menuFrame -Force
    [ordered]@{executableSha256=$probe.executableSha256;bridgeSha256=$probe.bridgeSha256;toggleCallbacksVerified=$true;standardAaRestoredVerified=$true;dropdownOverlayVerified=$true;dlaaOffOnVerified=$true;dlssOffOnVerified=$true;preferencePersistenceVerified=$true;reopenedMenuVerified=$true;audioVerified=$probe.audioOutputVerified;gracefulExitVerified=$probe.gracefulExitVerified;menuFrame=$menuFrame;log=$pidLog} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $repositoryPath '.build\neural-aa-toggle-validation.json') -Encoding UTF8
    Write-Output $pidLog
} finally {
    $env:GENERALS_TEST_NEURAL_TOGGLE=$previousTest
    [IO.File]::WriteAllBytes($optionsPath,$original)
    if((Get-FileHash -LiteralPath $optionsPath).Hash -ne $originalHash){throw 'Options.ini was not restored exactly.'}
}
