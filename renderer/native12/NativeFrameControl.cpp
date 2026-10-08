#include "NativeFrameControl.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <cwchar>
#include <cstdlib>
#include <cstring>
namespace generals_mods::native12 {
static bool testTimingEnabled(){const char* p=getenv("GENERALS_FPS_PROFILE");if(p && strcmp(p,"1")==0)return true;const char* q=getenv("GENERALS_TEST_QUIT_SECONDS");if(!q)return false;char* end=nullptr;const long seconds=strtol(q,&end,10);return end!=q && end && *end==0 && seconds>0 && seconds<=86400;}
DisplayRate activeDisplayRate(HWND window) {
 DisplayRate result;
 MONITORINFOEXW monitor{};monitor.cbSize=sizeof(monitor);
 if(!GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor)){result.error=GetLastError();if(!result.error)result.error=ERROR_INVALID_MONITOR_HANDLE;return result;}
 for(int attempt=0;attempt<3;++attempt){
  UINT pathsCount=0,modesCount=0;
  LONG status=GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS,&pathsCount,&modesCount);
  if(status!=ERROR_SUCCESS){result.error=status;return result;}
  std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathsCount);std::vector<DISPLAYCONFIG_MODE_INFO> modes(modesCount);
  status=QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS,&pathsCount,paths.data(),&modesCount,modes.data(),nullptr);
  if(status==ERROR_INSUFFICIENT_BUFFER)continue;
  if(status!=ERROR_SUCCESS){result.error=status;return result;}
  for(UINT i=0;i<pathsCount;++i){
   auto& path=paths[i];DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
   source.header.type=DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;source.header.size=sizeof(source);
   source.header.adapterId=path.sourceInfo.adapterId;source.header.id=path.sourceInfo.id;
   if(DisplayConfigGetDeviceInfo(&source.header)!=ERROR_SUCCESS || wcscmp(source.viewGdiDeviceName,monitor.szDevice)!=0)continue;
   auto rate=path.targetInfo.refreshRate;
   if(rate.Denominator && rate.Numerator && double(rate.Numerator)/rate.Denominator>=1 && double(rate.Numerator)/rate.Denominator<=1000){
    result.numerator=rate.Numerator;result.denominator=rate.Denominator;result.exact=true;return result;
   }
  }
  result.error=ERROR_NOT_FOUND;return result;
 }
 result.error=ERROR_INSUFFICIENT_BUFFER;return result;
}
double frameTarget(FpsMode mode,double custom,const DisplayRate& rate,bool background){
 const double display=rate.hz();double goal=60;
 switch(mode){
 case FpsMode::Standard:goal=std::min(60.0,display);break;
 case FpsMode::Auto:goal=display;break;
 case FpsMode::Custom:goal=std::min(display,std::isfinite(custom) && custom>=1 ? custom : 60.0);break;
 case FpsMode::Diagnostic:goal=0;break;
 }
 return background && mode!=FpsMode::Diagnostic ? std::min(goal,30.0) : goal;
}
FrameControl::~FrameControl(){if(timer_)CloseHandle(timer_);}
void FrameControl::log(const char* reason){
 FILE* file=nullptr;if(fopen_s(&file,"AKU9992-frame-control.log","a")!=0)return;
 const char* name=mode_==FpsMode::Auto?"Auto":mode_==FpsMode::Custom?"Custom":mode_==FpsMode::Diagnostic?"Diagnostic":"Standard";
 fprintf(file,"AKU9992: %s; mode=%s; refresh=%u/%u; target=%.9f; background=%d; vsync=%d; status=%lu\n",reason,name,rate_.numerator,rate_.denominator,target_,background_?1:0,vsync_?1:0,static_cast<unsigned long>(rate_.error));fclose(file);
}
void FrameControl::configure(HWND window){
 window_=window;
 LARGE_INTEGER f{};QueryPerformanceFrequency(&f);frequency_=f.QuadPart;
 if(!timer_)timer_=CreateWaitableTimerExW(nullptr,nullptr,0x00000002,TIMER_ALL_ACCESS);
 if(!timer_)timer_=CreateWaitableTimerW(nullptr,FALSE,nullptr);
 const char* mode=getenv("GENERALS_FPS_MODE");mode_=FpsMode::Standard;
 if(mode && _stricmp(mode,"Auto")==0)mode_=FpsMode::Auto;
 else if(mode && _stricmp(mode,"Custom")==0)mode_=FpsMode::Custom;
 const bool diagnostic=testTimingEnabled();
 if(diagnostic && ((mode && _stricmp(mode,"Diagnostic")==0) || (getenv("GENERALS_RENDER_FPS") && strcmp(getenv("GENERALS_RENDER_FPS"),"0")==0)))mode_=FpsMode::Diagnostic;
 custom_=60;const char* custom=getenv("GENERALS_CUSTOM_FPS");
 if(custom){char* end=nullptr;const double parsed=strtod(custom,&end);if(end && *end=='\0' && std::isfinite(parsed) && parsed>=1 && parsed<=1000)custom_=parsed;}
 const char* sync=getenv("GENERALS_VSYNC");vsync_=sync && strcmp(sync,"1")==0;
 configured_=true;monitor_=nullptr;nextPoll_=0;reset();
 refresh(last_);
}
void FrameControl::refresh(LONGLONG now){
 const HMONITOR monitor=MonitorFromWindow(window_,MONITOR_DEFAULTTONEAREST);
 const HWND foreground=GetForegroundWindow();DWORD foregroundProcess=0;GetWindowThreadProcessId(foreground,&foregroundProcess);
 const bool background=IsIconic(window_) || (foregroundProcess!=GetCurrentProcessId());
 const bool test=testTimingEnabled();
 const bool effectiveBackground=background && !test;
 if(monitor!=monitor_ || now>=nextPoll_ || effectiveBackground!=background_){
  const bool initial=nextPoll_==0;const auto previous=rate_;const double previousTarget=target_;const bool oldBackground=background_;
  rate_=activeDisplayRate(window_);monitor_=monitor;background_=effectiveBackground;target_=frameTarget(mode_,custom_,rate_,background_);
  nextPoll_=now+frequency_;
  if(previousTarget!=target_ || previous.numerator!=rate_.numerator || previous.denominator!=rate_.denominator || oldBackground!=background_ || initial || previous.error!=rate_.error){
   deadline_=double(now);log(rate_.exact?"active display/settings updated":"refresh unavailable; safe 60 Hz fallback");
  }
 }
}
void FrameControl::reset(){LARGE_INTEGER t{};QueryPerformanceCounter(&t);last_=t.QuadPart;deadline_=double(last_);}
double FrameControl::waitFrame(){
 if(!configured_){configure(GetActiveWindow());}
 LARGE_INTEGER now{};QueryPerformanceCounter(&now);refresh(now.QuadPart);
 if(target_>0 && frequency_>0){
  const double period=double(frequency_)/target_;
  if(deadline_<=double(last_))deadline_=double(last_)+period;
  for(;;){
   QueryPerformanceCounter(&now);const double remaining=(deadline_-double(now.QuadPart))/double(frequency_);
   if(remaining<=0)break;
   if(timer_){LARGE_INTEGER due{};due.QuadPart=-std::max<LONGLONG>(1,static_cast<LONGLONG>(remaining*10000000.0));
    if(SetWaitableTimer(timer_,&due,0,nullptr,nullptr,FALSE)){WaitForSingleObject(timer_,INFINITE);continue;}
   }
   Sleep(remaining>0.001 ? static_cast<DWORD>(remaining*1000) : 1);
  }
  deadline_+=period;
  if(deadline_<=double(now.QuadPart))deadline_=double(now.QuadPart)+period;
 }
 QueryPerformanceCounter(&now);const double elapsed=frequency_>0 ? double(now.QuadPart-last_)/double(frequency_) : 1.0/60;
 last_=now.QuadPart;return std::max(0.000001,elapsed);
}
FrameControl& frameControl(){static FrameControl value;return value;}
}
extern "C" __declspec(dllexport) double generalsNativeWaitFrame(){return generals_mods::native12::frameControl().waitFrame();}
extern "C" __declspec(dllexport) void generalsNativeResetFrameClock(){generals_mods::native12::frameControl().reset();}
extern "C" __declspec(dllexport) double generalsNativeGetFrameTarget(){return generals_mods::native12::frameControl().target();}extern "C" __declspec(dllexport) double generalsNativeGetDisplayRate(HWND window){return generals_mods::native12::activeDisplayRate(window).hz();}
extern "C" __declspec(dllexport) int generalsNativeDisplayRateExact(HWND window){return generals_mods::native12::activeDisplayRate(window).exact?1:0;}
