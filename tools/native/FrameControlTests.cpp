#include "../../renderer/native12/NativeFrameControl.h"
#include <cstdio>
#include <cmath>
using namespace generals_mods::native12;
int main(){
 auto expect=[](double a,double b){return std::fabs(a-b)<1e-8;};
 DisplayRate r;r.numerator=60000;r.denominator=1001;r.exact=true;
 if(!expect(frameTarget(FpsMode::Standard,80,r),60000.0/1001))return 1;
 r.numerator=120000;
 if(!expect(frameTarget(FpsMode::Auto,80,r),120000.0/1001) || !expect(frameTarget(FpsMode::Custom,80,r),80))return 2;
 if(!expect(frameTarget(FpsMode::Custom,180,r),120000.0/1001) || !expect(frameTarget(FpsMode::Standard,80,r),60))return 3;
 if(!expect(frameTarget(FpsMode::Auto,80,r,true),30) || !expect(frameTarget(FpsMode::Diagnostic,80,r,true),0))return 4;
 r.numerator=24;r.denominator=1;
 if(!expect(frameTarget(FpsMode::Standard,60,r),24))return 5;
 SetEnvironmentVariableA("GENERALS_FPS_PROFILE","1");_putenv_s("GENERALS_FPS_PROFILE","1");
 _putenv_s("GENERALS_FPS_MODE","Custom");_putenv_s("GENERALS_CUSTOM_FPS","59.94");_putenv_s("GENERALS_RENDER_FPS","");
 FrameControl control;control.configure(GetDesktopWindow());const double target=control.target();
 FILETIME c0{},e0{},k0{},u0{},c1{},e1{},k1{},u1{};GetThreadTimes(GetCurrentThread(),&c0,&e0,&k0,&u0);
 double elapsed=0;for(int i=0;i<120;++i)elapsed+=control.waitFrame();GetThreadTimes(GetCurrentThread(),&c1,&e1,&k1,&u1);
 auto ticks=[](FILETIME v){ULARGE_INTEGER n{};n.LowPart=v.dwLowDateTime;n.HighPart=v.dwHighDateTime;return n.QuadPart;};
 const double cpu=double((ticks(k1)+ticks(u1))-(ticks(k0)+ticks(u0)))/10000000.0;
 const double actual=120/elapsed;
 if(std::fabs(actual-target)>target*.03 || cpu/elapsed>.15)return 6;
Sleep(250);if(control.waitFrame()<.245)return 7;
 DisplayRate fallback;if(!expect(frameTarget(FpsMode::Auto,80,fallback),60))return 8;
 _putenv_s("GENERALS_FPS_PROFILE","0");_putenv_s("GENERALS_TEST_QUIT_SECONDS","");
 _putenv_s("GENERALS_FPS_MODE","Diagnostic");_putenv_s("GENERALS_RENDER_FPS","0");
 FrameControl ordinary;ordinary.configure(GetDesktopWindow());if(ordinary.target()<=0)return 9;
 auto rate=activeDisplayRate(GetDesktopWindow());
 printf("PASS FPS modes: Standard, Auto, Custom, fractional refresh, fallback, background cap and diagnostic gate\n");
 printf("PASS sleeping frame pacing: target %.6f, measured %.6f FPS, thread CPU %.3f%%\n",target,actual,100*cpu/elapsed);
 printf("Active refresh %u/%u Hz; exact=%d; status=%lu\n",rate.numerator,rate.denominator,rate.exact?1:0,static_cast<unsigned long>(rate.error));
 return 0;
}