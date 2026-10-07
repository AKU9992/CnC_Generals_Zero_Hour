#include "PreRTS.h"
#include "Common/StackDump.h"
#include "Common/Debug.h"
#include "Common/GlobalData.h"
#include <WWLib/Except.h>
#include <debug/debug.h>
#include <debug/debug_stack.h>
#include <cstdio>
#include <vector>

AsciiString g_LastErrorDump;
static void defaultStackLine(const char* line) { OutputDebugStringA(line); }
static void writeStackLine(void* address, void (*callback)(const char*))
{
    char text[512];
    DebugStackwalk::Signature::GetSymbol(reinterpret_cast<uintptr_t>(address), text, sizeof(text));
    callback(text);
    if (g_LastErrorDump.isNotEmpty()) { g_LastErrorDump.concat(text); g_LastErrorDump.concat("\n"); }
}
void FillStackAddresses(void** addresses, unsigned count, unsigned skip)
{
    if (!addresses || !count) return;
    const unsigned boundedSkip = skip > 256 ? 256 : skip;
    const unsigned boundedCount = count > 256 ? 256 : count;
    uintptr_t frames[512] = {};
    const int captured = Stack_Walk(frames, boundedCount + boundedSkip, nullptr);
    for (unsigned index = 0; index < count; ++index)
        addresses[index] = index < boundedCount && index + boundedSkip < static_cast<unsigned>(captured)
            ? reinterpret_cast<void*>(frames[index + boundedSkip]) : nullptr;
}
void StackDumpFromAddresses(void** addresses, unsigned count, void (*callback)(const char*))
{
    if (!callback) callback = defaultStackLine;
    for (unsigned index = 0; addresses && index < count && addresses[index]; ++index) writeStackLine(addresses[index], callback);
}
void StackDump(void (*callback)(const char*))
{
    void* addresses[128] = {};
    FillStackAddresses(addresses, 128, 1);
    StackDumpFromAddresses(addresses, 128, callback);
}
void StackDumpFromContext(uintptr_t ip, uintptr_t sp, uintptr_t bp, void (*callback)(const char*))
{
    CONTEXT context = {}; RtlCaptureContext(&context);
    context.Rip = ip; context.Rsp = sp; context.Rbp = bp;
    uintptr_t frames[128] = {};
    const int count = Stack_Walk(frames, 128, &context);
    if (!callback) callback = defaultStackLine;
    for (int index = 0; index < count; ++index) writeStackLine(reinterpret_cast<void*>(frames[index]), callback);
}
void GetFunctionDetails(void* pointer, char* name, char* file, unsigned* line, uintptr_t* address)
{
    if (address) *address = reinterpret_cast<uintptr_t>(pointer);
    DebugStackwalk::Signature::GetSymbol(reinterpret_cast<uintptr_t>(pointer), nullptr, 0, nullptr,
        name, name ? 256 : 0, nullptr, file, file ? 256 : 0, line, nullptr);
}
void DumpExceptionInfo(unsigned code, EXCEPTION_POINTERS* exception)
{
    char header[256];
    snprintf(header, sizeof(header), "Zero Hour x64 exception 0x%08X at %p\n", code,
        exception && exception->ExceptionRecord ? exception->ExceptionRecord->ExceptionAddress : nullptr);
    g_LastErrorDump = header; OutputDebugStringA(header);
    if (exception && exception->ContextRecord) {
        const CONTEXT& context = *exception->ContextRecord;
        StackDumpFromContext(context.Rip, context.Rsp, context.Rbp, defaultStackLine);
    }
    AsciiString path;
    if (TheGlobalData) path = TheGlobalData->getPath_UserData();
    path.concat("ReleaseCrashInfo-x64.txt");
    FILE* log = nullptr;
    if (fopen_s(&log, path.str(), "wb") == 0 && log) {
        fwrite(g_LastErrorDump.str(), 1, strlen(g_LastErrorDump.str()), log); fclose(log);
    }
}
