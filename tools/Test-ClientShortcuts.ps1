[CmdletBinding()]
param([string]$GameRoot='E:\C&C ZH GPTMOD',[switch]$Direct)
$ErrorActionPreference='Stop'
$repository=Split-Path $PSScriptRoot -Parent
$output=Join-Path $repository '.build\client-release'
$audioExe=Join-Path $repository '.build\audio-tests\X64AudioTests.exe'
$saved=@{}
foreach($key in @('GENERALS_TEST_QUIT_SECONDS','GENERALS_CAPTURE_FRAME','GENERALS_X64_STARTUP_TRACE','GENERALS_NEURAL_AA')){$saved[$key]=[Environment]::GetEnvironmentVariable($key)}
try {
    foreach($edition in @('CaCG','CaCGZH')) {
        $game=[IO.Path]::GetFullPath((Join-Path $GameRoot $edition))
        $exe=Join-Path $game 'Client\generals-client.exe'
        if(Get-Process -Name 'generals-client','generals-x64-test','generalszh-x64-test' -ErrorAction SilentlyContinue){throw 'Close the game before the release probe.'}
        $env:GENERALS_TEST_QUIT_SECONDS='20';$env:GENERALS_X64_STARTUP_TRACE='1';$env:GENERALS_NEURAL_AA='DLAA'
        $frame=Join-Path $output ($edition+'-release.bmp');$env:GENERALS_CAPTURE_FRAME=$frame
        $name=if($edition -eq 'CaCG'){'Command and Conquer Generals.lnk'}else{'Command and Conquer Generals Zero Hour [MOD SymBioz].lnk'}
        $shortcut=Join-Path ([Environment]::GetFolderPath('CommonDesktopDirectory')) $name
        $launcherPath=if($Direct){Join-Path $game 'generals.exe'}else{$shortcut}
        $before=@(Get-Process -Name generals -ErrorAction SilentlyContinue|ForEach-Object Id)
        $started=Get-Date
        Start-Process -FilePath $launcherPath -WorkingDirectory $game
        $process=$null;$launcher=$null;$audio=$null
        try {
            for($attempt=0;$attempt -lt 80 -and -not $process;$attempt++) {
                Start-Sleep -Milliseconds 250
                $process=Get-Process -Name 'generals-client' -ErrorAction SilentlyContinue|Where-Object{$_.Path -eq $exe}|Select-Object -First 1
            }
            if(-not $process){throw 'The desktop shortcut did not start the release client.'}
            $launcher=Get-Process -Name generals -ErrorAction SilentlyContinue|Where-Object{$_.Id -notin $before -and $_.Path -eq (Join-Path $game 'generals.exe')}|Select-Object -First 1
            if(-not $launcher){throw 'The release launcher process was not found.'}
            $null=$launcher.Handle; $null=$process.Handle
            $audio=Start-Process -FilePath $audioExe -ArgumentList @('--watch',[string]$process.Id) -WorkingDirectory (Split-Path $audioExe -Parent) -WindowStyle Hidden -RedirectStandardOutput (Join-Path $output ($edition+'-audio.log')) -RedirectStandardError (Join-Path $output ($edition+'-audio-error.log')) -PassThru
            if(-not $process.WaitForExit(60000)){throw 'The release client failed graceful exit.'}
            if(-not $launcher.WaitForExit(5000) -or $launcher.ExitCode -ne 0){throw ('The release launcher failed: launcher='+$launcher.ExitCode+' game='+$process.ExitCode)}
            if(-not $audio.WaitForExit(5000) -or $audio.ExitCode -ne 0){throw 'No game audio output detected.'}
            $log=[IO.File]::ReadAllText((Join-Path $game 'GeneralsD3D12.log'))
            $pidLog=($log -split "`n"|Where-Object{$_.StartsWith("PID $($process.Id):")}) -join "`n"
            if(-not $pidLog.Contains('First successful D3D12 draw: 0x00000000') -or -not $pidLog.Contains('First D3D12 presentation: 0x00000000')){throw 'Release rendering failed.'}
            $neural=[IO.File]::ReadAllText((Join-Path $game 'GeneralsNeuralAA.log'))
            $neural=($neural -split "`n"|Where-Object{$_.StartsWith("PID $($process.Id):")}) -join "`n"
            if(-not $neural.Contains('32 actual neural frames completed')){throw 'Release DLAA processing failed.'}
            [ordered]@{edition=$edition;shortcut=$launcherPath;actualClient=$exe;executableSha256=(Get-FileHash $exe).Hash;rendererVerified=$true;neuralVerified=$true;audioVerified=$true;gracefulExitVerified=$true;testedAt=$started.ToString('o')}|ConvertTo-Json|Set-Content (Join-Path $output ($edition+'-shortcut-validation.json')) -Encoding UTF8
            Write-Output ($edition+': shortcut, renderer, DLAA, audio and exit passed.')
        } finally {
            foreach($owned in @($process,$launcher,$audio)){if($owned -and -not $owned.HasExited){Stop-Process -Id $owned.Id}}
        }
    }
} finally {foreach($key in $saved.Keys){[Environment]::SetEnvironmentVariable($key,$saved[$key])}}
