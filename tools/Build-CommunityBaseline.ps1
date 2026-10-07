[CmdletBinding()]
param([switch]$ConfigureOnly, [switch]$ExperimentalFps, [switch]$DlaaMenuPreview)
$ErrorActionPreference = 'Stop'

# Build a separate reference executable. This does not replace our source tree
# or install anything into the user's game directory.
$referenceCommit = 'b805c12ee1aedc0a4b241006803b8e04bbf288a6'
$repositoryPath = Split-Path $PSScriptRoot -Parent
$referenceRoot = Join-Path $repositoryPath '.build\community-reference'
$sourcePath = Join-Path $referenceRoot ('GeneralsGameCode-' + $referenceCommit)
$archivePath = Join-Path $repositoryPath '.build\community-reference.zip'
$buildPath = Join-Path $repositoryPath '.build\community-baseline-ninja'
if (-not (Test-Path -LiteralPath (Join-Path $sourcePath 'CMakeLists.txt'))) {
    $null = New-Item -ItemType Directory -Path $referenceRoot -Force
    Invoke-WebRequest -Uri ('https://codeload.github.com/TheSuperHackers/GeneralsGameCode/zip/' + $referenceCommit) -OutFile $archivePath
    Expand-Archive -LiteralPath $archivePath -DestinationPath $referenceRoot
}
$menuSourcePath = Join-Path $sourcePath 'GeneralsMD\Code\GameEngine\Source\GameClient\GUI\GUICallbacks\Menus\OptionsMenu.cpp'
$hasDlaaMenu = [IO.File]::ReadAllText($menuSourcePath).Contains('// generals-mods DLAA selection')
$requireCleanBuild = $DlaaMenuPreview.IsPresent -ne $hasDlaaMenu
if ($hasDlaaMenu -and -not $DlaaMenuPreview) {
    # Restore the two UI/interface files from the pinned archive when returning
    # to the stable FPS build. Preserve our FPS/system-D3D8 patches elsewhere.
    $archive = [IO.Compression.ZipFile]::OpenRead($archivePath)
    try {
        foreach ($relativePath in @('Core/GameEngine/Include/GameClient/Display.h', 'GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/OptionsMenu.cpp')) {
            $entry = $archive.GetEntry('GeneralsGameCode-' + $referenceCommit + '/' + $relativePath)
            if (-not $entry) { throw 'Cannot restore the pinned stable interface.' }
            $reader = [IO.StreamReader]::new($entry.Open())
            try { $originalText = $reader.ReadToEnd() } finally { $reader.Dispose() }
            [IO.File]::WriteAllText((Join-Path $sourcePath $relativePath), $originalText)
        }
    } finally { $archive.Dispose() }
}

# The bundled Git fails to create origin/HEAD when cloning these repositories.
# Fetch the same pinned sources as archives, only in the reference checkout.
foreach ($module in Get-ChildItem -LiteralPath (Join-Path $sourcePath 'cmake') -Filter '*.cmake') {
    $originalText = [IO.File]::ReadAllText($module.FullName)
    $patchedText = [regex]::Replace($originalText,
        'GIT_REPOSITORY\s+https://github\.com/([^\s]+?)\s+GIT_TAG\s+([a-f0-9]{40})',
        { param($match)
            $projectName = $match.Groups[1].Value -replace '\.git$', ''
            'URL https://codeload.github.com/' + $projectName + '/zip/' + $match.Groups[2].Value + "`n    DOWNLOAD_EXTRACT_TIMESTAMP TRUE"
        })
    if ($originalText -ne $patchedText) { [IO.File]::WriteAllText($module.FullName, $patchedText) }
}

# The game executable resource contains an icon and a DPI manifest. It does
# not use MFC resources, so the Windows SDK resource header is sufficient.
$resourcePath = Join-Path $sourcePath 'GeneralsMD\Code\Main\RTS.RC'
$resourceText = [IO.File]::ReadAllText($resourcePath)
$patchedResourceText = $resourceText.Replace('afxres.h', 'winres.h')
if ($resourceText -ne $patchedResourceText) { [IO.File]::WriteAllText($resourcePath, $patchedResourceText) }

$vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installationPath = & $vswherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installationPath) { throw 'MSVC x86/x64 tools are required.' }
$cmakePath = Join-Path $installationPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path -LiteralPath $cmakePath)) { throw 'The Visual Studio CMake component is required.' }
$ninjaPath = Join-Path $installationPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
$environmentScript = Join-Path $installationPath 'VC\Auxiliary\Build\vcvars32.bat'
$null = New-Item -ItemType Directory -Path $buildPath -Force
$commandPath = Join-Path $buildPath 'configure.cmd'
$configureCommand = '"' + $cmakePath + '" -S "' + $sourcePath + '" -B "' + $buildPath + '" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="' + $ninjaPath + '" -DRTS_BUILD_GENERALS=OFF -DRTS_BUILD_CORE_TOOLS=OFF -DRTS_BUILD_ZEROHOUR_TOOLS=OFF -DCMAKE_POLICY_DEFAULT_CMP0169=OLD'
[IO.File]::WriteAllLines($commandPath, @('@echo off', ('call "' + $environmentScript + '"'), 'if errorlevel 1 exit /b %errorlevel%', $configureCommand, 'exit /b %errorlevel%'), [Text.Encoding]::Default)
& cmd.exe /d /c $commandPath
if ($LASTEXITCODE -ne 0) { throw 'Reference configuration failed.' }
if ($ExperimentalFps) {
    & (Join-Path $PSScriptRoot 'Apply-ExperimentalFpsPatch.ps1') -SourcePath $sourcePath
    & (Join-Path $PSScriptRoot 'Apply-SystemD3D8Patch.ps1') -SourcePath $sourcePath
}
if ($DlaaMenuPreview) {
    & (Join-Path $PSScriptRoot 'Apply-DlaaMenuPatch.ps1') -SourcePath $sourcePath
}
if (-not $ConfigureOnly) {
    $commandPath = Join-Path $buildPath 'build.cmd'
    $cleanFlag = if ($requireCleanBuild) { ' --clean-first' } else { '' }
    [IO.File]::WriteAllLines($commandPath, @('@echo off', ('call "' + $environmentScript + '"'), 'if errorlevel 1 exit /b %errorlevel%', ('"' + $cmakePath + '" --build "' + $buildPath + '"' + $cleanFlag + ' --target z_generals --parallel 4'), 'exit /b %errorlevel%'), [Text.Encoding]::Default)
    & cmd.exe /d /c $commandPath
    if ($LASTEXITCODE -ne 0) { throw 'Reference game build failed; inspect the compiler output.' }
}
Write-Output ('Reference source: ' + $referenceCommit)
Write-Output ('Reference build directory: ' + $buildPath)
