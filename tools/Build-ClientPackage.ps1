[CmdletBinding()]
param([string]$GameRoot='E:\C&C ZH GPTMOD',[string]$OutputDirectory,[switch]$ReuseArchive,[switch]$Native12)
$ErrorActionPreference='Stop'
$repository=Split-Path $PSScriptRoot -Parent
$build=Join-Path $repository $(if($Native12){'.build\native-client-release'}else{'.build\client-release'})
if(-not $OutputDirectory){$OutputDirectory=Join-Path $repository 'dist'}
$null=New-Item -ItemType Directory -Path $OutputDirectory -Force
Add-Type -AssemblyName System.IO.Compression
$files=[Collections.Generic.List[object]]::new()
foreach($edition in @('CaCG','CaCGZH')) {
    $game=[IO.Path]::GetFullPath((Join-Path $GameRoot $edition)).TrimEnd('\')
    $validation=Get-Content (Join-Path $build ($edition+'-shortcut-validation.json')) -Raw|ConvertFrom-Json
    if(-not $validation.rendererVerified -or -not $validation.audioVerified -or -not $validation.gracefulExitVerified -or
        $validation.executableSha256 -ne (Get-FileHash (Join-Path $game 'Client\generals-client.exe')).Hash){throw ('A current desktop shortcut probe is required: '+$edition)}
    if($Native12){
        if(-not $validation.neuralVerified -or -not $validation.waterVerified -or -not $validation.shadowVerified -or
            $validation.backendSha256 -ne (Get-FileHash (Join-Path $game 'Client/generals-native12.dll')).Hash -or
            $validation.launcherSha256 -ne (Get-FileHash (Join-Path $game 'generals.exe')).Hash){throw ('A current native client probe is required: '+$edition)}
        if(Test-Path -LiteralPath (Join-Path $game 'Client/generals-d3d12.dll')){throw 'Native package must not contain the old renderer bridge.'}
    }
    $candidates=@(Get-ChildItem -LiteralPath $game -File|Where-Object {
        $_.Extension -in @('.big','.ico','.ttf') -or $_.Name -in @('generals.exe','game.dat','Generals.dat','langdata.dat','00000000.016','00000000.256')
    })
    foreach($folder in @('Client','Data','art','Window','options')) {
        $path=Join-Path $game $folder
        if(Test-Path -LiteralPath $path){$candidates+=Get-ChildItem -LiteralPath $path -Recurse -File}
    }
    foreach($file in $candidates|Sort-Object FullName -Unique) {
        if($file.Name -match '(?i)\.bak$|\.log$|\.tmp$|\.dmp$|serial|_sn\.|Test-|CrashInfo|keych'){continue}
        $relative=$edition+'/'+$file.FullName.Substring($game.Length+1).Replace('\','/')
        $files.Add([pscustomobject]@{File=$file.FullName;Relative=$relative;Length=$file.Length})
    }
}
$zipPath=Join-Path $OutputDirectory 'Generals-GPT-Client.zip'
$manifest=[Collections.Generic.List[string]]::new()
if(-not $ReuseArchive) {
$stream=[IO.File]::Create($zipPath)
$archive=[IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Create,$false)
try {
    $count=0
    foreach($file in $files) {
        $hash=(Get-FileHash -LiteralPath $file.File -Algorithm SHA256).Hash
        $manifest.Add($hash+"`t"+$file.Length+"`t"+$file.Relative)
        $entry=$archive.CreateEntry($file.Relative,[IO.Compression.CompressionLevel]::Fastest)
        $input=[IO.File]::OpenRead($file.File);$output=$entry.Open()
        try{$input.CopyTo($output)}finally{$input.Dispose();$output.Dispose()}
        $count++
        if($count % 1000 -eq 0){Write-Output ('Packed '+$count+' / '+$files.Count+' files.')}
    }
    $entry=$archive.CreateEntry('release.manifest',[IO.Compression.CompressionLevel]::Optimal)
    $writer=[IO.StreamWriter]::new($entry.Open(),[Text.UTF8Encoding]::new($false))
    try{$writer.Write($manifest -join "`n")}finally{$writer.Dispose()}
}finally{$archive.Dispose()}
[IO.File]::WriteAllLines((Join-Path $build 'release.manifest'),$manifest,[Text.UTF8Encoding]::new($false))
} elseif(-not (Test-Path -LiteralPath $zipPath)) {throw 'The existing archive was not found.'}
$setupPath=Join-Path $OutputDirectory 'Generals-GPT-Setup.exe'
$stub=Join-Path $build 'Setup-stub.exe'
$temporary=$setupPath+'.building'
$target=[IO.File]::Create($temporary)
$payload=[IO.File]::OpenRead($zipPath)
try {
    $stubStream=[IO.File]::OpenRead($stub)
    try{$stubStream.CopyTo($target)}finally{$stubStream.Dispose()}
    $origin=$target.Position;$size=$payload.Length;$payload.CopyTo($target)
    $writer=[IO.BinaryWriter]::new($target,[Text.Encoding]::UTF8,$true)
    $writer.Write([long]$origin);$writer.Write([long]$size);$writer.Write([Text.Encoding]::ASCII.GetBytes('GGPTPK01'));$writer.Flush()
}finally{$payload.Dispose();$target.Dispose()}
for($attempt=0;;$attempt++) {
    try {Move-Item -LiteralPath $temporary -Destination $setupPath -Force;break}
    catch [IO.IOException] {if($attempt -ge 31){throw};Start-Sleep -Milliseconds 250}
}
$results=[ordered]@{builtAt=(Get-Date).ToString('o');files=$files.Count;unpackedBytes=($files|Measure-Object Length -Sum).Sum;archive=$zipPath;archiveBytes=(Get-Item $zipPath).Length;archiveSha256=(Get-FileHash $zipPath).Hash;installer=$setupPath;installerBytes=(Get-Item $setupPath).Length;installerSha256=(Get-FileHash $setupPath).Hash}
$results|ConvertTo-Json|Set-Content (Join-Path $OutputDirectory 'release.json') -Encoding UTF8
Write-Output ($results|ConvertTo-Json)
