[CmdletBinding()]
param([ValidateRange(5,40)][int]$Seconds=10)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$results=@()
@{cases=@();complete=$false} | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $root '.build/native12/game-validation.json') -Encoding UTF8
foreach($edition in @('Generals','ZeroHour')){
    foreach($mode in @('Off','DLAA','DLSSQuality')){
        $arguments=@{Edition=$edition;NeuralMode=$mode;Seconds=$Seconds}
        if($edition -eq 'Generals'){$arguments.ShellMap='maps\Tournament Lake\Tournament Lake.map'}
        & (Join-Path $PSScriptRoot 'Test-NativeW3DGame.ps1') @arguments
        $result=Get-Content -Raw (Join-Path $root ('.build/native12/game-'+$edition+'-'+$mode+'.json')) | ConvertFrom-Json
        if(-not $result.water -or -not $result.stencilShadow){throw ('Water/stencil pass missing: '+$edition+' '+$mode)}
        $results+=$result
        @{cases=$results;complete=($results.Count -eq 6)} | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $root '.build/native12/game-validation.json') -Encoding UTF8
    }
}
