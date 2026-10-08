param([Parameter(Mandatory=$true)][string]$SourcePath,[ValidateSet('Generals','GeneralsMD')][string]$GameEdition='GeneralsMD')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$expected=Join-Path $root '.build/community-reference-x64/GeneralsGameCode-b805c12ee1aedc0a4b241006803b8e04bbf288a6'
if([IO.Path]::GetFullPath($SourcePath).TrimEnd('\','/') -ne [IO.Path]::GetFullPath($expected).TrimEnd('\','/')){throw 'Only the pinned isolated game sources may be patched.'}
$path=Join-Path $SourcePath ($GameEdition+'/Code/GameEngine/Source/Common/GlobalData.cpp')
$text=[IO.File]::ReadAllText($path)
if(-not $text.Contains('GENERALS_TEST_USER_DATA')){
    if($GameEdition -eq 'GeneralsMD'){
        $anchor='m_userDataDir = BuildUserDataPathFromRegistry();'
        if(-not $text.Contains($anchor)){throw 'Pinned Zero Hour user directory initialization changed.'}
        $text=$text.Replace($anchor,$anchor+@'

    if (getenv("GENERALS_TEST_QUIT_SECONDS")) {
        const char* directory=getenv("GENERALS_TEST_USER_DATA");
        if (directory && directory[0]) m_userDataDir=directory;
    }
'@)
    }else{
    $pattern='(?s)(AsciiString GlobalData::getPath_UserData\(\) const\s*\{)\s*return m_userDataDir;'
    if(-not [regex]::IsMatch($text,$pattern)){throw 'Pinned GlobalData user directory function changed.'}
    $replacement=@'
$1
    // Automated native renderer probes keep preferences and saves in the workspace.
    if (getenv("GENERALS_TEST_QUIT_SECONDS"))
    {
        const char* directory = getenv("GENERALS_TEST_USER_DATA");
        if (directory && directory[0]) return AsciiString(directory);
    }
    return m_userDataDir;
'@
    $text=[regex]::Replace($text,$pattern,$replacement)
    }
    $text='#include <cstdlib>'+"`r`n"+$text
    [IO.File]::WriteAllText($path,$text)
}
$path=Join-Path $SourcePath 'Core/GameEngine/Source/Common/CommandLine.cpp'
$text=[IO.File]::ReadAllText($path)
if(-not $text.Contains('GENERALS_TEST_SHELL_MAP')){
    $pattern='(parseCommandLine\(paramsForEngineInit, ARRAY_SIZE\(paramsForEngineInit\),\s*TheWritableGlobalData->m_commandLineData.m_parsedArguments\);)'
    if(-not [regex]::IsMatch($text,$pattern)){throw 'Pinned command line initialization changed.'}
    $text=[regex]::Replace($text,$pattern,@'
$1
    if (getenv("GENERALS_TEST_QUIT_SECONDS")) {
        const char* map=getenv("GENERALS_TEST_SHELL_MAP");
        if (map && map[0]) { TheWritableGlobalData->m_shellMapName=map; TheWritableGlobalData->m_shellMapOn=TRUE; }
    }
'@)
    [IO.File]::WriteAllText($path,$text)
}
$path=Join-Path $SourcePath 'Core/GameEngineDevice/Source/W3DDevice/GameClient/Water/W3DWater.cpp'
foreach($backend in @('Std','Win32')){
    $filesystemPath=Join-Path $SourcePath ('Core/GameEngineDevice/Source/'+$backend+'Device/Common/'+$backend+'BIGFileSystem.cpp')
    $filesystemText=[IO.File]::ReadAllText($filesystemPath)
    if(-not $filesystemText.Contains('GENERALS_TEST_BASE_GAME')){
        $anchor='GetStringFromGeneralsRegistry("", "InstallPath", installPath );'
        if(-not $filesystemText.Contains($anchor)){throw 'Pinned base game archive initialization changed.'}
        $filesystemText=$filesystemText.Replace($anchor,$anchor+@'

    if (getenv("GENERALS_TEST_QUIT_SECONDS")) {
        const char* base=getenv("GENERALS_TEST_BASE_GAME");
        if (base && base[0]) installPath=base;
    }
'@)
        [IO.File]::WriteAllText($filesystemPath,'#include <cstdlib>'+"`r`n"+$filesystemText)
    }
}
$shaderPath=Join-Path $SourcePath 'Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DShaderManager.cpp'
$shaderText=[IO.File]::ReadAllText($shaderPath)
if(-not $shaderText.Contains('// native12 uses HLSL instead of legacy shader files')){
    $pattern='(HRESULT W3DShaderManager::LoadAndCreateD3DShader\([^\r\n]+\)\s*\{)'
    if(-not [regex]::IsMatch($shaderText,$pattern)){throw 'Pinned legacy shader loader changed.'}
    $shaderText=[regex]::Replace($shaderText,$pattern,@'
$1
    // native12 uses HLSL instead of legacy shader files. Terrain uses the
    // existing material-stage fallback; water has its own native HLSL factory.
    if (GetModuleHandleW(L"generals-native12.dll")) return E_NOTIMPL;
'@)
    [IO.File]::WriteAllText($shaderPath,$shaderText)
}
$text=[IO.File]::ReadAllText($path)
if(-not $text.Contains('GeneralsNativeCreateWaterShaders')){
    $anchor="//We're using the same grid"
    if([regex]::Matches($text,[regex]::Escape($anchor)).Count -ne 1){throw 'Pinned water device initialization changed.'}
    $text=$text.Replace($anchor,(@'

    // Native D3D12 water keeps the W3D geometry and animation, using HLSL programs.
    typedef BOOL (WINAPI* NativeWaterFactory)(IDirect3DDevice8*,DWORD*,DWORD*,DWORD*,DWORD*,DWORD*);
    HMODULE nativeModule=GetModuleHandleW(L"generals-native12.dll");
    NativeWaterFactory nativeFactory=nativeModule ? reinterpret_cast<NativeWaterFactory>(GetProcAddress(nativeModule,"GeneralsNativeCreateWaterShaders")) : nullptr;
    const bool nativeWaterShaders=nativeFactory && nativeFactory(m_pDev,&m_dwWaveVertexShader,&m_dwWavePixelShader,&m_riverWaterPixelShader,&m_waterPixelShader,&m_trapezoidWaterPixelShader);
'@)+"`n"+$anchor)
    $anchor='hr = W3DShaderManager::LoadAndCreateD3DShader("shaders\\wave.pso", &Declaration[0], 0, false, &m_dwWavePixelShader);'
    if(-not $text.Contains($anchor)){throw 'Pinned wave pixel initialization changed.'}
    $text=$text.Replace($anchor,'if (!nativeWaterShaders) {'+"`n"+$anchor)
    $anchor='// Create reflection texture'
    $text=$text.Replace($anchor,"}`n"+$anchor)
    $text=$text.Replace('if (W3DShaderManager::getChipset() >= DC_GENERIC_PIXEL_SHADER_1_1)','if (!nativeWaterShaders && W3DShaderManager::getChipset() >= DC_GENERIC_PIXEL_SHADER_1_1)')
    [IO.File]::WriteAllText($path,$text)
}
