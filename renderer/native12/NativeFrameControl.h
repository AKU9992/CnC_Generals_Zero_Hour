#pragma once
#include <windows.h>
#include <cstdint>
#include <cstdio>
namespace generals_mods::native12 {
enum class FpsMode { Standard, Auto, Custom, Diagnostic };
struct DisplayRate { UINT numerator=60,denominator=1; DWORD error=0; bool exact=false; double hz() const {return double(numerator)/denominator;} };
DisplayRate activeDisplayRate(HWND window);
double frameTarget(FpsMode mode,double custom,const DisplayRate& rate,bool background=false);
class FrameControl {
public:
 ~FrameControl();
 void configure(HWND window);
 double waitFrame();
 void reset();
 double target() const {return target_;}
 bool vsync() const {return vsync_;}
 void setForTest(FpsMode mode,double custom=60) {mode_=mode;custom_=custom;nextPoll_=0;}
 const DisplayRate& rate() const {return rate_;}
private:
 void refresh(LONGLONG now);
 void log(const char* reason);
 HWND window_=nullptr;HMONITOR monitor_=nullptr;HANDLE timer_=nullptr;
 LONGLONG frequency_=0,last_=0,nextPoll_=0;double deadline_=0,target_=60,custom_=60;
 DisplayRate rate_; FpsMode mode_=FpsMode::Standard;bool vsync_=false,background_=false,configured_=false;
};
FrameControl& frameControl();
}