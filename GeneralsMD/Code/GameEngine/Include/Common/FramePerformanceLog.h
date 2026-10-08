// Optional frame timing for rebuilt executables; does not alter simulation cadence.
#ifndef GENERALS_FRAME_PERFORMANCE_LOG_H
#define GENERALS_FRAME_PERFORMANCE_LOG_H
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "Common/AkuCpuPhases.h"

class FramePerformanceLog
{
public:
    explicit FramePerformanceLog(const char *userDataDirectory)
        : m_file(NULL), m_start(0), m_updateEnd(0), m_frames(0)
    {
        m_frequency.QuadPart = 0;
        char enabled[2] = { 0 };
        if (GetEnvironmentVariableA("GENERALS_FPS_PROFILE", enabled, sizeof(enabled)) != 1
            || enabled[0] != '1' || !userDataDirectory
            || !QueryPerformanceFrequency(&m_frequency) || m_frequency.QuadPart <= 0)
            return;
        const char *name = "FramePerformance.csv";
        size_t length = strlen(userDataDirectory);
        bool separatorNeeded = length > 0 && userDataDirectory[length - 1] != '\\';
        char path[MAX_PATH];
        if (length + (separatorNeeded ? 1 : 0) + strlen(name) >= sizeof(path))
            return;
        strcpy(path, userDataDirectory);
        if (separatorNeeded) strcat(path, "\\");
        strcat(path, name);
        m_file = fopen(path, "w");
        if (m_file) {
            fprintf(m_file, "sample,logic_frame,in_game,update_ms,limiter_ms,frame_ms,logic_ms,audio_ms,client_ms,messages_ms,network_ms,draw_ms,drawables_ms,ai_ms,pathfind_ms,target_fps\n");
            fflush(m_file);
        }
    }

    ~FramePerformanceLog() { if (m_file) fclose(m_file); }

    void beginFrame()
    {
        if (!m_file) return;
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        m_start = m_updateEnd = now.QuadPart;
        akuPhases().enabled=true;memset(akuPhases().ticks,0,sizeof(akuPhases().ticks));
    }

    void endUpdate()
    {
        if (!m_file) return;
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        m_updateEnd = now.QuadPart;
    }

    void endFrame(unsigned long logicFrame, bool inGame)
    {
        if (!m_file) return;
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        double scale = 1000.0 / (double)m_frequency.QuadPart;
        fprintf(m_file, "%lu,%lu,%d,%.6f,%.6f,%.6f", ++m_frames, logicFrame, inGame ? 1 : 0,
                (double)(m_updateEnd - m_start) * scale,
                (double)(now.QuadPart - m_updateEnd) * scale,
                (double)(now.QuadPart - m_start) * scale);
        for(int phase=0;phase<AkuPhaseCount;++phase)fprintf(m_file,",%.6f",double(akuPhases().ticks[phase])*scale);
        typedef double(__cdecl* TargetFn)();
        static TargetFn target=reinterpret_cast<TargetFn>(GetProcAddress(GetModuleHandleW(L"generals-native12.dll"),"generalsNativeGetFrameTarget"));
        fprintf(m_file,",%.9f\n",target?target():0.0);
        akuPhases().enabled=false;
        if (m_frames % 300 == 0) fflush(m_file);
    }

private:
    FramePerformanceLog(const FramePerformanceLog &);
    FramePerformanceLog &operator=(const FramePerformanceLog &);
    FILE *m_file;
    LARGE_INTEGER m_frequency;
    LONGLONG m_start, m_updateEnd;
    unsigned long m_frames;
};
#endif
