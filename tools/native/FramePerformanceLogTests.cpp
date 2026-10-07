#include "Common/FramePerformanceLog.h"
#include <cmath>
#include <cstdio>
#include <cstring>

int main(int argc, char **argv)
{
    if (argc != 2) return 1;
    char path[MAX_PATH];
    if (strlen(argv[1]) + strlen("\\FramePerformance.csv") >= sizeof(path)) return 2;
    strcpy(path, argv[1]);
    strcat(path, "\\FramePerformance.csv");
    SetEnvironmentVariableA("GENERALS_FPS_PROFILE", "0");
    {
        FramePerformanceLog disabled(argv[1]);
        disabled.beginFrame(); disabled.endUpdate(); disabled.endFrame(1, true);
    }
    if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) return 3;
    SetEnvironmentVariableA("GENERALS_FPS_PROFILE", "10");
    {
        FramePerformanceLog disabled(argv[1]);
        disabled.beginFrame(); disabled.endUpdate(); disabled.endFrame(1, true);
    }
    if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) return 4;
    SetEnvironmentVariableA("GENERALS_FPS_PROFILE", "1");
    {
        FramePerformanceLog enabled(argv[1]);
        for (unsigned long frame = 1; frame <= 2; ++frame) {
            enabled.beginFrame(); Sleep(2); enabled.endUpdate(); Sleep(3);
            enabled.endFrame(frame * 10, frame == 2);
        }
    }
    SetEnvironmentVariableA("GENERALS_FPS_PROFILE", NULL);
    FILE *file = fopen(path, "r");
    if (!file) return 5;
    char line[256];
    if (!fgets(line, sizeof(line), file)
        || strcmp(line, "sample,logic_frame,in_game,update_ms,limiter_ms,frame_ms\n")) return 6;
    for (unsigned long expected = 1; expected <= 2; ++expected) {
        unsigned long sample = 0, logicFrame = 0;
        int inGame = 0;
        double updateMs = 0, waitMs = 0, totalMs = 0;
        if (!fgets(line, sizeof(line), file)
            || sscanf(line, "%lu,%lu,%d,%lf,%lf,%lf", &sample, &logicFrame,
                      &inGame, &updateMs, &waitMs, &totalMs) != 6) return 7;
        if (sample != expected || logicFrame != expected * 10 || inGame != (expected == 2)
            || updateMs < 0 || waitMs < 0 || totalMs <= 0
            || std::fabs(totalMs - updateMs - waitMs) > 0.000002) return 8;
    }
    if (fgets(line, sizeof(line), file)) return 9;
    fclose(file);
    std::printf("PASS: opt-in profiling, disabled modes, CSV timings and logic-frame labels (%u-bit).\n",
                static_cast<unsigned>(sizeof(void*) * 8));
    return 0;
}
