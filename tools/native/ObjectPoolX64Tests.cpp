#include <windows.h>
#include <cstdio>
#include <cstdint>
#include "Precompiled/CppMacros.h"
#include "mempool.h"

struct PoolProbeNode { PoolProbeNode* next=nullptr; uintptr_t payload=0; };
int main() {
    static_assert(sizeof(void*)==8,"Run the x64 regression test.");
    for(unsigned iteration=0;iteration<100;++iteration) {
        ObjectPoolClass<PoolProbeNode,2> pool;
        PoolProbeNode* nodes[12]={};
        for(unsigned i=0;i<12;++i) {
            nodes[i]=pool.Allocate_Object();
            if(reinterpret_cast<uintptr_t>(nodes[i])%alignof(PoolProbeNode)) return 1;
            nodes[i]->next=nodes[i]; nodes[i]->payload=0x1122334455667788ull+i;
        }
        for(unsigned i=0;i<12;++i) {
            if(nodes[i]->next!=nodes[i] || nodes[i]->payload!=0x1122334455667788ull+i) return 2;
            pool.Free_Object(nodes[i]);
        }
        // Destructor must traverse every block after all payloads were written.
    }
    puts("PASS: x64 object pool alignment, six block links and repeated destruction");
}
