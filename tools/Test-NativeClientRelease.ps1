[CmdletBinding()]
param([string]$GameRoot,[string]$GuiLayout)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$build=Join-Path $repo '.build/native-client-release'
if(-not $GameRoot){$GameRoot=Join-Path $build 'client'}
$GameRoot=[IO.Path]::GetFullPath($GameRoot)
$audioExe=Join-Path $repo '.build/audio-tests/X64AudioTests.exe'
$variables=@{GENERALS_TEST_GUI_LAYOUT=$GuiLayout;GENERALS_TEST_QUIT_SECONDS='15';GENERALS_X64_STARTUP_TRACE='1';GENERALS_NEURAL_AA='DLSSQuality';GENERALS_TEST_BASE_GAME=$null;GENERALS_TEST_USER_DATA=$null;GENERALS_CAPTURE_FRAME=$null;GENERALS_TEST_SHELL_MAP=$null}
$previous=@{}
try{
    foreach($name in $variables.Keys){$previous[$name]=[Environment]::GetEnvironmentVariable($name);[Environment]::SetEnvironmentVariable($name,$variables[$name])}
    foreach($edition in @('CaCG','CaCGZH')){
        $game=Join-Path $GameRoot $edition
        $clientExe=Join-Path $game 'Client/generals-client.exe'
        $user=Join-Path $build ('user-'+$edition)
        $null=New-Item -ItemType Directory -Path $user -Force
        [IO.File]::WriteAllText((Join-Path $user 'Options.ini'),"StaticGameLOD = High"+[Environment]::NewLine+"IdealStaticGameLOD = High"+[Environment]::NewLine+"UseShadowVolumes = yes"+[Environment]::NewLine+"UseShadowDecals = yes"+[Environment]::NewLine)
        $env:GENERALS_TEST_USER_DATA=$user+'\'
        $env:GENERALS_CAPTURE_FRAME=Join-Path $build ($edition+'-release')
        $env:GENERALS_TEST_SHELL_MAP=if($edition -eq 'CaCG'){'maps\Tournament Lake\Tournament Lake.map'}else{$null}
        $launcher=$null;$process=$null;$audio=$null
        try{
            $launcher=Start-Process -FilePath (Join-Path $game 'generals.exe') -WorkingDirectory $game -WindowStyle Hidden -PassThru
            for($attempt=0;$attempt -lt 80 -and -not $process;$attempt++){
                Start-Sleep -Milliseconds 250
                $process=Get-Process -Name 'generals-client' -ErrorAction SilentlyContinue | Where-Object {$_.Path -eq $clientExe} | Select-Object -First 1
            }
            if(-not $process){throw 'Native launcher did not start its packaged client.'}
            $null=$process.Handle
            $audio=Start-Process -FilePath $audioExe -ArgumentList @('--watch',[string]$process.Id) -WorkingDirectory (Split-Path $audioExe -Parent) -WindowStyle Hidden -RedirectStandardOutput (Join-Path $build ($edition+'-audio.log')) -RedirectStandardError (Join-Path $build ($edition+'-audio-error.log')) -PassThru
            $modules=@();$watch=[Diagnostics.Stopwatch]::StartNew();$exited=$false
            while($watch.Elapsed.TotalSeconds -lt 55){
                if($process.WaitForExit(1000)){$exited=$true;break}
                try{$process.Refresh();$modules=@($modules+@($process.Modules | Where-Object {$_.ModuleName -match '^d3d(8|9|12)\.dll$|^generals-.*\.dll$'} | Select-Object -ExpandProperty ModuleName) | Sort-Object -Unique)}catch{}
            }
            if(-not $exited -or $process.ExitCode -ne 0 -or -not $launcher.WaitForExit(5000) -or $launcher.ExitCode -ne 0){throw 'Packaged game or launcher failed graceful exit.'}
            if(-not $audio.WaitForExit(5000) -or $audio.ExitCode -ne 0){throw 'Packaged game audio output failed.'}
            $lines=[IO.File]::ReadAllLines((Join-Path $game 'GeneralsNative12.log')) | Where-Object {$_.StartsWith('PID '+$process.Id+':')}
            $log=$lines -join [Environment]::NewLine
            $capture=$env:GENERALS_CAPTURE_FRAME+'.native.bmp'
            $textures=Join-Path $game 'GeneralsTextureFailures.log'
            $missing=if(Test-Path -LiteralPath $textures){@([IO.File]::ReadAllLines($textures) | Where-Object {$_.StartsWith('PID '+$process.Id+':') -and $_.Contains('Missing texture')})}else{@()}
            if(-not $log.Contains('First native W3D draw completed') -or -not $log.Contains('First native W3D Present completed') -or -not $log.Contains('32 native W3D DLSS/DLAA world frames completed') -or -not $log.Contains('Native runtime audit: D3D12 present, D3D8/D3D9 absent') -or $log.Contains('draw failed') -or $modules -contains 'd3d8.dll' -or $modules -contains 'd3d9.dll' -or -not (Test-Path -LiteralPath $capture) -or $missing.Count -gt 10){throw 'Packaged native rendering failed.'}
            if(-not $log.Contains('Native water HLSL draw completed') -or -not $log.Contains('Native stencil shadow draw completed')){throw 'Packaged water or shadow pass missing.'}
            [ordered]@{edition=$edition;guiLayout=$GuiLayout;launcher=(Join-Path $game 'generals.exe');actualClient=$clientExe;launcherSha256=(Get-FileHash -LiteralPath (Join-Path $game 'generals.exe')).Hash;executableSha256=(Get-FileHash -LiteralPath $clientExe).Hash;backendSha256=(Get-FileHash -LiteralPath (Join-Path $game 'Client/generals-native12.dll')).Hash;rendererVerified=$true;neuralVerified=$true;audioVerified=$true;gracefulExitVerified=$true;waterVerified=$true;shadowVerified=$true;graphicsModules=$modules;missingTextureCount=$missing.Count;capture=$capture;testedAt=(Get-Date).ToString('o')} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $build ($edition+'-shortcut-validation.json')) -Encoding UTF8
            Write-Output ($edition+': native launcher, DLSS Quality, water, shadows, audio and graceful exit passed.')
        }finally{foreach($owned in @($process,$launcher,$audio)){if($owned -and -not $owned.HasExited){Stop-Process -Id $owned.Id}}}
    }
}finally{foreach($name in $previous.Keys){[Environment]::SetEnvironmentVariable($name,$previous[$name])}}