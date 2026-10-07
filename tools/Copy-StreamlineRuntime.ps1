[CmdletBinding()]
param([Parameter(Mandatory)][string]$ExecutableDirectory)
$ErrorActionPreference='Stop'
$repositoryPath=Split-Path $PSScriptRoot -Parent
$sdkPath=Join-Path $repositoryPath '.build\streamline-sdk-v2.14.1'
$destination=Join-Path $ExecutableDirectory 'Streamline'
$files=@('sl.interposer.dll','sl.common.dll','sl.dlss.dll','nvngx_dlss.dll')
# Validate the complete set before changing any destination file.
foreach($file in $files) {
    $path=Join-Path $sdkPath ('bin\x64\'+$file)
    $signature=Get-AuthenticodeSignature -LiteralPath $path
    if($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'NVIDIA') {throw ('Invalid NVIDIA signature: '+$file)}
}
$null=New-Item -ItemType Directory -Path $destination -Force
foreach($file in $files) {Copy-Item -LiteralPath (Join-Path $sdkPath ('bin\x64\'+$file)) -Destination (Join-Path $destination $file) -Force}
Copy-Item -LiteralPath (Join-Path $sdkPath 'license.txt') -Destination (Join-Path $destination 'Streamline-license.txt') -Force
Copy-Item -LiteralPath (Join-Path $sdkPath 'bin\x64\nvngx_dlss.license.txt') -Destination $destination -Force
Write-Output ('Installed verified NVIDIA DLSS runtime: '+$destination)
