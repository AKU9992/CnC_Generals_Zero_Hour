[CmdletBinding()]
param([string]$OutputDirectory,[switch]$Clean)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
Set-Location $repo
if(-not $OutputDirectory){$OutputDirectory=Join-Path $repo 'dist/Native12-2026-10-08-Fix2'}
$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory)
$build=Join-Path $repo '.build/native-client-release'
$null=New-Item -ItemType Directory -Path $build,$OutputDirectory -Force
$state=Join-Path $build 'fix2-release-state.json'
function Set-Stage([string]$stage){
    @{stage=$stage;complete=$false;updatedAt=(Get-Date).ToString('o')} | ConvertTo-Json | Set-Content $state -Encoding UTF8
}
try{
    foreach($edition in @('Generals','ZeroHour')){
        Set-Stage ('build-'+$edition)
        & ./tools/Build-X64Game.ps1 -Native12 -Edition $edition -Clean:$Clean *> (Join-Path $build ('fix2-build-'+$edition+'.log'))
    }
    Set-Stage 'gpu-and-audio-regressions'
    & ./tools/Build-NativeRenderer12.ps1 -W3D -Neural -Test *> (Join-Path $build 'fix2-gpu.log')
    & ./tools/Test-X64Audio.ps1 -BuildOnly *> (Join-Path $build 'fix2-audio-build.log')
    Push-Location (Join-Path $repo '.build/audio-tests')
    try{
        & ./X64AudioTests.exe *> (Join-Path $build 'fix2-audio.log')
        if($LASTEXITCODE -ne 0){throw 'Audio regressions failed.'}
    }finally{Pop-Location}
    Set-Stage 'prepare-full-client'
    & ./tools/Prepare-NativeClientRelease.ps1 *> (Join-Path $build 'fix2-prepare.log')
    Set-Stage 'test-packaged-client'
    & ./tools/Test-NativeClientRelease.ps1 -GuiLayout 'Menus/QuitMenu.wnd' *> (Join-Path $build 'fix2-test-client.log')
    foreach($edition in @('CaCG','CaCGZH')){
        Copy-Item (Join-Path $build ($edition+'-shortcut-validation.json')) (Join-Path $OutputDirectory ($edition+'-before-packaging.json')) -Force
    }
    Set-Stage 'pack-full-client'
    & ./tools/Build-ClientPackage.ps1 -Native12 -GameRoot (Join-Path $build 'client') -OutputDirectory $OutputDirectory *> (Join-Path $build 'fix2-pack.log')
    $installer=Join-Path $OutputDirectory 'Generals-GPT-Setup.exe'
    Set-Stage 'verify-installer-all-files'
    $process=Start-Process -FilePath $installer -ArgumentList '--verify' -WindowStyle Hidden -PassThru
    $process.WaitForExit()
    if($process.ExitCode -ne 0){throw 'Full installer integrity failed.'}
    Set-Stage 'extract-full-installer'
    $destination=Join-Path $build 'install-check-fix2'
    $process=Start-Process -FilePath $installer -ArgumentList @('--extract',$destination) -WindowStyle Hidden -PassThru
    $process.WaitForExit()
    if($process.ExitCode -ne 0){throw 'Full installer extraction failed.'}
    Set-Stage 'test-installed-games'
    & ./tools/Test-NativeClientRelease.ps1 -GameRoot $destination -GuiLayout 'Menus/QuitMenu.wnd' *> (Join-Path $build 'fix2-test-installed.log')
    $checks=@()
    foreach($edition in @('CaCG','CaCGZH')){
        $report=Join-Path $build ($edition+'-shortcut-validation.json')
        $checks+=Get-Content $report -Raw | ConvertFrom-Json
        Copy-Item $report (Join-Path $OutputDirectory ($edition+'-installed-validation.json')) -Force
        Copy-Item (Join-Path $build ($edition+'-release.native.bmp')) (Join-Path $OutputDirectory ($edition+'-menu.native.bmp')) -Force
    }
    $release=Get-Content (Join-Path $OutputDirectory 'release.json') -Raw | ConvertFrom-Json
    $release | Add-Member -NotePropertyName version -NotePropertyValue 'Native12-Fix2'
    $release | Add-Member -NotePropertyName includedFixes -NotePropertyValue @('audio-playback-rate-reset','disabled-unit-promotion-icons','GUI-alpha-preservation','W3D-DX12-pixel-center-correction')
    $release | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $OutputDirectory 'release.json') -Encoding UTF8
    @{version='Native12-Fix2';installerAllFilesVerified=$true;installerExtracted=$true;gpuRegressionsPassed=$true;audioRegressionsPassed=$true;gameChecks=$checks;finishedAt=(Get-Date).ToString('o')} |
        ConvertTo-Json -Depth 7 | Set-Content (Join-Path $OutputDirectory 'validation.json') -Encoding UTF8
    foreach($name in @('fix2-gpu.log','fix2-audio.log','fix2-test-installed.log')){
        Copy-Item (Join-Path $build $name) $OutputDirectory -Force
    }
    @{stage='complete';complete=$true;outputDirectory=$OutputDirectory;finishedAt=(Get-Date).ToString('o')} | ConvertTo-Json | Set-Content $state -Encoding UTF8
}catch{
    @{stage='failed';complete=$false;error=($_|Out-String);updatedAt=(Get-Date).ToString('o')} | ConvertTo-Json | Set-Content $state -Encoding UTF8
    throw
}