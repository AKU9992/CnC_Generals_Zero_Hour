#include <windows.h>
#include <dbghelp.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#pragma comment(lib, "dbghelp.lib")

struct Region { DWORD64 address; DWORD64 size; const unsigned char* bytes; };
static std::vector<Region> regions;
static BOOL CALLBACK readDumpMemory(HANDLE, DWORD64 address, PVOID output, DWORD count, LPDWORD read)
{
    *read = 0;
    for (const Region& region : regions) {
        if (address >= region.address && address - region.address < region.size) {
            DWORD64 available = region.size - (address - region.address);
            DWORD amount = available < count ? static_cast<DWORD>(available) : count;
            std::memcpy(output, region.bytes + (address - region.address), amount);
            *read = amount;
            return amount == count;
        }
    }
    return FALSE;
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 3) { std::puts("Usage: InspectCrashDump dump.dmp local-symbol-directory"); return 2; }
    HANDLE file = CreateFileW(argv[1], GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) return 3;
    HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!mapping) { CloseHandle(file); return 4; }
    void* base = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    if (!base) { CloseHandle(mapping); CloseHandle(file); return 5; }
    PMINIDUMP_DIRECTORY directory = nullptr;
    void* stream = nullptr;
    ULONG size = 0;
    if (!MiniDumpReadDumpStream(base, ExceptionStream, &directory, &stream, &size)) return 6;
    const auto* exception = static_cast<MINIDUMP_EXCEPTION_STREAM*>(stream);
    std::printf("Exception: 0x%08lx at 0x%llx, thread %lu\n", exception->ExceptionRecord.ExceptionCode,
        exception->ExceptionRecord.ExceptionAddress, exception->ThreadId);
    for (ULONG i = 0; i < exception->ExceptionRecord.NumberParameters; ++i)
        std::printf("Parameter %lu: 0x%llx\n", i, exception->ExceptionRecord.ExceptionInformation[i]);

    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_FAIL_CRITICAL_ERRORS);
    if (!SymInitializeW(process, argv[2], FALSE)) return 7;
    if (MiniDumpReadDumpStream(base, ModuleListStream, &directory, &stream, &size)) {
        const auto* modules = static_cast<MINIDUMP_MODULE_LIST*>(stream);
        for (ULONG i = 0; i < modules->NumberOfModules; ++i) {
            const auto& module = modules->Modules[i];
            const auto* name = reinterpret_cast<const MINIDUMP_STRING*>(static_cast<const unsigned char*>(base) + module.ModuleNameRva);
            std::wstring path(name->Buffer, name->Length / sizeof(wchar_t));
            SymLoadModuleExW(process, nullptr, path.c_str(), nullptr, module.BaseOfImage, module.SizeOfImage, nullptr, 0);
            if (exception->ExceptionRecord.ExceptionAddress >= module.BaseOfImage &&
                exception->ExceptionRecord.ExceptionAddress - module.BaseOfImage < module.SizeOfImage)
                std::wprintf(L"Fault module: %ls + 0x%llx\n", path.c_str(), exception->ExceptionRecord.ExceptionAddress - module.BaseOfImage);
        }
    }
    if (MiniDumpReadDumpStream(base, MemoryListStream, &directory, &stream, &size)) {
        const auto* memory = static_cast<MINIDUMP_MEMORY_LIST*>(stream);
        for (ULONG i = 0; i < memory->NumberOfMemoryRanges; ++i) {
            const auto& entry = memory->MemoryRanges[i];
            regions.push_back({entry.StartOfMemoryRange, entry.Memory.DataSize, static_cast<const unsigned char*>(base) + entry.Memory.Rva});
        }
    }
    if (MiniDumpReadDumpStream(base, Memory64ListStream, &directory, &stream, &size)) {
        const auto* memory = static_cast<MINIDUMP_MEMORY64_LIST*>(stream);
        ULONG64 offset = memory->BaseRva;
        for (ULONG64 i = 0; i < memory->NumberOfMemoryRanges; ++i) {
            const auto& entry = memory->MemoryRanges[i];
            regions.push_back({entry.StartOfMemoryRange, entry.DataSize, static_cast<const unsigned char*>(base) + offset});
            offset += entry.DataSize;
        }
    }
    if (MiniDumpReadDumpStream(base, ThreadListStream, &directory, &stream, &size)) {
        const auto* threads = static_cast<MINIDUMP_THREAD_LIST*>(stream);
        for (ULONG i = 0; i < threads->NumberOfThreads; ++i) {
            const auto& entry = threads->Threads[i].Stack;
            regions.push_back({entry.StartOfMemoryRange, entry.Memory.DataSize, static_cast<const unsigned char*>(base) + entry.Memory.Rva});
        }
    }
    CONTEXT context = {};
    if (exception->ThreadContext.DataSize < sizeof(context)) return 8;
    std::memcpy(&context, static_cast<const unsigned char*>(base) + exception->ThreadContext.Rva, sizeof(context));
    STACKFRAME64 frame = {};
#if defined(_WIN64)
    frame.AddrPC.Offset = context.Rip;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrFrame.Offset = context.Rbp;
    constexpr DWORD machine = IMAGE_FILE_MACHINE_AMD64;
#else
    frame.AddrPC.Offset = context.Eip;
    frame.AddrStack.Offset = context.Esp;
    frame.AddrFrame.Offset = context.Ebp;
    constexpr DWORD machine = IMAGE_FILE_MACHINE_I386;
#endif
    frame.AddrPC.Mode = frame.AddrStack.Mode = frame.AddrFrame.Mode = AddrModeFlat;
    for (unsigned i = 0; i < 40 && frame.AddrPC.Offset; ++i) {
        alignas(SYMBOL_INFO) unsigned char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO); symbol->MaxNameLen = MAX_SYM_NAME;
        DWORD64 displacement = 0;
        std::printf("#%u 0x%llx", i, frame.AddrPC.Offset);
        if (SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol)) std::printf(" %s + 0x%llx", symbol->Name, displacement);
        IMAGEHLP_LINE64 line = {}; line.SizeOfStruct = sizeof(line);
        DWORD lineDisplacement = 0;
        if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &lineDisplacement, &line)) std::printf(" (%s:%lu)", line.FileName, line.LineNumber);
        std::puts("");
        if (!StackWalk64(machine, process, nullptr, &frame, &context, readDumpMemory,
            SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) break;
    }
    SymCleanup(process); UnmapViewOfFile(base); CloseHandle(mapping); CloseHandle(file);
    return 0;
}
