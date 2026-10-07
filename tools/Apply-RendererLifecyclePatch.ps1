[CmdletBinding()]
param([Parameter(Mandatory)][string]$SourcePath)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$expected=Join-Path $root '.build\community-reference-x64\GeneralsGameCode-b805c12ee1aedc0a4b241006803b8e04bbf288a6'
if([IO.Path]::GetFullPath($SourcePath) -ne [IO.Path]::GetFullPath($expected)) {throw 'Only the pinned game may be patched.'}
$path=Join-Path $SourcePath 'Core\Libraries\Source\WWVegas\WW3D2\dx8wrapper.cpp'
$text=[IO.File]::ReadAllText($path)
if(-not $text.Contains('#include "NeuralBridgeClient.h"')) {$text=$text.Replace('#include "SystemDirect3D8.h"', '#include "SystemDirect3D8.h"'+"`n"+'#include "NeuralBridgeClient.h"')}
[IO.File]::WriteAllText($path,$text)
if(-not $text.Contains('neuralShutdown(D3DDevice)')) {
    $anchor='void DX8Wrapper::Release_Device()'
    $text=[regex]::Replace($text,[regex]::Escape($anchor)+'\s*\{', $anchor+"`n{`n    // Shut down Streamline outside the DLL loader lock.`n    generals_mods::neuralShutdown(D3DDevice);")
    [IO.File]::WriteAllText($path,$text)
}
$path=Join-Path $SourcePath 'GeneralsMD\Code\GameEngine\Source\GameClient\GUI\GUICallbacks\Menus\OptionsMenu.cpp'
$text=[IO.File]::ReadAllText($path)
if(-not $text.Contains('// Apply MSAA to the current device')) {
    $anchor='Int mode = index > 0 ? 1 << index : 0;'
    $text=$text.Replace($anchor,$anchor+@'

        // Apply MSAA to the current device, including when resolution is unchanged.
        const auto previousAA = WW3D::Get_MSAA_Mode();
        WW3D::Set_MSAA_Mode(static_cast<WW3D::MultiSampleModeEnum>(mode));
        if (previousAA != WW3D::Get_MSAA_Mode() &&
            !TheDisplay->setDisplayMode(TheDisplay->getWidth(), TheDisplay->getHeight(), TheDisplay->getBitDepth(), TheDisplay->getWindowed()))
        {
            WW3D::Set_MSAA_Mode(previousAA);
            mode = previousAA;
        }
'@)
    [IO.File]::WriteAllText($path,$text)
}
$path=Join-Path $SourcePath 'GeneralsMD\Code\GameEngine\Source\Common\GameEngine.cpp'
$text=[IO.File]::ReadAllText($path)
if(-not $text.Contains('GENERALS_TEST_QUIT_SECONDS')) {
    $anchor='while( !m_quitting )'
    $text=$text.Replace($anchor,@'
char quitSecondsText[16] = {};
    GetEnvironmentVariableA("GENERALS_TEST_QUIT_SECONDS", quitSecondsText, sizeof(quitSecondsText));
    const int quitSeconds = atoi(quitSecondsText);
    const DWORD quitTestStart = timeGetTime();
    while( !m_quitting )
'@)
    $text=$text.Replace('performanceLog.beginFrame();',@'
if (quitSeconds > 0 && quitSeconds <= 60 && timeGetTime() - quitTestStart >= DWORD(quitSeconds * 1000))
        {
            setQuitting(TRUE); // Exercise ordinary teardown in unattended tests.
            break;
        }
        performanceLog.beginFrame();
'@)
    [IO.File]::WriteAllText($path,$text)
}

$path=Join-Path $SourcePath 'Core\Libraries\Source\WWVegas\WW3D2\dx8wrapper.cpp'
$text=[IO.File]::ReadAllText($path)
if(-not $text.Contains('static void traceGraphicsShutdown')) {
    $helper=@"
static void traceGraphicsShutdown(const char* stage) {
    char enabled[2] = {};
    if (!GetEnvironmentVariableA("GENERALS_X64_STARTUP_TRACE", enabled, sizeof(enabled))) return;
    FILE* file=nullptr;
    if(fopen_s(&file,"GeneralsX64Startup.log","a")==0 && file) {
        fprintf(file,"PID %lu: Graphics shutdown %s\n",GetCurrentProcessId(),stage); fclose(file);
    }
}
"@
    $text=$text.Replace('void DX8Wrapper::Shutdown()', $helper+"`nvoid DX8Wrapper::Shutdown()")
    $text=$text.Replace('Do_Onetime_Device_Dependent_Shutdowns();','traceGraphicsShutdown("W3D resources start");'+"`n"+'        Do_Onetime_Device_Dependent_Shutdowns();'+"`n"+'        traceGraphicsShutdown("W3D resources done");')
    $text=$text.Replace('D3DDevice->Release();','traceGraphicsShutdown("device release start");'+"`n"+'        D3DDevice->Release();'+"`n"+'        traceGraphicsShutdown("device release done");')
    $text=$text.Replace('FreeLibrary(D3D8Lib);','traceGraphicsShutdown("bridge unload start");'+"`n"+'        FreeLibrary(D3D8Lib);'+"`n"+'        traceGraphicsShutdown("bridge unload done");')
    [IO.File]::WriteAllText($path,$text)
}
$path=Join-Path $SourcePath 'Core\GameEngine\Source\Common\System\SubsystemInterface.cpp'
$text=[IO.File]::ReadAllText($path)
if(-not $text.Contains('traceSubsystemShutdown')) {
    $helper=@"
static void traceSubsystemShutdown(const char* stage, const char* name) {
    char enabled[2]={};
    if(!GetEnvironmentVariableA("GENERALS_X64_STARTUP_TRACE",enabled,sizeof(enabled))) return;
    FILE* file=nullptr;
    if(fopen_s(&file,"GeneralsX64Startup.log","a")==0 && file) {
        fprintf(file,"PID %lu: Shutdown %s %s\n",GetCurrentProcessId(),stage,name); fclose(file);
    }
}
"@
    $text=$text.Replace('#include "Common/SubsystemInterface.h"', '#include <windows.h>'+"`n"+'#include <cstdio>'+"`n"+'#include "Common/SubsystemInterface.h"')
    $text=$text.Replace('void SubsystemInterfaceList::shutdownAll()', $helper+"`nvoid SubsystemInterfaceList::shutdownAll()")
    $text=$text.Replace('delete sys;', 'traceSubsystemShutdown("start",sys->getName().str());'+"`n"+'        delete sys;'+"`n"+'        traceSubsystemShutdown("done","");')
    [IO.File]::WriteAllText($path,$text)
}
$path=Join-Path $SourcePath 'GeneralsMD\Code\Main\WinMain.cpp'
$text=[IO.File]::ReadAllText($path)
if(-not $text.Contains('GameMain returned')) {
    $helper=@"
static void traceExitStage(const char* stage) {
    char enabled[2]={};
    if(!GetEnvironmentVariableA("GENERALS_X64_STARTUP_TRACE",enabled,sizeof(enabled))) return;
    FILE* file=nullptr;
    if(fopen_s(&file,"GeneralsX64Startup.log","a")==0 && file) {
        fprintf(file,"PID %lu: Exit %s\n",GetCurrentProcessId(),stage); fclose(file);
    }
}
"@
    $text=$text.Replace('Int APIENTRY WinMain(', $helper+"`nInt APIENTRY WinMain(")
    # Actual signature in this source can use INT; fall back to placing helper before main.
    if(-not $text.Contains('static void traceExitStage')) {throw 'Unexpected WinMain signature.'}
    $text=$text.Replace('exitcode = GameMain();', 'exitcode = GameMain();'+"`n"+'        traceExitStage("GameMain returned");')
    $text=$text.Replace('shutdownMemoryManager();','traceExitStage("memory shutdown start");'+"`n"+'        shutdownMemoryManager();'+"`n"+'        traceExitStage("memory shutdown done");')
    [IO.File]::WriteAllText($path,$text)
}
$path=Join-Path $SourcePath 'Core\Libraries\Source\WWVegas\WW3D2\dx8wrapper.cpp'
$text=[IO.File]::ReadAllText($path)
if(-not $text.Contains('finishGraphicsLibraryShutdown')) {
    $helper=@"
// Keep wrapper vtables alive until particle and template textures have released.
static HMODULE deferredGraphicsLibrary = nullptr;
void finishGraphicsLibraryShutdown() {
    if (!deferredGraphicsLibrary) return;
    const auto finalize = reinterpret_cast<void (WINAPI*)()>(GetProcAddress(deferredGraphicsLibrary,"GeneralsNeuralFinalize"));
    if(finalize) finalize();
    FreeLibrary(deferredGraphicsLibrary); deferredGraphicsLibrary=nullptr;
}
"@
    $text=$text.Replace('HINSTANCE D3D8Lib = nullptr;', 'HINSTANCE D3D8Lib = nullptr;'+"`n"+$helper)
    $text=$text.Replace('FreeLibrary(D3D8Lib);', 'if (!deferredGraphicsLibrary) deferredGraphicsLibrary = D3D8Lib;'+"`n"+'        else FreeLibrary(D3D8Lib); // An earlier retained reference keeps the module mapped.')
    $text=$text.Replace('"bridge unload start"','"bridge unload deferred start"').Replace('"bridge unload done"','"bridge unload deferred done"')
    [IO.File]::WriteAllText($path,$text)
}
$path=Join-Path $SourcePath 'GeneralsMD\Code\Main\WinMain.cpp'
$text=[IO.File]::ReadAllText($path)
if(-not $text.Contains('finishGraphicsLibraryShutdown();')) {
    $text=$text.Replace('Int APIENTRY WinMain(', 'extern void finishGraphicsLibraryShutdown();'+"`nInt APIENTRY WinMain(")
    $text=$text.Replace('traceExitStage("GameMain returned");', 'traceExitStage("GameMain returned");'+"`n"+'        finishGraphicsLibraryShutdown();'+"`n"+'        traceExitStage("graphics library finalized");')
    [IO.File]::WriteAllText($path,$text)
}
$path=Join-Path $SourcePath 'Core\Libraries\Source\WWVegas\WWLib\mempool.h'
$text=[IO.File]::ReadAllText($path)
if(-not $text.Contains('// generals-mods pointer-sized object pool header')) {
    $anchor='FreeListHead = (T*)(BlockListHead + 1);'
    if(-not $text.Contains($anchor)) {throw 'Unexpected ObjectPool block layout.'}
    $text=$text.Replace($anchor,'// generals-mods pointer-sized object pool header: objects must not overwrite the high half of the block link.'+"`n"+'        FreeListHead = reinterpret_cast<T*>(reinterpret_cast<unsigned char*>(BlockListHead) + sizeof(uint32*));')
    [IO.File]::WriteAllText($path,$text)
}
# Temporary fault/template probes were used to diagnose teardown. Keep only stage tracing.
$path=Join-Path $SourcePath 'GeneralsMD\Code\Main\WinMain.cpp'
$text=[IO.File]::ReadAllText($path)
$text=[regex]::Replace($text,'(?s)static LONG CALLBACK traceShutdownFault\(.*?(?=extern void finishGraphicsLibraryShutdown)', '')
$text=$text.Replace('AddVectoredExceptionHandler(1,traceShutdownFault);','')
[IO.File]::WriteAllText($path,$text)
$path=Join-Path $SourcePath 'GeneralsMD\Code\GameEngine\Source\Common\Thing\ThingFactory.cpp'
$text=[IO.File]::ReadAllText($path)
$text=[regex]::Replace($text,'(?s)char enabled\[2\]=\{\};\s*if\(GetEnvironmentVariableA\("GENERALS_X64_STARTUP_TRACE".*?(?=deleteInstance\(tmpl\);)', '')
[IO.File]::WriteAllText($path,$text)
