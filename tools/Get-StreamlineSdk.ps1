[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repositoryPath = Split-Path $PSScriptRoot -Parent
$sdkDirectory = Join-Path $repositoryPath '.build\streamline-sdk-v2.14.1'
$archivePath = Join-Path $repositoryPath '.build\streamline-sdk-v2.14.1.zip'
$expectedSha256 = '92c4d954631a1710da86ca3fa8d5034f2b9503838c95fc4ae977ae149319781b'
if (-not (Test-Path -LiteralPath $archivePath)) {
    Invoke-WebRequest -Uri 'https://github.com/NVIDIA-RTX/Streamline/releases/download/v2.14.1/streamline-sdk-v2.14.1.zip' -OutFile $archivePath
}
if ((Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash -ne $expectedSha256) {
    throw 'Streamline SDK archive checksum does not match the official release.'
}
if (-not (Test-Path -LiteralPath (Join-Path $sdkDirectory 'include\sl.h'))) {
    Expand-Archive -LiteralPath $archivePath -DestinationPath $sdkDirectory -Force
}
$interposerPath = Join-Path $sdkDirectory 'bin\x64\sl.interposer.dll'
$signature = Get-AuthenticodeSignature -LiteralPath $interposerPath
if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'NVIDIA') {
    throw 'The Streamline interposer does not have a valid NVIDIA digital signature.'
}
Write-Output ('Verified Streamline SDK 2.14.1: ' + $sdkDirectory)
