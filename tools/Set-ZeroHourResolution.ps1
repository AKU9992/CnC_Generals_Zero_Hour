[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)]
    [ValidateSet('UltraWide', 'FourK', 'Restore')]
    [string]$Profile,
    [string]$OptionsPath = (Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Command and Conquer Generals Zero Hour Data\Options.ini'),
    [string]$BackupPath
)

$ErrorActionPreference = 'Stop'
if (Get-Process -Name generals,game -ErrorAction SilentlyContinue) {
    throw 'Close Generals / Zero Hour before changing its options.'
}
$optionsFile = Get-Item -LiteralPath $OptionsPath
if ($optionsFile.PSIsContainer) { throw 'OptionsPath must be a file.' }
$OptionsPath = $optionsFile.FullName
$originalBytes = [IO.File]::ReadAllBytes($OptionsPath)

if ($Profile -eq 'Restore') {
    if (-not $BackupPath) { throw 'Restore requires BackupPath from a previous run.' }
    $backupFile = Get-Item -LiteralPath $BackupPath
    if ($backupFile.PSIsContainer -or $backupFile.FullName -eq $OptionsPath) {
        throw 'BackupPath must be a different file.'
    }
    $updatedBytes = [IO.File]::ReadAllBytes($backupFile.FullName)
} else {
    if ($originalBytes -contains 0) { throw 'UTF-16 or binary options files are not supported; no changes made.' }
    # Map bytes one-to-one, preserving existing encoding, BOM and unrelated settings.
    $byteEncoding = [Text.Encoding]::GetEncoding(28591)
    $optionsText = $byteEncoding.GetString($originalBytes)
    $pattern = '(?m)^([ \t]*Resolution[ \t]*=[ \t]*)[^\r\n]*'
    $matches = [regex]::Matches($optionsText, $pattern)
    if ($matches.Count -gt 1) { throw 'Multiple Resolution entries found; no changes made.' }
    $resolution = if ($Profile -eq 'UltraWide') { '3440 1440' } else { '3840 2160' }
    if ($matches.Count -eq 1) {
        $updatedText = [regex]::Replace($optionsText, $pattern, ('${1}' + $resolution))
    } else {
        $newLine = if ($optionsText.Contains("`r`n")) { "`r`n" } else { "`n" }
        $separator = if ($optionsText.Length -gt 0 -and -not $optionsText.EndsWith("`n")) { $newLine } else { '' }
        $updatedText = $optionsText + $separator + 'Resolution = ' + $resolution + $newLine
    }
    $updatedBytes = $byteEncoding.GetBytes($updatedText)
}

if ($PSCmdlet.ShouldProcess($OptionsPath, "Apply $Profile resolution profile with a backup")) {
    $newBackupPath = $OptionsPath + '.generals-mods-' + [Guid]::NewGuid().ToString('N') + '.bak'
    [IO.File]::Copy($OptionsPath, $newBackupPath, $false)
    [IO.File]::WriteAllBytes($OptionsPath, $updatedBytes)
    [PSCustomObject]@{
        Profile = $Profile
        Resolution = $resolution
        OptionsPath = $OptionsPath
        BackupPath = $newBackupPath
    }
}
