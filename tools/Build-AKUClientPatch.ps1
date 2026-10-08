[CmdletBinding()]
param([string]$OutputDirectory,[ValidateSet("Fix4","Fix5")][string]$PatchVersion="Fix4",[string]$ReportPath)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$releaseName='AKU9992-Native12-'+$PatchVersion
if(-not $ReportPath){$ReportPath=Join-Path $root ('docs/native12-'+$PatchVersion.ToLower()+'.md')}
if(-not $OutputDirectory){$OutputDirectory=Join-Path $root ("dist/"+$releaseName)}
$stage=Join-Path $root (".build/native12/"+$PatchVersion.ToLower()+"-update")
$null=New-Item -ItemType Directory -Path $OutputDirectory,$stage -Force
$csc='C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$launcher=Join-Path $root '.build/native12/AKU9992-launcher.exe'
$stub=Join-Path $root '.build/native12/AKU9992-Setup-stub.exe'
$icon=Join-Path $root '.build/original-client/CaCG/Generals.ico'
& $csc /nologo /target:winexe /platform:x64 /optimize+ "/out:$launcher" /reference:System.Windows.Forms.dll /reference:System.Drawing.dll "/win32icon:$icon" (Join-Path $root 'client/Launcher.cs')
if($LASTEXITCODE -ne 0){throw 'Launcher compilation failed.'}
& $csc /nologo /target:winexe /platform:x64 /optimize+ /define:NATIVE12,PATCH "/out:$stub" /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.IO.Compression.dll /reference:System.IO.Compression.FileSystem.dll /reference:Microsoft.CSharp.dll "/win32icon:$icon" (Join-Path $root 'client/Installer.cs')
if($LASTEXITCODE -ne 0){throw 'Patch installer compilation failed.'}
$readme="AKU9992 — Native12 x64 — $PatchVersion`r`n`r`nЗакройте обе игры. Установите патч в общую папку CaCG и CaCGZH либо распакуйте ZIP с заменой файлов.`r`nОбычный запуск: generals.exe. Настройки FPS: AKU9992-Settings.cmd в папке выбранной игры.`r`nStandard: 60 FPS либо ниже частоты экрана. Auto: активная частота экрана. Custom: до активной частоты экрана. В фоне предел 30 FPS. VSync выбирается отдельно.`r`nСимуляция сохраняет 30 тиков/с при обычной скорости; предел FPS не меняет скорость игры. Индексированная геометрия сокращает работу CPU без снижения качества.`r`nСохранены исправления звука, иконок, полос интерфейса, воды, теней и прокрутки мышью. DLSS Quality/DLAA требуют совместимой RTX.`r`nОтчёт проверки: Client/AKU9992-Validation.md. Сохранения и выбранные настройки FPS патч не заменяет.`r`nОткат: закройте игры и распакуйте сохранённый AKU9992-Native12-Fix3-Update.zip с заменой файлов. Для полного отката настроек удалите только Client/AKU9992-settings.ini и AKU9992-Settings.cmd.`r`nИсправлена загрузка сохранений в новый профиль и восстановление локального игрока; формат файлов сохранён. Авторство сборки: AKU9992.`r`n"
if($PatchVersion -eq "Fix5"){$readme=$readme.Replace("AKU9992-Native12-Fix3-Update.zip","AKU9992-Native12-Fix4-Update.zip")+"`r`nFix5: исправлен сброс кэша текста меню настроек и повторные выделения DX12-дескрипторов.`r`n"}
$files=@()
foreach($game in @('CaCG','CaCGZH')){
 $exe=if($game -eq 'CaCG'){'.build/game-generals-native12/Generals/generalsv.exe'}else{'.build/game-zerohour-native12/GeneralsMD/generalszh.exe'}
 $sources=@{($game+'/generals.exe')=$launcher;($game+'/Client/generals-client.exe')=(Join-Path $root $exe);($game+'/Client/generals-native12.dll')=(Join-Path $root '.build/native12/generals-native12.dll')}
 foreach($relative in $sources.Keys){
  $destination=Join-Path $stage $relative
  $null=New-Item -ItemType Directory -Path (Split-Path $destination -Parent) -Force
  Copy-Item -LiteralPath $sources[$relative] -Destination $destination -Force
  $files+=$relative
 }
$relative=$game+'/AKU9992-Settings.cmd'
 [IO.File]::WriteAllText((Join-Path $stage $relative),"@echo off`r`ncd /d `"%~dp0`"`r`nstart `"`" `"%~dp0generals.exe`" --settings`r`n",[Text.Encoding]::ASCII)
 $files+=$relative
 $relative=$game+'/Client/README-Native12.txt'
 [IO.File]::WriteAllText((Join-Path $stage $relative),$readme,[Text.UTF8Encoding]::new($true))
 $files+=$relative
}
foreach($game in @('CaCG','CaCGZH')){
 $relative=$game+'/Client/AKU9992-Validation.md'
 Copy-Item -LiteralPath $ReportPath -Destination (Join-Path $stage $relative) -Force
 $files+=$relative
}
$oldReport=Join-Path $stage 'AKU9992-Validation.md'
if(Test-Path -LiteralPath $oldReport){Remove-Item -LiteralPath $oldReport}
# Audit known local identities without recording their values in the report.
$sensitive=@($env:COMPUTERNAME,$env:USERNAME,(Split-Path $env:USERPROFILE -Leaf),[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String('Q29tbXVuaXR5IFBhdGNo')))
$gitName=& git -C $root config user.name
if($gitName -and $gitName -ne 'AKU9992'){$sensitive+=$gitName}
$audit=@()
foreach($relative in @($files+'AKU9992-Setup-stub.exe')){
 $path=if($relative -eq 'AKU9992-Setup-stub.exe'){$stub}else{Join-Path $stage $relative}
 $bytes=[IO.File]::ReadAllBytes($path)
 $ascii=[Text.Encoding]::Latin1.GetString($bytes);$unicode=[Text.Encoding]::Unicode.GetString($bytes);$unicodeOdd=[Text.Encoding]::Unicode.GetString($bytes,1,$bytes.Length-1);$utf8=[Text.Encoding]::UTF8.GetString($bytes)
 foreach($value in $sensitive){
  if($value -and $value.Length -ge 4 -and ($ascii.IndexOf($value,[StringComparison]::OrdinalIgnoreCase) -ge 0 -or $unicode.IndexOf($value,[StringComparison]::OrdinalIgnoreCase) -ge 0 -or $unicodeOdd.IndexOf($value,[StringComparison]::OrdinalIgnoreCase) -ge 0 -or $utf8.IndexOf($value,[StringComparison]::OrdinalIgnoreCase) -ge 0)){throw ('Privacy audit failed: '+$relative)}
 }
 if($ascii -match '(?i)C:\\Users\\' -or $unicode -match '(?i)C:\\Users\\' -or $unicodeOdd -match '(?i)C:\\Users\\' -or $utf8 -match '(?i)C:\\Users\\'){throw ('Private path found: '+$relative)}
 $audit+=@{file=$relative;identityStringsAbsent=$true}
}
$manifest=foreach($relative in ($files|Sort-Object)){$path=Join-Path $stage $relative;(Get-FileHash $path).Hash+"`t"+(Get-Item $path).Length+"`t"+$relative}
[IO.File]::WriteAllLines((Join-Path $stage 'release.manifest'),$manifest,[Text.UTF8Encoding]::new($false))
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip=Join-Path $OutputDirectory ($releaseName+"-Update.zip")
if(Test-Path -LiteralPath $zip){Remove-Item -LiteralPath $zip}
[IO.Compression.ZipFile]::CreateFromDirectory($stage,$zip,[IO.Compression.CompressionLevel]::Optimal,$false)
$archive=[IO.Compression.ZipFile]::OpenRead($zip)
try{
 if($archive.Entries.Count -ne $files.Count+1){throw 'Unexpected patch entry count.'}
 foreach($line in $manifest){
  $fields=$line.Split("`t");$entry=$archive.GetEntry($fields[2]);if(-not $entry){throw 'Missing patch entry.'}
  $stream=$entry.Open();$sha=[Security.Cryptography.SHA256]::Create()
  try{$hash=[BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','')}finally{$sha.Dispose();$stream.Dispose()}
  if($hash -ne $fields[0]){throw 'Patch hash mismatch.'}
 }
}finally{$archive.Dispose()}
$setup=Join-Path $OutputDirectory ($releaseName+"-Patch.exe")
$output=[IO.File]::Create($setup)
try{
 foreach($path in @($stub,$zip)){$input=[IO.File]::OpenRead($path);try{$input.CopyTo($output)}finally{$input.Dispose()}}
 $writer=[IO.BinaryWriter]::new($output,[Text.Encoding]::UTF8,$true)
 try{$writer.Write([long](Get-Item $stub).Length);$writer.Write([long](Get-Item $zip).Length);$writer.Write([Text.Encoding]::ASCII.GetBytes('GGPTPK01'))}finally{$writer.Dispose()}
}finally{$output.Dispose()}
$check=Start-Process -FilePath $setup -ArgumentList '--verify' -WindowStyle Hidden -PassThru -Wait
if($check.ExitCode -ne 0){throw 'Embedded patch verification failed.'}
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'README.txt'),$readme,[Text.UTF8Encoding]::new($true))
@{publisher='AKU9992';version=("Native12-"+$PatchVersion);archiveVerified=$true;installerVerified=$true;privacyAudit=$audit;sha256=(Get-FileHash $zip).Hash;bytes=(Get-Item $zip).Length}|ConvertTo-Json -Depth 5|Set-Content (Join-Path $OutputDirectory 'update.json') -Encoding utf8
Write-Output ('Verified patch: '+$zip)