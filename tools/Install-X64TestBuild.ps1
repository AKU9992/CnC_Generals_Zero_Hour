[CmdletBinding(SupportsShouldProcess)]
param([string]$GameDirectory = 'E:\C&C ZH GPTMOD\CaCGZH')
$ErrorActionPreference = 'Stop'
$repositoryPath = Split-Path $PSScriptRoot -Parent
$sourceExe = Join-Path $repositoryPath '.build\game-x64\GeneralsMD\generalszh.exe'
$sourceDll = Join-Path $repositoryPath '.build\d3d12-bridge-x64\generals-d3d12.dll'
$probePath = Join-Path $repositoryPath '.build\x64-game-startup.json'
$probe = Get-Content -LiteralPath $probePath -Raw | ConvertFrom-Json
if (-not $probe.stayedAlive -or -not $probe.actualD3D12Device -or -not $probe.successfulDraw -or -not $probe.successfulPresent) {
    throw 'A successful x64 game startup/rendering probe is required.'
}
if ((Get-FileHash -LiteralPath $sourceExe).Hash -ne $probe.executableSha256) { throw 'The executable changed after the startup probe.' }
if (-not $probe.bridgeSha256 -or (Get-FileHash -LiteralPath $sourceDll).Hash -ne $probe.bridgeSha256) { throw 'A startup probe for the current bridge is required.' }
$neuralValidation=Get-Content -LiteralPath (Join-Path $repositoryPath '.build\neural-renderer-validation.json') -Raw | ConvertFrom-Json
if($neuralValidation.bridgeSha256 -ne $probe.bridgeSha256 -or -not $neuralValidation.switchingVerified -or -not $neuralValidation.edgeCoverageVerified -or -not $neuralValidation.partialViewportAspectVerified -or -not $neuralValidation.nativeHudReadbackVerified -or -not $neuralValidation.deviceReleaseVerified) {throw 'DLSS/DLAA edge coverage, partial viewport proportions, switching and cleanup tests for the current bridge are required.'}
foreach($mode in @('Off','DLAA','DLSSQuality')) {
    $modeProbe=Get-Content -LiteralPath (Join-Path $repositoryPath ('.build\x64-game-startup-'+$mode+'.json')) -Raw | ConvertFrom-Json
    if(-not $modeProbe.gracefulExitVerified) {throw ('Graceful shutdown must pass: '+$mode)}
    if(-not $modeProbe.audioOutputVerified) {throw ('Actual x64 game audio output must pass: '+$mode)}
    if($modeProbe.executableSha256 -ne $probe.executableSha256 -or $modeProbe.bridgeSha256 -ne $probe.bridgeSha256 -or -not $modeProbe.stayedAlive -or -not $modeProbe.successfulPresent) {throw ('A current game probe is required: '+$mode)}
    if($mode -ne 'Off' -and (-not $modeProbe.neuralAaIntegrated -or -not $modeProbe.neuralHistoryVerified -or $modeProbe.neuralLog.Contains('[error]'))) {throw ('Actual neural game frames must pass: '+$mode)}
}
foreach ($path in @($sourceExe, $sourceDll)) {
    $bytes = [IO.File]::ReadAllBytes($path)
    $peOffset = [BitConverter]::ToInt32($bytes, 0x3c)
    if ([BitConverter]::ToUInt16($bytes, $peOffset + 4) -ne 0x8664) { throw "Expected an AMD64 binary: $path" }
}
$exeText = [Text.Encoding]::GetEncoding(28591).GetString([IO.File]::ReadAllBytes($sourceExe))
if ($exeText.Contains('OptionsMenu.wnd:ButtonDLSS') -or $exeText.Contains('OptionsMenu.wnd:ButtonDLAA')) { throw 'Obsolete AA buttons remain in this build.' }
if (-not (Test-Path -LiteralPath (Join-Path $GameDirectory 'game.dat'))) { throw 'Select the installed Zero Hour directory.' }
if (Get-Process -Name 'generalszh-x64-test' -ErrorAction SilentlyContinue) { throw 'Close the x64 test before installing.' }
# Keep the 64-bit bridge apart from the existing Win32 bridge with the same DLL name.
$testDirectory = Join-Path $GameDirectory 'GeneralsGPT-x64'
$launcherPath = Join-Path $GameDirectory 'Test-X64.cmd'
if ($PSCmdlet.ShouldProcess($testDirectory, 'Install isolated x64 development game with DLSS Quality and DLAA')) {
    $previousDirectory=Join-Path $GameDirectory 'GeneralsGPT-x64-before-dlss'
    if(-not (Test-Path -LiteralPath $previousDirectory) -and (Test-Path -LiteralPath (Join-Path $testDirectory 'generalszh-x64-test.exe'))) {
        $null=New-Item -ItemType Directory -Path $previousDirectory -Force
        foreach($name in @('generalszh-x64-test.exe','generals-d3d12.dll','d3d8to9-LICENSE.md','README.txt')) {
            $previousFile=Join-Path $testDirectory $name
            if(Test-Path -LiteralPath $previousFile) {Copy-Item -LiteralPath $previousFile -Destination (Join-Path $previousDirectory $name)}
        }
        [IO.File]::WriteAllLines((Join-Path $GameDirectory 'Test-X64-Previous.cmd'),@('@echo off','cd /d "%~dp0"','set "GENERALS_RENDERER=d3d12"','set "GENERALS_RENDER_FPS=0"','set "GENERALS_NEURAL_AA=Off"','"%~dp0GeneralsGPT-x64-before-dlss\generalszh-x64-test.exe" -useCwd -nologo','if errorlevel 1 pause'),[Text.Encoding]::ASCII)
    }
    $null = New-Item -ItemType Directory -Path $testDirectory -Force
    $files = @{
        (Join-Path $testDirectory 'generalszh-x64-test.exe') = $sourceExe
        (Join-Path $testDirectory 'generals-d3d12.dll') = $sourceDll
        (Join-Path $testDirectory 'd3d8to9-LICENSE.md') = (Join-Path $repositoryPath '.build\d3d12-bridge-x64\d3d8to9-LICENSE.md')
    }
    foreach ($destination in $files.Keys) {
        if (Test-Path -LiteralPath $destination) { Copy-Item -LiteralPath $destination -Destination ($destination + '.' + [Guid]::NewGuid().ToString('N') + '.bak') }
        Copy-Item -LiteralPath $files[$destination] -Destination $destination -Force
    }
    & (Join-Path $PSScriptRoot 'Copy-StreamlineRuntime.ps1') -ExecutableDirectory $testDirectory
    if (Test-Path -LiteralPath $launcherPath) { Copy-Item -LiteralPath $launcherPath -Destination ($launcherPath + '.' + [Guid]::NewGuid().ToString('N') + '.bak') }
    [IO.File]::WriteAllLines($launcherPath, @('@echo off','cd /d "%~dp0"','set "GENERALS_RENDERER=d3d12"','set "GENERALS_RENDER_FPS=0"','set "GENERALS_FPS_PROFILE=1"','"%~dp0GeneralsGPT-x64\generalszh-x64-test.exe" -useCwd -nologo','if errorlevel 1 pause'), [Text.Encoding]::ASCII)
    [IO.File]::WriteAllText((Join-Path $testDirectory 'README.txt'), @'
Experimental AMD64 game using the D3D8 -> D3D9On12 -> D3D12 bridge.
Use Test-X64.cmd in the parent game directory so existing game assets are found.
Settings contain DLSS Quality and DLAA in the AA dropdown. Choose a mode and
apply the settings. DLSS renders the scene at lower resolution; DLAA uses
native resolution. Both use NVIDIA Streamline and actual depth/motion data.
The HUD is rendered after the neural pass. Selection persists in Options.ini.
NVIDIA RTX feature support is checked at runtime; unsupported hardware keeps
these modes unavailable. Runtime libraries are verified NVIDIA signatures.
Movement of opaque fixed-function meshes includes vertex animation. Shader
displacement and transparent effects currently use camera reprojection.
GPU tests and short game scene checks passed; extended battles require testing.
The retail 32-bit Miles/Bink libraries cannot run in this process; sound and
intro videos need replacement backends. This is a development test build.
The existing FPS build, Win32 DX12 build and original game remain available.
'@)
    [ordered]@{
        executable = (Join-Path $testDirectory 'generalszh-x64-test.exe')
        executableSha256 = (Get-FileHash -LiteralPath $sourceExe).Hash
        bridgeSha256 = (Get-FileHash -LiteralPath $sourceDll).Hash
        architecture = 'AMD64'
        startupDrawPresentVerified = $true
        gracefulExitVerified = $true
        neuralEdgeCoverageVerified = $true
        visualVerified = $false
        fullBattleVerified = $false
        dlssIntegrated = $true
        dlaaIntegrated = $true
        audioBackendReady = $true
        audioOutputVerified = $true
        videoBackendReady = $false
        launcher = $launcherPath
    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $repositoryPath '.build\x64-install.json') -Encoding UTF8
    Write-Output "Installed isolated x64 development launcher: $launcherPath"
}
