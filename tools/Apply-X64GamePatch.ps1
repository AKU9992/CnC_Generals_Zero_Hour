[CmdletBinding()]
param([Parameter(Mandatory)][string]$SourcePath, [ValidateSet('Generals','GeneralsMD')][string]$GameEdition='GeneralsMD')
$editionSourcePath = Join-Path $SourcePath $GameEdition
$ErrorActionPreference = 'Stop'
$repositoryPath = Split-Path $PSScriptRoot -Parent
$expectedPath = Join-Path $repositoryPath '.build\community-reference-x64\GeneralsGameCode-b805c12ee1aedc0a4b241006803b8e04bbf288a6'
if ([IO.Path]::GetFullPath($SourcePath).TrimEnd('\') -ne [IO.Path]::GetFullPath($expectedPath).TrimEnd('\')) { throw 'Only the pinned x64 checkout may be patched.' }
$wwlib = Join-Path $SourcePath 'Core\Libraries\Source\WWVegas\WWLib'
foreach ($edition in @('Generals', 'GeneralsMD')) {
    $ddsPath = Join-Path $SourcePath ($edition + '\Code\Libraries\Source\WWVegas\WW3D2\ddsfile.h')
    $dds = [IO.File]::ReadAllText($ddsPath).Replace('void* Surface;', 'uint32_t Surface; // Serialized DX7 pointer slot is always four bytes.')
    if (-not $dds.Contains('#include <cstdint>')) { $dds = '#include <cstdint>' + "`n#include <cstddef>`n" + $dds }
    if (-not $dds.Contains('static_assert(sizeof(LegacyDDSURFACEDESC2)')) {
        $dds = $dds.Replace('enum DDSType', 'static_assert(sizeof(LegacyDDSURFACEDESC2) == 124, "DDS file header must retain its 32-bit disk layout");' + "`n" + 'static_assert(offsetof(LegacyDDSURFACEDESC2, PixelFormat) == 72);' + "`n" + 'static_assert(offsetof(LegacyDDSURFACEDESC2, Caps) == 104);' + "`n`n" + 'enum DDSType')
    }
    [IO.File]::WriteAllText($ddsPath, $dds)
}
$surfacePath = Join-Path $SourcePath 'Core\Libraries\Source\WWVegas\WW3D2\surfaceclass.cpp'
$surface = [IO.File]::ReadAllText($surfacePath)
$surface = $surface.Replace('(unsigned char*) ((unsigned int)lock_rect.pBits+(y-min->J)*lock_rect.Pitch+(x-min->I)*size)', 'static_cast<unsigned char*>(lock_rect.pBits)+(y-min->J)*lock_rect.Pitch+(x-min->I)*size')
$surface = $surface.Replace('(unsigned char*) ((unsigned int)lock_rect.pBits+y*lock_rect.Pitch)', 'static_cast<unsigned char*>(lock_rect.pBits)+y*lock_rect.Pitch')
[IO.File]::WriteAllText($surfacePath, $surface)
$ddsSourcePath = Join-Path $editionSourcePath 'Code\Libraries\Source\WWVegas\WW3D2\ddsfile.cpp'
$ddsSource = [IO.File]::ReadAllText($ddsSourcePath)
if (-not $ddsSource.Contains('// generals-mods DDS load probe')) {
    $helper = @'
// generals-mods DDS load probe; bounded and enabled only for startup diagnostics.
#include <atomic>
#include <cstdio>
static void traceDDSLoad(const char* name, unsigned width, unsigned height, unsigned actual, unsigned expected)
{
    char enabled[2] = {};
    if (!GetEnvironmentVariableA("GENERALS_X64_STARTUP_TRACE", enabled, sizeof(enabled))) return;
    static std::atomic<unsigned> count{0};
    if (count.fetch_add(1) >= 32) return;
    FILE* log = nullptr;
    if (fopen_s(&log, "GeneralsDDSLoad.log", "a") == 0 && log) {
        fprintf(log, "PID %lu: DDS %s %ux%u payload %u/%u\n", GetCurrentProcessId(),name,width,height,actual,expected);
        fclose(log);
    }
}

'@
    $ddsSource = $ddsSource.Replace('DDSFileClass::DDSFileClass(', $helper + 'DDSFileClass::DDSFileClass(')
    $ddsSource = $ddsSource.Replace('unsigned read_size=file->Read(DDSMemory,size);', 'unsigned read_size=file->Read(DDSMemory,size);' + "`n" + 'traceDDSLoad(Name,Width,Height,read_size,size);')
    [IO.File]::WriteAllText($ddsSourcePath, $ddsSource)
}
$mainCmake = Join-Path $editionSourcePath 'Code\Main\CMakeLists.txt'
$mainText = [IO.File]::ReadAllText($mainCmake)
$mainText = [regex]::Replace($mainText, '(?m)^    d3d(?:x)?8\r?\n', '')
[IO.File]::WriteAllText($mainCmake, $mainText)
Copy-Item -LiteralPath (Join-Path $repositoryPath 'renderer\LegacyExceptionX64.cpp') -Destination (Join-Path $wwlib 'Except.cpp') -Force
$headerPath = Join-Path $wwlib 'Except.h'
$header = [IO.File]::ReadAllText($headerPath)
$header = $header.Replace('int Stack_Walk(unsigned long *return_addresses', 'int Stack_Walk(uintptr_t *return_addresses')
$header = $header.Replace('extern unsigned long ExceptionReturn', 'extern uintptr_t ExceptionReturn')
if (-not $header.Contains('#include <cstdint>')) { $header = '#include <cstdint>' + "`n" + $header }
[IO.File]::WriteAllText($headerPath, $header)
$stackHeaderPath = Join-Path $editionSourcePath 'Code\GameEngine\Include\Common\StackDump.h'
$stackHeader = [IO.File]::ReadAllText($stackHeaderPath)
$stackHeader = $stackHeader.Replace('DWORD eip,DWORD esp,DWORD ebp', 'uintptr_t eip,uintptr_t esp,uintptr_t ebp')
$stackHeader = $stackHeader.Replace('unsigned int* address', 'uintptr_t* address')
[IO.File]::WriteAllText($stackHeaderPath, $stackHeader)
Copy-Item -LiteralPath (Join-Path $repositoryPath 'renderer\StackDumpX64.cpp') -Destination (Join-Path $editionSourcePath 'Code\GameEngine\Source\Common\System\StackDump.cpp') -Force
$windowPath = Join-Path $SourcePath 'Core\GameEngine\Include\GameClient\GameWindow.h'
$window = [IO.File]::ReadAllText($windowPath)
$window = $window.Replace('typedef UnsignedInt WindowMsgData;', 'typedef uintptr_t WindowMsgData;')
if (-not $window.Contains('#include <cstdint>')) { $window = '#include <cstdint>' + "`n" + $window }
[IO.File]::WriteAllText($windowPath, $window)
$engineCmake = Join-Path $editionSourcePath 'Code\GameEngine\CMakeLists.txt'
$engineText = [IO.File]::ReadAllText($engineCmake)
$engineTarget=if($GameEdition -eq 'Generals'){'g_gameengine'}else{'z_gameengine'}
$includeDirective='target_include_directories('+$engineTarget+' PRIVATE ${CMAKE_SOURCE_DIR}/Core/Libraries/Source)'
if (-not $engineText.Contains($includeDirective)) {
    [IO.File]::WriteAllText($engineCmake, $engineText + "`n" + $includeDirective + "`n")
}
$debugPath = Join-Path $SourcePath 'Core\Libraries\Source\debug\debug_except.cpp'
$debug = [IO.File]::ReadAllText($debugPath)
foreach ($register in @('ax','bx','cx','dx','si','di','ip','sp','bp')) {
    $debug = $debug.Replace(('ctx.E' + $register), ('ctx.R' + $register))
    $debug = $debug.Replace(('->E' + $register), ('->R' + $register))
}
$debug = $debug.Replace('static BOOL CALLBACK ExceptionDlgProc', 'static INT_PTR CALLBACK ExceptionDlgProc')
$fpu = [regex]::Match($debug, '(?s)void DebugExceptionhandler::LogFPURegisters\(.*?(?=// include exception dialog box)')
if ($fpu.Success) {
    $newFpu = @'
void DebugExceptionhandler::LogFPURegisters(Debug &dbg, struct _EXCEPTION_POINTERS *exptr)
{
    const CONTEXT& context = *exptr->ContextRecord;
    dbg << "x64 MXCSR:" << Debug::Hex() << context.MxCsr << "\n"
        << "x87 ControlWord:" << context.FltSave.ControlWord << "\n"
        << "x87 StatusWord:" << context.FltSave.StatusWord << "\n" << Debug::Dec();
}

'@
    $debug = $debug.Remove($fpu.Index, $fpu.Length).Insert($fpu.Index, $newFpu)
}
[IO.File]::WriteAllText($debugPath, $debug)
$debugDirectory = Join-Path $SourcePath 'Core\Libraries\Source\debug'
Copy-Item -LiteralPath (Join-Path $repositoryPath 'renderer\DebugStackX64.cpp') -Destination (Join-Path $debugDirectory 'debug_stack.cpp') -Force
foreach ($file in @('debug_stack.h', 'debug_debug.h', 'debug_debug.cpp')) {
    $path = Join-Path $debugDirectory $file
    $text = [IO.File]::ReadAllText($path)
    $text = $text.Replace('unsigned m_addr[MAX_ADDR]', 'uintptr_t m_addr[MAX_ADDR]').Replace('unsigned GetAddress(', 'uintptr_t GetAddress(')
    $text = $text.Replace('GetSymbol(unsigned addr', 'GetSymbol(uintptr_t addr')
    $text = $text.Replace('unsigned curStackFrame', 'uintptr_t curStackFrame').Replace('unsigned Debug::curStackFrame', 'uintptr_t Debug::curStackFrame')
    $text = $text.Replace('unsigned frameAddr', 'uintptr_t frameAddr').Replace('unsigned addr', 'uintptr_t addr')
    $text = $text.Replace('(unsigned)fileOrGroup', 'reinterpret_cast<uintptr_t>(fileOrGroup)')
    if (-not $text.Contains('#include <cstdint>')) { $text = '#include <cstdint>' + "`n" + $text }
    [IO.File]::WriteAllText($path, $text)
}
$debugPath = Join-Path $SourcePath 'Core\Libraries\Source\debug\debug_debug.cpp'
$debug = [IO.File]::ReadAllText($debugPath)
$capture = [regex]::Match($debug, '(?s)unsigned help;\s*#if defined\(_MSC_VER\).*?#endif(?=\s*curStackFrame=help;)')
if ($capture.Success) { $debug = $debug.Remove($capture.Index, $capture.Length).Insert($capture.Index, 'const auto help = reinterpret_cast<uintptr_t>(_ReturnAddress());') }
$debug = $debug.Replace('_asm int 0x03', '__debugbreak();')
$debug = $debug.Replace('char help[9];', 'char help[17];').Replace('_ultoa((unsigned long)ptr,help,16)', '_ui64toa(reinterpret_cast<uintptr_t>(ptr),help,16)')
$debug = $debug.Replace('char buf[9];', 'char buf[17];').Replace('sprintf(buf,"%08x",dump.m_absAddr?unsigned(cur):cur-dump.m_startPtr);', 'sprintf(buf,"%016llx",static_cast<unsigned long long>(dump.m_absAddr?reinterpret_cast<uintptr_t>(cur):cur-dump.m_startPtr));')
if (-not $debug.Contains('#include <intrin.h>')) { $debug = '#include <intrin.h>' + "`n" + $debug }
[IO.File]::WriteAllText($debugPath, $debug)
$videoHeader = Join-Path $SourcePath 'Core\GameEngine\Include\GameClient\WindowVideoManager.h'
$videoText = [IO.File]::ReadAllText($videoHeader).Replace('std::hash<UnsignedInt> hasher;', 'std::hash<ConstGameWindowPtr> hasher;').Replace('return hasher((UnsignedInt)p);', 'return hasher(p);')
[IO.File]::WriteAllText($videoHeader, $videoText)
$assetPath = Join-Path $editionSourcePath 'Code\GameEngineDevice\Source\W3DDevice\GameClient\W3DAssetManager.cpp'
$assetText = [IO.File]::ReadAllText($assetPath).Replace('((int)mesh_name) - ((int)name) + 1', 'static_cast<int>(mesh_name - name) + 1')
[IO.File]::WriteAllText($assetPath, $assetText)
foreach ($file in @('registry.h', 'registry.cpp', 'thread.h')) {
    $path = Join-Path $wwlib $file
    $text = [IO.File]::ReadAllText($path)
    $text = [regex]::Replace($text, '\bint\s+Key;', 'uintptr_t Key;')
    $text = $text.Replace('Key = (int)key;', 'Key = reinterpret_cast<uintptr_t>(key);')
    $text = $text.Replace('volatile unsigned long handle;', 'volatile uintptr_t handle;')
    if (-not $text.Contains('#include <cstdint>')) { $text = '#include <cstdint>' + "`n" + $text }
    [IO.File]::WriteAllText($path, $text)
}
$imePath = Join-Path $SourcePath 'Core\GameEngine\Source\GameClient\GUI\IMEManager.cpp'
$ime = [IO.File]::ReadAllText($imePath).Replace('(Char*) ((UnsignedInt) clist + (UnsignedInt) clist->dwOffset[i])', 'reinterpret_cast<Char*>(reinterpret_cast<unsigned char*>(clist) + clist->dwOffset[i])')
[IO.File]::WriteAllText($imePath, $ime)
$firewallPath = Join-Path $SourcePath 'Core\GameEngine\Source\GameNetwork\FirewallHelper.cpp'
$firewall = [IO.File]::ReadAllText($firewallPath).Replace('ntohl((UnsignedInt)mangler_addresses[m]);', '// Address bytes are already stored in network order.')
[IO.File]::WriteAllText($firewallPath, $firewall)
$gameEnginePath = Join-Path $editionSourcePath 'Code\GameEngine\Source\Common\GameEngine.cpp'
$gameEngine = [IO.File]::ReadAllText($gameEnginePath)
if (-not $gameEngine.Contains('// generals-mods x64 startup diagnostics')) {
    $trace = @'
// generals-mods x64 startup diagnostics, enabled only for startup probes.
static void traceX64Startup(const char* stage, const char* detail = "")
{
    char enabled[2] = {};
    if (!GetEnvironmentVariableA("GENERALS_X64_STARTUP_TRACE", enabled, sizeof(enabled))) return;
    FILE* log = nullptr;
    if (fopen_s(&log, "GeneralsX64Startup.log", "a") == 0 && log) {
        fprintf(log, "PID %lu: %s %s\n", GetCurrentProcessId(), stage, detail);
        fclose(log);
    }
}

'@
    $gameEngine = $gameEngine.Replace('template<class SUBSYSTEM>', $trace + 'template<class SUBSYSTEM>')
    $gameEngine = $gameEngine.Replace('TheSubsystemList->initSubsystem(sys, path1, path2, pXfer, name);', 'traceX64Startup("Initializing", name.str());' + "`n" + 'TheSubsystemList->initSubsystem(sys, path1, path2, pXfer, name);' + "`n" + 'traceX64Startup("Initialized", name.str());')
    $gameEngine = $gameEngine.Replace("void GameEngine::init()`r`n{", "void GameEngine::init()`r`n{`r`n    traceX64Startup(`"Engine init begin`", `"`" );")
    [IO.File]::WriteAllText($gameEnginePath, $gameEngine)
}
