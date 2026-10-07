[CmdletBinding()]
param([switch]$ConfigureOnly, [switch]$Clean, [ValidateSet('ZeroHour','Generals')][string]$Edition='ZeroHour')
$ErrorActionPreference = 'Stop'
$repositoryPath = Split-Path $PSScriptRoot -Parent
$commit = 'b805c12ee1aedc0a4b241006803b8e04bbf288a6'
$sourcePath = Join-Path $repositoryPath ('.build\community-reference-x64\GeneralsGameCode-' + $commit)
$archivePath = Join-Path $repositoryPath '.build\community-reference.zip'
$buildPath = Join-Path $repositoryPath $(if($Edition -eq 'Generals'){'.build\game-generals-x64'}else{'.build\game-x64'})
$gameEdition=if($Edition -eq 'Generals'){'Generals'}else{'GeneralsMD'}
$buildTarget=if($Edition -eq 'Generals'){'g_generals'}else{'z_generals'}
if (-not (Test-Path -LiteralPath (Join-Path $sourcePath 'CMakeLists.txt'))) {
    if (-not (Test-Path -LiteralPath $archivePath)) { throw 'Download the pinned community reference first.' }
    Expand-Archive -LiteralPath $archivePath -DestinationPath (Split-Path $sourcePath -Parent) -Force
}
# Keep x86 and x64 sources, generated files and dependencies separate.
$resourcePath = Join-Path $sourcePath ($gameEdition+'\Code\Main\RTS.RC')
[IO.File]::WriteAllText($resourcePath, [IO.File]::ReadAllText($resourcePath).Replace('afxres.h', 'winres.h'))
$rootCmake = Join-Path $sourcePath 'CMakeLists.txt'
$rootText = [IO.File]::ReadAllText($rootCmake)
$rootText = $rootText.Replace('if((WIN32 OR "${CMAKE_SYSTEM}" MATCHES "Windows") AND ${CMAKE_SIZEOF_VOID_P} EQUAL 4)', 'if(WIN32 OR "${CMAKE_SYSTEM}" MATCHES "Windows")')
[IO.File]::WriteAllText($rootCmake, $rootText)
$dependencyRoot = Join-Path $repositoryPath '.build\community-baseline-ninja\_deps'
# Old immediate FetchContent_Populate calls ignore SOURCE_DIR overrides.
# Use the already downloaded, immutable pinned sources directly.
foreach ($module in @('lzhl', 'zlib')) {
    $modulePath = Join-Path $sourcePath ('cmake\' + $module + '.cmake')
    $text = [IO.File]::ReadAllText($modulePath)
    $text = [regex]::Replace($text, '(?s)FetchContent_Populate\(.*?\n\)', '')
    if ($module -eq 'lzhl') {
        $pinnedPath = Join-Path $dependencyRoot 'lzhl-src\CompLibHeader'
        $text = [regex]::Replace($text, 'set\(LZHL_DIR [^\r\n]+', ('set(LZHL_DIR "' + $pinnedPath.Replace('\','/') + '")'))
    } else {
        $pinnedPath = Join-Path $repositoryPath '.build\community-baseline-ninja\Core\Libraries\Source\Compression\_deps\zlib-1.1.4-src\ZLib'
        $text = [regex]::Replace($text, 'set\(ZLIB_DIR [^\r\n]+', ('set(ZLIB_DIR "' + $pinnedPath.Replace('\','/') + '")'))
    }
    [IO.File]::WriteAllText($modulePath, $text)
}
$dx8Path = Join-Path $buildPath 'dx8-sdk'
if (-not (Test-Path -LiteralPath $dx8Path)) {
    $null = New-Item -ItemType Directory -Path $buildPath -Force
    Copy-Item -LiteralPath (Join-Path $dependencyRoot 'dx8-src') -Destination $dx8Path -Recurse
}
$dx8Cmake = Join-Path $dx8Path 'CMakeLists.txt'
$dx8Text = [IO.File]::ReadAllText($dx8Cmake)
$dx8Text = $dx8Text.Replace('INTERFACE d3d8 dinput8 dxguid', 'INTERFACE dinput8 dxguid')
$dx8Text = $dx8Text.Replace('target_link_libraries(d3d8lib INTERFACE d3dx8)', '# x64 D3DX compatibility is linked separately.')
$dx8Text = [regex]::Replace($dx8Text, '(?m)^\s*target_link_directories\(d3d8lib BEFORE INTERFACE \$\{CMAKE_CURRENT_SOURCE_DIR\}\)\s*$', '')
Copy-Item -LiteralPath (Join-Path $repositoryPath 'renderer\D3DX8CompatibilityX64.cpp') -Destination (Join-Path $dx8Path 'D3DX8CompatibilityX64.cpp') -Force
if (-not $dx8Text.Contains('add_library(d3dx8_compat')) {
    $dx8Text += @'

add_library(d3dx8_compat STATIC D3DX8CompatibilityX64.cpp)
target_include_directories(d3dx8_compat PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(d3dx8_compat PRIVATE NOMINMAX)
target_compile_features(d3dx8_compat PRIVATE cxx_std_17)
target_link_libraries(d3d8lib INTERFACE d3dx8_compat)
'@
}
[IO.File]::WriteAllText($dx8Cmake, $dx8Text)
& (Join-Path $PSScriptRoot 'Apply-ExperimentalFpsPatch.ps1') -SourcePath $sourcePath -GameEdition $gameEdition
& (Join-Path $PSScriptRoot 'Apply-SystemD3D8Patch.ps1') -SourcePath $sourcePath
& (Join-Path $PSScriptRoot 'Apply-NeuralAaMenuPatch.ps1') -SourcePath $sourcePath -GameEdition $gameEdition
& (Join-Path $PSScriptRoot 'Apply-NeuralGamePatch.ps1') -SourcePath $sourcePath
& (Join-Path $PSScriptRoot 'Apply-NeuralAaButtonsPatch.ps1') -SourcePath $sourcePath -GameEdition $gameEdition
& (Join-Path $PSScriptRoot 'Apply-NeuralAaTogglePatch.ps1') -SourcePath $sourcePath -GameEdition $gameEdition
& (Join-Path $PSScriptRoot 'Apply-RendererLifecyclePatch.ps1') -SourcePath $sourcePath -GameEdition $gameEdition
& (Join-Path $PSScriptRoot 'Apply-X64GamePatch.ps1') -SourcePath $sourcePath -GameEdition $gameEdition
& (Join-Path $PSScriptRoot 'Apply-X64AudioPatch.ps1') -SourcePath $sourcePath
& (Join-Path $PSScriptRoot 'Apply-LegacyRegistryViewPatch.ps1') -SourcePath $sourcePath
& (Join-Path $PSScriptRoot 'Apply-X64TextureDiagnostics.ps1') -SourcePath $sourcePath
if($Edition -eq 'Generals'){ & (Join-Path $PSScriptRoot 'Apply-GeneralsStartupPatch.ps1') -SourcePath $sourcePath }
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$cmake = Join-Path $installation 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ninja = Join-Path $installation 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
$arguments = @()
foreach ($dependency in @('dx8', 'gamespy', 'lzhl', 'stb')) {
    $dependencySource = Join-Path $dependencyRoot ($dependency + '-src')
    if ($dependency -eq 'dx8') { $dependencySource = $dx8Path }
    if (-not (Test-Path -LiteralPath $dependencySource)) { throw ('Missing pinned dependency: ' + $dependency) }
    $arguments += ('-DFETCHCONTENT_SOURCE_DIR_' + $dependency.ToUpperInvariant() + '="' + $dependencySource + '"')
}
$null = New-Item -ItemType Directory -Path $buildPath -Force
$commandPath = Join-Path $buildPath 'configure.cmd'
$editionFlags=if($Edition -eq 'Generals'){'-DRTS_BUILD_GENERALS=ON -DRTS_BUILD_ZEROHOUR=OFF'}else{'-DRTS_BUILD_GENERALS=OFF -DRTS_BUILD_ZEROHOUR=ON'}
$command = '"' + $cmake + '" -S "' + $sourcePath + '" -B "' + $buildPath + '" -G Ninja -DCMAKE_MAKE_PROGRAM="' + $ninja + '" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_SCAN_FOR_MODULES=OFF '+$editionFlags+' -DRTS_BUILD_CORE_TOOLS=OFF -DRTS_BUILD_GENERALS_TOOLS=OFF -DRTS_BUILD_ZEROHOUR_TOOLS=OFF -DCMAKE_POLICY_DEFAULT_CMP0169=OLD ' + ($arguments -join ' ')
[IO.File]::WriteAllLines($commandPath, @('@echo off', 'set "VSLANG=1033"', ('call "' + $installation + '\VC\Auxiliary\Build\vcvars64.bat"'), 'if errorlevel 1 exit /b %errorlevel%', $command, 'exit /b %errorlevel%'), [Text.Encoding]::Default)
& cmd.exe /d /c $commandPath
if ($LASTEXITCODE -ne 0) { throw 'x64 configuration failed.' }
if (-not $ConfigureOnly) {
    $commandPath = Join-Path $buildPath 'build.cmd'
    # Localized cl.exe /showIncludes is not reliably recognized by this Ninja.
    # Rebuild all objects after header changes to prevent mixed x86/x64 ABI.
    $headerFiles = Get-ChildItem -LiteralPath $sourcePath -Recurse -File | Where-Object { $_.Extension -in @('.h','.hpp','.inl') } | Sort-Object FullName
    $headerHashes = foreach ($headerFile in $headerFiles) { $headerFile.FullName + ':' + (Get-FileHash -LiteralPath $headerFile.FullName -Algorithm SHA256).Hash }
    $hashAlgorithm = [Security.Cryptography.SHA256]::Create()
    try { $headerSignature = [BitConverter]::ToString($hashAlgorithm.ComputeHash([Text.Encoding]::UTF8.GetBytes($headerHashes -join "`n"))) } finally { $hashAlgorithm.Dispose() }
    $signaturePath = Join-Path $buildPath 'headers.sha256'
    $headersChanged = -not (Test-Path -LiteralPath $signaturePath) -or [IO.File]::ReadAllText($signaturePath) -ne $headerSignature
    $cleanFlag = if ($Clean -or $headersChanged) { ' --clean-first' } else { '' }
    [IO.File]::WriteAllLines($commandPath, @('@echo off', 'set "VSLANG=1033"', ('call "' + $installation + '\VC\Auxiliary\Build\vcvars64.bat"'), 'if errorlevel 1 exit /b %errorlevel%', ('"' + $cmake + '" --build "' + $buildPath + '"' + $cleanFlag + ' --target '+$buildTarget+' --parallel 4'), 'exit /b %errorlevel%'), [Text.Encoding]::Default)
    & cmd.exe /d /c $commandPath
    if ($LASTEXITCODE -ne 0) { throw 'x64 game build failed; inspect compiler/linker diagnostics.' }
    [IO.File]::WriteAllText($signaturePath, $headerSignature)
}
