[CmdletBinding()]
param([Parameter(Mandatory)][string]$ClientRoot,[string]$OutputPath)
$ErrorActionPreference='Stop'
$resolvedRoot=(Resolve-Path -LiteralPath $ClientRoot).Path
function Read-PeMachine([string]$path){
    $stream=[IO.File]::Open($path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    $reader=[IO.BinaryReader]::new($stream)
    try{
        if($reader.ReadUInt16() -ne 0x5a4d){return 'NotPE'}
        $null=$stream.Seek(0x3c,[IO.SeekOrigin]::Begin)
        $offset=$reader.ReadInt32()
        if($offset -lt 64 -or $offset -gt $stream.Length-24){throw ('Invalid PE offset: '+$path)}
        $null=$stream.Seek($offset,[IO.SeekOrigin]::Begin)
        if($reader.ReadUInt32() -ne 0x4550){throw ('Invalid PE signature: '+$path)}
        switch($reader.ReadUInt16()){
            0x8664{return 'AMD64'}
            0x14c{return 'I386'}
            0xaa64{return 'ARM64'}
            default{return 'Other'}
        }
    }finally{$reader.Dispose()}
}
$runtime=@();$legacy=@()
foreach($edition in @('CaCG','CaCGZH')){
    $editionRoot=Join-Path $resolvedRoot $edition
    $runtimeRoot=Join-Path $editionRoot 'Client'
    if(-not (Test-Path -LiteralPath (Join-Path $runtimeRoot 'generals-client.exe'))){throw ('Missing active runtime: '+$edition)}
    foreach($file in Get-ChildItem -LiteralPath $runtimeRoot -File -Recurse | Where-Object {$_.Extension -in @('.exe','.dll')}){
        $machine=Read-PeMachine $file.FullName
        $runtime+=@{file=$file.FullName.Substring($resolvedRoot.Length+1);machine=$machine;sha256=(Get-FileHash -LiteralPath $file.FullName).Hash}
    }
    foreach($file in Get-ChildItem -LiteralPath $editionRoot -File | Where-Object {$_.Extension -in @('.exe','.dll')}){
        $legacy+=@{file=$file.FullName.Substring($resolvedRoot.Length+1);machine=(Read-PeMachine $file.FullName)}
    }
}
$report=@{activeRuntime=$runtime;rootBinaries=$legacy;allActiveRuntimeAMD64=(@($runtime | Where-Object {$_.machine -ne 'AMD64'}).Count -eq 0)}
if($OutputPath){$report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $OutputPath -Encoding utf8}
$runtime | ForEach-Object {[pscustomobject]@{File=$_.file;Machine=$_.machine}} | Format-Table -AutoSize
if(-not $report.allActiveRuntimeAMD64){throw 'The active Client runtime contains a non-AMD64 binary.'}
Write-Output 'All active Client runtime binaries are AMD64. Root files, managed launcher flags and dynamic loads require separate review.'
