[CmdletBinding()]
param([string]$GameDirectory = 'E:\C&C ZH GPTMOD\CaCGZH', [string]$GeneralsDirectory = 'E:\C&C ZH GPTMOD\CaCG')
$ErrorActionPreference = 'Stop'
$repositoryPath = Split-Path $PSScriptRoot -Parent
$names = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$ddsCount = 0; $tgaCount = 0
$invalid = [Collections.Generic.List[string]]::new()
function Read-BE32($reader) {
    $b = $reader.ReadBytes(4)
    if ($b.Length -ne 4) { throw 'Truncated BIG index.' }
    [Array]::Reverse($b)
    [BitConverter]::ToUInt32($b,0)
}
foreach ($directory in @($GameDirectory,$GeneralsDirectory)) {
    foreach ($archive in Get-ChildItem -LiteralPath $directory -Filter '*.big') {
        $stream = [IO.File]::OpenRead($archive.FullName)
        $reader = [IO.BinaryReader]::new($stream)
        try {
            $magic = [Text.Encoding]::ASCII.GetString($reader.ReadBytes(4))
            if ($magic -notin @('BIGF','BIG4')) { continue }
            $null = $reader.ReadBytes(4)
            $count = Read-BE32 $reader; $indexEnd = Read-BE32 $reader
            if ($count -gt 100000 -or $indexEnd -gt $stream.Length) { throw 'Invalid BIG index.' }
            for ($i = 0; $i -lt $count; ++$i) {
                $offset = Read-BE32 $reader; $size = Read-BE32 $reader
                $bytes = [Collections.Generic.List[byte]]::new()
                do {
                    $byte = $reader.ReadByte()
                    if ($byte) { $bytes.Add($byte) }
                    if ($bytes.Count -gt 1024) { throw 'Invalid BIG filename.' }
                } while ($byte)
                $name = [Text.Encoding]::ASCII.GetString($bytes.ToArray())
                $extension = [IO.Path]::GetExtension($name).ToLowerInvariant()
                if ($extension -notin @('.dds','.tga')) { continue }
                $null = $names.Add([IO.Path]::GetFileName($name))
                if ([uint64]$offset + $size -gt $stream.Length) { throw 'Invalid texture range.' }
                $position = $stream.Position; $stream.Position = $offset
                $header = $reader.ReadBytes([Math]::Min(128,$size))
                $stream.Position = $position
                if ($extension -eq '.dds') {
                    ++$ddsCount
                    if ($header.Length -lt 128 -or [Text.Encoding]::ASCII.GetString($header,0,4) -ne 'DDS ' -or
                        [BitConverter]::ToUInt32($header,4) -ne 124 -or [BitConverter]::ToUInt32($header,76) -ne 32 -or
                        -not [BitConverter]::ToUInt32($header,12) -or -not [BitConverter]::ToUInt32($header,16)) {
                        $invalid.Add($archive.Name + ':' + $name)
                    }
                } else {
                    ++$tgaCount
                    if ($header.Length -lt 18 -or -not [BitConverter]::ToUInt16($header,12) -or -not [BitConverter]::ToUInt16($header,14)) {
                        $invalid.Add($archive.Name + ':' + $name)
                    }
                }
            }
        } finally { $reader.Dispose() }
    }
}
$probe = Get-Content -LiteralPath (Join-Path $repositoryPath '.build\x64-game-startup.json') -Raw | ConvertFrom-Json
$failures = @(Get-Content -LiteralPath (Join-Path $GameDirectory 'GeneralsTextureFailures.log') | Where-Object { $_.StartsWith("PID $($probe.processId): Missing") })
$absent = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($failure in $failures) {
    if ($failure -match 'file (.+)$') {
        $name = [IO.Path]::GetFileName($Matches[1])
        if (-not $names.Contains($name) -and -not $names.Contains([IO.Path]::ChangeExtension($name,'.dds'))) { $null = $absent.Add($name) }
    }
}
$result = [ordered]@{
    ddsHeadersChecked = $ddsCount
    tgaHeadersChecked = $tgaCount
    invalidHeaders = @($invalid.ToArray())
    latestProbeProcessId = $probe.processId
    missingTexturesInProbe = $failures.Count
    referencesAbsentFromInstalledArchives = @($absent)
    failedReferencesPresentInArchives = $failures.Count - $absent.Count
}
$json = $result | ConvertTo-Json
$json | Set-Content -LiteralPath (Join-Path $repositoryPath '.build\texture-archive-coverage.json') -Encoding UTF8
Write-Output $json
if ($invalid.Count -or $result.failedReferencesPresentInArchives) { throw 'Texture archive/header coverage failed.' }
