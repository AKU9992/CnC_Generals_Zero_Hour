[CmdletBinding()]
param([string]$GameDirectory = 'E:\C&C ZH GPTMOD\CaCGZH')
$ErrorActionPreference = 'Stop'
$outputPath = Join-Path (Split-Path $PSScriptRoot -Parent) '.build\dds-texture-samples'
$null = New-Item -ItemType Directory -Path $outputPath -Force
function Read-BigEndian32($reader) {
    $bytes = $reader.ReadBytes(4)
    if ($bytes.Length -ne 4) { throw 'Truncated BIG archive index.' }
    [Array]::Reverse($bytes)
    [BitConverter]::ToUInt32($bytes, 0)
}
foreach ($archiveName in @('TexturesZH.big', 'TerrainZH.big')) {
    $stream = [IO.File]::OpenRead((Join-Path $GameDirectory $archiveName))
    $reader = [IO.BinaryReader]::new($stream)
    try {
        $magic = [Text.Encoding]::ASCII.GetString($reader.ReadBytes(4))
        if ($magic -notin @('BIGF','BIG4')) { throw 'Unsupported BIG archive.' }
        $null = $reader.ReadBytes(4)
        $count = Read-BigEndian32 $reader
        $indexEnd = Read-BigEndian32 $reader
        if ($count -gt 100000 -or $indexEnd -gt $stream.Length) { throw 'Invalid archive index.' }
        $saved = 0
        for ($i = 0; $i -lt $count -and $saved -lt 3; ++$i) {
            $offset = Read-BigEndian32 $reader
            $size = Read-BigEndian32 $reader
            $nameBytes = [Collections.Generic.List[byte]]::new()
            do {
                $value = $reader.ReadByte()
                if ($value) { $nameBytes.Add($value) }
                if ($nameBytes.Count -gt 1024) { throw 'Invalid archive filename.' }
            } while ($value)
            $name = [Text.Encoding]::ASCII.GetString($nameBytes.ToArray())
            if (-not $name.EndsWith('.dds', [StringComparison]::OrdinalIgnoreCase)) { continue }
            if ([uint64]$offset + $size -gt $stream.Length -or $size -gt 16777216) { throw 'Invalid texture range.' }
            $indexPosition = $stream.Position
            $stream.Position = $offset
            $data = $reader.ReadBytes($size)
            $stream.Position = $indexPosition
            if ([Text.Encoding]::ASCII.GetString($data,0,4) -ne 'DDS ') { continue }
            $destination = Join-Path $outputPath ($archiveName + '-' + [IO.Path]::GetFileName($name))
            [IO.File]::WriteAllBytes($destination, $data)
            Write-Output $destination
            ++$saved
        }
        if (-not $saved -and $archiveName -eq 'TexturesZH.big') { throw "No raw DDS samples found in $archiveName" }
    } finally { $reader.Dispose() }
}
