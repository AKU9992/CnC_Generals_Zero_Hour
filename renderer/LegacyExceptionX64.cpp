// x64 replacement for the WWLib x86 register/stack diagnostics.
#include "Except.h"
#include <dbghelp.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <unordered_map>
#pragma comment(lib, "dbghelp.lib")

uintptr_t ExceptionReturnStack = 0, ExceptionReturnAddress = 0, ExceptionReturnFrame = 0;
static bool exitOnException = false, tryingToExit = false;
static void (*exceptionCallback)() = nullptr;
static char* (*versionCallback)() = nullptr;
static DWORD mainThread = 0;
static std::mutex symbolMutex;
std::mutex& LegacySymbolMutex() { return symbolMutex; }
static std::once_flag symbolInit;
static bool symbolsReady = false;
static std::mutex threadMutex;
static std::unordered_map<DWORD, HANDLE> registeredThreads;

void Load_Image_Helper()
{
    std::call_once(symbolInit, [] {
        SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_FAIL_CRITICAL_ERRORS);
        symbolsReady = SymInitialize(GetCurrentProcess(), nullptr, TRUE) != FALSE;
    });
}

int Stack_Walk(uintptr_t* addresses, int capacity, CONTEXT* supplied)
{
    if (!addresses || capacity <= 0) return 0;
    Load_Image_Helper();
    if (!symbolsReady) return 0;
    std::lock_guard<std::mutex> lock(symbolMutex);
    CONTEXT context = {};
    if (supplied) context = *supplied; else RtlCaptureContext(&context);
    STACKFRAME64 frame = {};
    frame.AddrPC = {context.Rip, 0, AddrModeFlat};
    frame.AddrStack = {context.Rsp, 0, AddrModeFlat};
    frame.AddrFrame = {context.Rbp, 0, AddrModeFlat};
    int count = 0;
    while (count < capacity && StackWalk64(IMAGE_FILE_MACHINE_AMD64, GetCurrentProcess(),
        GetCurrentThread(), &frame, &context, nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
    {
        if (!frame.AddrPC.Offset) break;
        addresses[count++] = static_cast<uintptr_t>(frame.AddrPC.Offset);
    }
    return count;
}

bool Lookup_Symbol(void* address, char* symbol, int& displacement)
{
    Load_Image_Helper();
    if (!symbolsReady || !symbol) return false;
    std::lock_guard<std::mutex> lock(symbolMutex);
    alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO) + 256] = {};
    auto* info = reinterpret_cast<SYMBOL_INFO*>(storage);
    info->SizeOfStruct = sizeof(SYMBOL_INFO);
    info->MaxNameLen = 255;
    DWORD64 offset = 0;
    if (!SymFromAddr(GetCurrentProcess(), reinterpret_cast<DWORD64>(address), &offset, info)) return false;
    // Existing WWLib callers provide at least 128 characters.
    strncpy_s(symbol, 128, info->Name, _TRUNCATE);
    if (offset > INT_MAX) return false;
    displacement = static_cast<int>(offset);
    return true;
}

int Exception_Handler(int code, EXCEPTION_POINTERS* exception)
{
    static std::atomic_flag handling = ATOMIC_FLAG_INIT;
    if (handling.test_and_set()) return EXCEPTION_CONTINUE_SEARCH;
    tryingToExit = exitOnException;
    char message[256] = {};
    snprintf(message, sizeof(message), "WWLib x64 exception 0x%08X at %p\r\n", code,
        exception && exception->ExceptionRecord ? exception->ExceptionRecord->ExceptionAddress : nullptr);
    OutputDebugStringA(message);
    HANDLE log = CreateFileW(L"LegacyException-x64.log", FILE_APPEND_DATA, FILE_SHARE_READ,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (log != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(log, message, static_cast<DWORD>(strlen(message)), &written, nullptr);
        CloseHandle(log);
    }
    if (exceptionCallback) exceptionCallback();
    handling.clear();
    return exitOnException ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH;
}

void Register_Thread_ID(unsigned long id, char*, bool main)
{
    std::lock_guard<std::mutex> lock(threadMutex);
    if (registeredThreads.find(id) == registeredThreads.end())
        registeredThreads.emplace(id, OpenThread(THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT, FALSE, id));
    if (main) mainThread = id;
}
void Unregister_Thread_ID(unsigned long id, char*)
{
    std::lock_guard<std::mutex> lock(threadMutex);
    auto entry = registeredThreads.find(id);
    if (entry != registeredThreads.end()) {
        if (entry->second) CloseHandle(entry->second);
        registeredThreads.erase(entry);
    }
}
unsigned long Get_Main_Thread_ID() { return mainThread; }
void Register_Application_Exception_Callback(void (*callback)()) { exceptionCallback = callback; }
void Register_Application_Version_Callback(char* (*callback)()) { versionCallback = callback; }
void Set_Exit_On_Exception(bool enabled) { exitOnException = enabled; }
bool Is_Trying_To_Exit() { return tryingToExit; }
