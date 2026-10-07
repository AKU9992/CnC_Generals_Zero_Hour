[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$installerPath = Join-Path $PSScriptRoot 'vs_buildtools.exe'
$configurationPath = Join-Path $PSScriptRoot 'native-build.vsconfig'
Invoke-WebRequest -Uri 'https://aka.ms/vs/stable/vs_buildtools.exe' -OutFile $installerPath
$signature = Get-AuthenticodeSignature -LiteralPath $installerPath
if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'Microsoft Corporation') {
    throw 'The downloaded installer does not have a valid Microsoft signature.'
}
$installer = Start-Process -FilePath $installerPath -ArgumentList @(
    '--quiet', '--wait', '--norestart', '--config', ('"' + $configurationPath + '"')
) -WindowStyle Hidden -PassThru -Wait
if ($installer.ExitCode -eq 3010) {
    Write-Output 'Build tools installed; Windows reports that a restart is required. No restart was initiated.'
} elseif ($installer.ExitCode -ne 0) {
    throw "Build tools installer exited with code $($installer.ExitCode)."
} else {
    Write-Output 'Build tools installer completed successfully.'
}
