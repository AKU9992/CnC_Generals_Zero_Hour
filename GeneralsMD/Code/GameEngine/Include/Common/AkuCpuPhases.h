#ifndef AKU_CPU_PHASES_H
#define AKU_CPU_PHASES_H
#include <windows.h>
#include <cstring>
enum AkuPhase { AkuLogic, AkuAudio, AkuClient, AkuMessages, AkuNetwork, AkuDraw, AkuDrawables, AkuAI, AkuPathfind, AkuPhaseCount };
struct AkuPhaseData {bool enabled=false;LONGLONG ticks[AkuPhaseCount]{};};
inline AkuPhaseData& akuPhases(){static AkuPhaseData data;return data;}
class AkuCpuScope {
public:
 explicit AkuCpuScope(AkuPhase phase):phase_(phase){if(akuPhases().enabled)QueryPerformanceCounter(&start_);}
 ~AkuCpuScope(){if(start_.QuadPart){LARGE_INTEGER end{};QueryPerformanceCounter(&end);akuPhases().ticks[phase_]+=end.QuadPart-start_.QuadPart;}}
private:AkuPhase phase_;LARGE_INTEGER start_{};
};
template<class F> inline void akuProfileCall(AkuPhase phase,F fn){AkuCpuScope scope(phase);fn();}
#endif