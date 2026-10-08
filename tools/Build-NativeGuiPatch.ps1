[CmdletBinding()]
param([string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(-not $OutputDirectory){$OutputDirectory=Join-Path $root '.dist/Native12-2026-10-08-Fix2'}
$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory)
$dll=Join-Path $root '.build/native12/generals-native12.dll'
$stage=Join-Path $root '.build/native12/fix2-update'
$null=New-Item -ItemType Directory -Path $OutputDirectory,$stage -Force
$files=@()
foreach($game in @('CaCG','CaCGZH')){
    $relative=$game+'/Client/generals-native12.dll'
    $destination=Join-Path $stage $relative
    $null=New-Item -ItemType Directory -Path (Split-Path $destination -Parent) -Force
    Copy-Item -LiteralPath $dll -Destination $destination -Force
    $files+=@{path=$relative;sha256=(Get-FileHash -LiteralPath $destination).Hash;bytes=(Get-Item -LiteralPath $destination).Length}
}
$readme=@"
Native12 Fix2 — полосы в интерфейсе и повреждённые края букв

1. Полностью закройте Generals и Zero Hour.
2. Распакуйте архив в папку клиента, где находятся CaCG и CaCGZH.
3. Подтвердите замену двух файлов Client/generals-native12.dll.
4. Запустите игру обычным ярлыком. Переустановка не требуется.

Патч предназначен для клиента Generals GPT Native12 x64 (включая Fix1).
Исправлено соответствие центров пикселей W3D/DX8 и DirectX 12.
Устранены вертикальные швы между фрагментами кнопок и смещение тонких
штрихов шрифтов. Настройки, сохранения и игровые ресурсы не изменяются.
Исправления grayscale-иконок из Fix1 включены в DLL.
Исправление звука из Fix1 находится в EXE и этим патчем не заменяется.
"@
[IO.File]::WriteAllText((Join-Path $stage 'README-Fix2.txt'),$readme,[Text.UTF8Encoding]::new($false))
@{version='Native12-Fix2';architecture='x64';files=$files} | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $stage 'update.manifest.json') -Encoding UTF8
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip=Join-Path $OutputDirectory 'Generals-GPT-Native12-Fix2-Update.zip'
if(Test-Path -LiteralPath $zip){Remove-Item -LiteralPath $zip}
[IO.Compression.ZipFile]::CreateFromDirectory($stage,$zip,[IO.Compression.CompressionLevel]::Optimal,$false)
$archive=[IO.Compression.ZipFile]::OpenRead($zip)
try{
    foreach($file in $files){
        $entry=$archive.GetEntry($file.path)
        if(-not $entry){$entry=$archive.GetEntry($file.path.Replace('/','\'))}
        if(-not $entry){throw ('Missing patch entry '+$file.path)}
        $stream=$entry.Open()
        try{$sha=[Security.Cryptography.SHA256]::Create();try{$hash=[BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','')}finally{$sha.Dispose()}}finally{$stream.Dispose()}
        if($hash -ne $file.sha256){throw ('Corrupt patch entry '+$file.path)}
    }
}finally{$archive.Dispose()}
@{version='Native12-Fix2';zip=$zip;sha256=(Get-FileHash -LiteralPath $zip).Hash;bytes=(Get-Item -LiteralPath $zip).Length;files=$files;archiveVerified=$true} | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $OutputDirectory 'update.json') -Encoding UTF8
Write-Output $zip