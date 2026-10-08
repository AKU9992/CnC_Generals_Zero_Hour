[CmdletBinding()]
param([string]$AssetRoot,[string]$GameRoot)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
if(-not $AssetRoot){$AssetRoot=Join-Path $repo '.build/original-client'}
if(-not $GameRoot){$GameRoot=Join-Path $repo '.build/native-client-release/client'}
$build=Join-Path $repo '.build/native-client-release'
$null=New-Item -ItemType Directory -Path $build -Force
$csc='C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe'
& $csc /nologo /target:winexe /platform:x64 /optimize+ "/out:$build/generals.exe" /reference:System.Windows.Forms.dll /reference:System.Drawing.dll "/win32icon:$AssetRoot/CaCG/Generals.ico" (Join-Path $repo 'client/Launcher.cs')
if($LASTEXITCODE -ne 0){throw 'Native launcher compilation failed.'}
& $csc /nologo /target:winexe /platform:x64 /optimize+ /define:NATIVE12 "/out:$build/Setup-stub.exe" /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.IO.Compression.dll /reference:System.IO.Compression.FileSystem.dll /reference:Microsoft.CSharp.dll "/win32icon:$AssetRoot/CaCG/Generals.ico" (Join-Path $repo 'client/Installer.cs')
if($LASTEXITCODE -ne 0){throw 'Installer compilation failed.'}
foreach($edition in @('CaCG','CaCGZH')){
    $source=Join-Path $AssetRoot $edition
    $target=Join-Path $GameRoot $edition
    if([IO.Path]::GetFullPath($source).TrimEnd('\') -eq [IO.Path]::GetFullPath($target).TrimEnd('\')){throw 'Use a separate native client destination.'}
    if(Test-Path -LiteralPath (Join-Path $target 'Client/generals-d3d12.dll')){throw 'Destination contains a previous bridge client; use a new folder.'}
    $null=New-Item -ItemType Directory -Path $target -Force
    Get-ChildItem -LiteralPath $source -File | Where-Object { $_.Extension -in @('.big','.ico','.ttf') -or $_.Name -in @('game.dat','Generals.dat','langdata.dat','00000000.016','00000000.256') } | Copy-Item -Destination $target -Force
    foreach($folder in @('Data','art','Window','options')){
        $path=Join-Path $source $folder
        if(Test-Path -LiteralPath $path){
            $base=[IO.Path]::GetFullPath($source).TrimEnd('\')
            foreach($file in Get-ChildItem -LiteralPath $path -Recurse -File){
                if($file.Name -match '(?i)\.bak$|\.log$|\.tmp$|\.dmp$|serial|_sn\.|Test-|CrashInfo|keych'){continue}
                $destination=Join-Path $target $file.FullName.Substring($base.Length+1)
                $null=New-Item -ItemType Directory -Path (Split-Path $destination -Parent) -Force
                Copy-Item -LiteralPath $file.FullName -Destination $destination -Force
            }
        }
    }
    $client=Join-Path $target 'Client'
    $null=New-Item -ItemType Directory -Path $client -Force
    $exe=if($edition -eq 'CaCG'){'.build/game-generals-native12/Generals/generalsv.exe'}else{'.build/game-zerohour-native12/GeneralsMD/generalszh.exe'}
    Copy-Item -LiteralPath (Join-Path $repo $exe) -Destination (Join-Path $client 'generals-client.exe') -Force
    Copy-Item -LiteralPath (Join-Path $repo '.build/native12/generals-native12.dll') -Destination $client -Force
    foreach($file in @('msvcp140.dll','vcruntime140.dll','vcruntime140_1.dll','msvcp140_1.dll','msvcp140_2.dll','msvcp140_atomic_wait.dll')){
        Copy-Item -LiteralPath (Join-Path $source ('Client/'+$file)) -Destination $client -Force
    }
    Copy-Item -LiteralPath (Join-Path $repo 'LICENSE.md') -Destination (Join-Path $client 'GameCode-LICENSE.md') -Force
    & (Join-Path $PSScriptRoot 'Copy-StreamlineRuntime.ps1') -ExecutableDirectory $client
    if($edition -eq 'CaCGZH'){[IO.File]::WriteAllText((Join-Path $client 'ZeroHour.edition'),'ZeroHour')}
    Copy-Item -LiteralPath (Join-Path $build 'generals.exe') -Destination $target -Force
    $readme=@"
Generals GPT — Native DirectX 12 / x64, тестовая сборка.
Распакуйте CaCG и CaCGZH рядом. Запускайте generals.exe в нужной папке.
DLAA / DLSS Quality выбираются в настройках сглаживания.
Для начала рекомендуем Off; DLSS/DLAA требуют совместимую NVIDIA RTX.
Проверяйте схватки, кампании, воду, тени, звук и смену разрешения.
Bink-видео x64 пока не перенесены. Длительные матчи требуют проверки.
Логи: GeneralsNative12.log, GeneralsTextureFailures.log в папке игры.
Fix1: исправлены частоты звуков переиспользованных каналов и серые иконки.
Fix2: исправлены центры пикселей W3D/DX12, швы кнопок и штрихи текста.
"@
    [IO.File]::WriteAllText((Join-Path $client 'README-Native12.txt'),$readme,[Text.UTF8Encoding]::new($true))
}
Write-Output ('Prepared native client: '+$GameRoot)