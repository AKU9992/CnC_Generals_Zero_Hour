#include "debug.h"
#include "debug_stack.h"
#include <WWLib/Except.h>
#include <windows.h>
#include <dbghelp.h>
#undef StackWalk
#include <cstdio>
#include <cstring>
#include <mutex>
#pragma comment(lib, "dbghelp.lib")
extern std::mutex& LegacySymbolMutex();

DebugStackwalk::DebugStackwalk() { Load_Image_Helper(); }
DebugStackwalk::~DebugStackwalk() {}
void* DebugStackwalk::GetDbghelpHandle() { Load_Image_Helper(); return GetModuleHandleW(L"dbghelp.dll"); }
bool DebugStackwalk::IsOldDbghelp() { return false; }
DebugStackwalk::Signature::Signature(const Signature& source) { *this = source; }
DebugStackwalk::Signature& DebugStackwalk::Signature::operator=(const Signature& source)
{
    m_numAddr = source.m_numAddr;
    memcpy(m_addr, source.m_addr, m_numAddr * sizeof(m_addr[0]));
    return *this;
}
uintptr_t DebugStackwalk::Signature::GetAddress(int index) const
{
    return index >= 0 && static_cast<unsigned>(index) < m_numAddr ? m_addr[index] : 0;
}
int DebugStackwalk::StackWalk(Signature& signature, CONTEXT* context)
{
    signature.m_numAddr = ::Stack_Walk(signature.m_addr, Signature::MAX_ADDR, context);
    return signature.m_numAddr;
}
void DebugStackwalk::Signature::GetSymbol(uintptr_t address, char* text, unsigned capacity)
{
    char module[256] = {}, symbol[256] = {}, file[256] = {};
    unsigned moduleOffset = 0, symbolOffset = 0, line = 0, lineOffset = 0;
    GetSymbol(address, module, sizeof(module), &moduleOffset, symbol, sizeof(symbol),
        &symbolOffset, file, sizeof(file), &line, &lineOffset);
    if (text && capacity) snprintf(text, capacity, "%p %s+%X %s+%X %s:%u",
        reinterpret_cast<void*>(address), module, moduleOffset, symbol, symbolOffset, file, line);
}
void DebugStackwalk::Signature::GetSymbol(uintptr_t address,
    char* module, unsigned moduleSize, unsigned* moduleOffset,
    char* symbol, unsigned symbolSize, unsigned* symbolOffset,
    char* file, unsigned fileSize, unsigned* line, unsigned* lineOffset)
{
    if (module && moduleSize) *module = 0;
    if (symbol && symbolSize) *symbol = 0;
    if (file && fileSize) *file = 0;
    if (moduleOffset) *moduleOffset = 0;
    if (symbolOffset) *symbolOffset = 0;
    if (line) *line = 0;
    if (lineOffset) *lineOffset = 0;
    Load_Image_Helper();
    std::lock_guard<std::mutex> lock(LegacySymbolMutex());
    IMAGEHLP_MODULE64 info = {};
    info.SizeOfStruct = sizeof(info);
    if (SymGetModuleInfo64(GetCurrentProcess(), address, &info)) {
        if (module && moduleSize) strncpy_s(module, moduleSize, info.ModuleName, _TRUNCATE);
        if (moduleOffset && address - info.BaseOfImage <= UINT_MAX) *moduleOffset = static_cast<unsigned>(address - info.BaseOfImage);
    }
    alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO) + 256] = {};
    auto* symbolInfo = reinterpret_cast<SYMBOL_INFO*>(storage);
    symbolInfo->SizeOfStruct = sizeof(SYMBOL_INFO); symbolInfo->MaxNameLen = 255;
    DWORD64 displacement = 0;
    if (SymFromAddr(GetCurrentProcess(), address, &displacement, symbolInfo)) {
        if (symbol && symbolSize) strncpy_s(symbol, symbolSize, symbolInfo->Name, _TRUNCATE);
        if (symbolOffset && displacement <= UINT_MAX) *symbolOffset = static_cast<unsigned>(displacement);
    }
    IMAGEHLP_LINE64 source = {}; source.SizeOfStruct = sizeof(source);
    DWORD relative = 0;
    if (SymGetLineFromAddr64(GetCurrentProcess(), address, &relative, &source)) {
        if (file && fileSize) strncpy_s(file, fileSize, source.FileName, _TRUNCATE);
        if (line) *line = source.LineNumber;
        if (lineOffset) *lineOffset = relative;
    }
}
Debug& operator<<(Debug& output, const DebugStackwalk::Signature& signature)
{
    for (unsigned index = 0; index < signature.Size(); ++index) {
        char text[512]; signature.GetSymbol(signature.GetAddress(index), text, sizeof(text));
        output << text << "\n";
    }
    return output;
}
