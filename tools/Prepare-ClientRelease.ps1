[CmdletBinding()]
param([string]$GameRoot='E:\C&C ZH GPTMOD')
$ErrorActionPreference='Stop'
$repository=Split-Path $PSScriptRoot -Parent
$output=Join-Path $repository '.build\client-release'
$null=New-Item -ItemType Directory -Path $output -Force
$compiler='C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$launcher=Join-Path $output 'generals.exe'
& $compiler /nologo /target:winexe /platform:x64 /optimize+ "/out:$launcher" /reference:System.Windows.Forms.dll /reference:System.Drawing.dll "/win32icon:$GameRoot\CaCG\Generals.ico" (Join-Path $repository 'client\Launcher.cs')
if($LASTEXITCODE -ne 0){throw 'Client launcher compilation failed.'}
$setup=Join-Path $output 'Setup-stub.exe'
& $compiler /nologo /target:winexe /platform:x64 /optimize+ "/out:$setup" /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.IO.Compression.dll /reference:System.IO.Compression.FileSystem.dll /reference:Microsoft.CSharp.dll "/win32icon:$GameRoot\CaCG\Generals.ico" (Join-Path $repository 'client\Installer.cs')
if($LASTEXITCODE -ne 0){throw 'Installer compilation failed.'}
foreach($edition in @('CaCG','CaCGZH')) {
    $game=Join-Path $GameRoot $edition
    $old=Join-Path $game 'GeneralsGPT-x64'
    $stable=Join-Path $game 'Client'
    $alreadyStable=-not (Test-Path -LiteralPath $old)
    if($alreadyStable){$old=$stable}
    $sourceExe=Join-Path $old $(if($alreadyStable){'generals-client.exe'}elseif($edition -eq 'CaCG'){'generals-x64-test.exe'}else{'generalszh-x64-test.exe'})
    $null=New-Item -ItemType Directory -Path $stable -Force
    if(-not $alreadyStable) {
      Copy-Item -LiteralPath $sourceExe -Destination (Join-Path $stable 'generals-client.exe') -Force
      foreach($file in @('generals-d3d12.dll','d3d8to9-LICENSE.md')) {
        Copy-Item -LiteralPath (Join-Path $old $file) -Destination (Join-Path $stable $file) -Force
      }
    $null=New-Item -ItemType Directory -Path (Join-Path $stable 'Streamline') -Force
    foreach($file in @('nvngx_dlss.dll','nvngx_dlss.license.txt','sl.common.dll','sl.dlss.dll','sl.interposer.dll','Streamline-license.txt')) {
        Copy-Item -LiteralPath (Join-Path $old ('Streamline\'+$file)) -Destination (Join-Path $stable ('Streamline\'+$file)) -Force
    }
    }
    Copy-Item -LiteralPath (Join-Path $repository 'LICENSE.md') -Destination (Join-Path $stable 'GameCode-LICENSE.md') -Force
    $crt='C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Redist\MSVC\14.51.36231\x64\Microsoft.VC145.CRT'
    foreach($file in @('msvcp140.dll','vcruntime140.dll','vcruntime140_1.dll','msvcp140_1.dll','msvcp140_2.dll','msvcp140_atomic_wait.dll')) {
        Copy-Item -LiteralPath (Join-Path $crt $file) -Destination (Join-Path $stable $file) -Force
    }
    if($edition -eq 'CaCGZH'){[IO.File]::WriteAllText((Join-Path $stable 'ZeroHour.edition'),'ZeroHour')}
    $backup=Join-Path $output ('original-launchers\'+$edition)
    $null=New-Item -ItemType Directory -Path $backup -Force
    $original=Join-Path $game 'generals.exe'
    if(-not (Test-Path -LiteralPath (Join-Path $backup 'generals.exe'))) {
        Copy-Item -LiteralPath $original -Destination (Join-Path $backup 'generals.exe')
    }
    Copy-Item -LiteralPath $launcher -Destination $original -Force
    [ordered]@{edition=$edition;launcher=$original;executableSha256=(Get-FileHash (Join-Path $stable 'generals-client.exe')).Hash;bridgeSha256=(Get-FileHash (Join-Path $stable 'generals-d3d12.dll')).Hash} | ConvertTo-Json | Set-Content (Join-Path $output ($edition+'-release.json')) -Encoding UTF8
}
Write-Output 'Existing desktop shortcuts now target the release launcher.'
