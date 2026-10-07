[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$testDirectory = Join-Path $PSScriptRoot ('.resolution-test-' + [Guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $testDirectory
$optionsPath = Join-Path $testDirectory 'Options.ini'
$scriptPath = Join-Path $PSScriptRoot 'Set-ZeroHourResolution.ps1'
$encoding = [Text.Encoding]::GetEncoding(28591)
function Assert-BytesEqual($expected, $actual, $message) {
    if ([Convert]::ToBase64String($expected) -cne [Convert]::ToBase64String($actual)) { throw $message }
}
try {
    # A non-ASCII byte, mixed whitespace and comments must survive profile changes.
    $original = $encoding.GetBytes("; custom " + [char]233 + "`r`n  Resolution = 1280 1024`r`nMusicVolume = 37`r`n")
    [IO.File]::WriteAllBytes($optionsPath, $original)
    $result = & $scriptPath -Profile UltraWide -OptionsPath $optionsPath
    Assert-BytesEqual $original ([IO.File]::ReadAllBytes($result.BackupPath)) 'Backup lost original bytes.'
    $expected = $encoding.GetBytes($encoding.GetString($original).Replace('1280 1024', '3440 1440'))
    Assert-BytesEqual $expected ([IO.File]::ReadAllBytes($optionsPath)) 'UltraWide changed unrelated settings.'
    $null = & $scriptPath -Profile FourK -OptionsPath $optionsPath
    $expected = $encoding.GetBytes($encoding.GetString($original).Replace('1280 1024', '3840 2160'))
    Assert-BytesEqual $expected ([IO.File]::ReadAllBytes($optionsPath)) 'FourK profile is incorrect.'
    $null = & $scriptPath -Profile Restore -OptionsPath $optionsPath -BackupPath $result.BackupPath
    Assert-BytesEqual $original ([IO.File]::ReadAllBytes($optionsPath)) 'Restore did not recover original bytes.'
    $null = & $scriptPath -Profile FourK -OptionsPath $optionsPath -WhatIf
    Assert-BytesEqual $original ([IO.File]::ReadAllBytes($optionsPath)) 'WhatIf modified the file.'

    [IO.File]::WriteAllBytes($optionsPath, $encoding.GetBytes('MusicVolume = 37'))
    $null = & $scriptPath -Profile UltraWide -OptionsPath $optionsPath
    $expected = $encoding.GetBytes("MusicVolume = 37`nResolution = 3440 1440`n")
    Assert-BytesEqual $expected ([IO.File]::ReadAllBytes($optionsPath)) 'Missing entry was not appended correctly.'

    foreach ($invalidText in @("Resolution = 1 1`nResolution = 2 2", "R`0e`0s`0")) {
        $invalid = $encoding.GetBytes($invalidText)
        [IO.File]::WriteAllBytes($optionsPath, $invalid)
        $rejected = $false
        try { $null = & $scriptPath -Profile FourK -OptionsPath $optionsPath } catch { $rejected = $true }
        if (-not $rejected) { throw 'An ambiguous or unsupported options file was accepted.' }
        Assert-BytesEqual $invalid ([IO.File]::ReadAllBytes($optionsPath)) 'Rejected input was modified.'
    }
    Write-Output 'PASS: UltraWide, FourK, backups, restore, WhatIf, missing entry, duplicate entry and unsupported encoding.'
} finally {
    $resolvedDirectory = [IO.Path]::GetFullPath($testDirectory)
    $allowedParent = [IO.Path]::GetFullPath($PSScriptRoot) + [IO.Path]::DirectorySeparatorChar
    if (-not $resolvedDirectory.StartsWith($allowedParent, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Test cleanup path is outside the tools directory.'
    }
    Get-ChildItem -LiteralPath $resolvedDirectory -File | Remove-Item
    Remove-Item -LiteralPath $resolvedDirectory
}
