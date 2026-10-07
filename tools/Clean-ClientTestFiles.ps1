[CmdletBinding()]
param([string]$GameRoot='E:\C&C ZH GPTMOD')
$ErrorActionPreference='Stop'
$repository=Split-Path $PSScriptRoot -Parent
$backup=Join-Path $repository ('.build\client-release\removed-tests-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
$removed=[Collections.Generic.List[string]]::new()
foreach($edition in @('CaCG','CaCGZH')) {
    $game=[IO.Path]::GetFullPath((Join-Path $GameRoot $edition)).TrimEnd('\')
    $validation=Get-Content (Join-Path $repository ('.build\client-release\'+$edition+'-shortcut-validation.json')) -Raw|ConvertFrom-Json
    if($validation.executableSha256 -ne (Get-FileHash (Join-Path $game 'Client\generals-client.exe')).Hash -or -not $validation.gracefulExitVerified){throw 'A current release validation is required before cleanup.'}
    $items=@(Get-ChildItem -LiteralPath $game -File|Where-Object {
        $_.Name -match '^Test-.*\.cmd(?:\..*\.bak)?$|^generalszh-(?:fps|d3d12)-test\.exe(?:\..*\.bak)?$|\.bak$|^Generals.*\.log$|^ReleaseCrashInfo-x64\.txt$|^dbghelp\.dll\.win11-backup$'
    })
    foreach($name in @('GeneralsGPT-x64','GeneralsGPT-x64-before-dlss')) {
        $path=Join-Path $game $name
        if(Test-Path -LiteralPath $path){$items+=Get-Item -LiteralPath $path}
    }
    foreach($item in $items) {
        # Resolve and constrain every recursive operation to the named game root.
        $resolved=[IO.Path]::GetFullPath($item.FullName)
        if(-not $resolved.StartsWith($game+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Cleanup target escapes the game directory.'}
        if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Refusing cleanup of a reparse point.'}
        $destination=Join-Path $backup ($edition+'\'+$item.Name)
        if(-not [IO.Path]::GetFullPath($destination).StartsWith([IO.Path]::GetFullPath($backup)+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Backup target escapes the backup directory.'}
        $null=New-Item -ItemType Directory -Path (Split-Path $destination -Parent) -Force
        Copy-Item -LiteralPath $resolved -Destination $destination -Recurse -Force
        Remove-Item -LiteralPath $resolved -Recurse -Force
        $removed.Add($resolved)
    }
}
$removed|ConvertTo-Json|Set-Content (Join-Path $backup 'removed-files.json') -Encoding UTF8
Write-Output ('Removed '+$removed.Count+' obsolete test entries from game directories; recovery backup: '+$backup)
