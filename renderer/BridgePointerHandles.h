#pragma once
#include <unordered_map>
#include <new>

// D3D8 exposes DWORD handles even in a 64-bit process. Store full pointers
// behind opaque IDs; never write an eight-byte COM pointer to a DWORD slot.
class BridgePointerHandles
{
public:
    DWORD add(void* pointer)
    {
        if (!pointer || next_ == 0) throw std::bad_alloc();
        const DWORD handle = next_++;
        pointers_.emplace(handle, pointer);
        return handle;
    }
    void* get(DWORD handle) const
    {
        auto entry = pointers_.find(handle);
        return entry == pointers_.end() ? nullptr : entry->second;
    }
    void* take(DWORD handle)
    {
        auto entry = pointers_.find(handle);
        if (entry == pointers_.end()) return nullptr;
        void* pointer = entry->second;
        pointers_.erase(entry);
        return pointer;
    }
private:
    DWORD next_ = 0x80000001;
    std::unordered_map<DWORD, void*> pointers_;
};
