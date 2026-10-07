[CmdletBinding(SupportsShouldProcess)]
param([string]$GameDirectory='E:\C&C ZH GPTMOD\CaCGZH')
$ErrorActionPreference='Stop'
$path=Join-Path $GameDirectory 'Data\INI\GameData.ini'
$text=[IO.File]::ReadAllText($path)
# This revision is idempotent: do not multiply again on repeated installs.
if($text.Contains('; GeneralsGPT camera revision 2026-10-07')) {Write-Output 'Camera revision already installed.';return}
$before=@{};$after=@{}
foreach($key in @('CameraHeight','MaxCameraHeight')) {
    $match=[regex]::Match($text,'(?m)^(\s*'+$key+'\s*=\s*)([\d.]+)')
    if(-not $match.Success) {throw ('Missing camera setting: '+$key)}
    $old=[double]::Parse($match.Groups[2].Value,[Globalization.CultureInfo]::InvariantCulture)
    $new=$old*1.25
    $before[$key]=$old;$after[$key]=$new
    $text=$text.Remove($match.Groups[2].Index,$match.Groups[2].Length).Insert($match.Groups[2].Index,$new.ToString('0.0##',[Globalization.CultureInfo]::InvariantCulture))
}
if($PSCmdlet.ShouldProcess($path,'Increase camera default and maximum height by 25 percent')) {
    Copy-Item -LiteralPath $path -Destination ($path+'.'+[Guid]::NewGuid().ToString('N')+'.bak')
    [IO.File]::WriteAllText($path,$text+"`r`n; GeneralsGPT camera revision 2026-10-07`r`n")
    @{before=$before;after=$after;path=$path} | ConvertTo-Json | Set-Content (Join-Path (Split-Path $PSScriptRoot -Parent) '.build\camera-height-install.json')
    Write-Output ($after | ConvertTo-Json)
}
