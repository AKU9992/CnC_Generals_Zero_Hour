#define NOMINMAX
#include "../../audio/X64AudioBackend.h"
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <tlhelp32.h>
#include <stdexcept>
#pragma comment(lib,"uuid.lib")
using namespace nativeMiles;
static std::atomic<unsigned> finished{0};
static void __stdcall complete(HSAMPLE){++finished;}
static float peak(DWORD pid) {
    ComPtr<IMMDeviceEnumerator> enumerator;ComPtr<IMMDevice> endpoint;ComPtr<IAudioSessionManager2> manager;ComPtr<IAudioSessionEnumerator> sessions;
    HRESULT hr=CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator));
    if(SUCCEEDED(hr))hr=enumerator->GetDefaultAudioEndpoint(eRender,eMultimedia,&endpoint);
    if(SUCCEEDED(hr))hr=endpoint->Activate(__uuidof(IAudioSessionManager2),CLSCTX_ALL,nullptr,(void**)manager.GetAddressOf());
    if(SUCCEEDED(hr))hr=manager->GetSessionEnumerator(&sessions);if(FAILED(hr))return 0;
    int count=0;sessions->GetCount(&count);float result=0;
    for(int i=0;i<count;++i){ComPtr<IAudioSessionControl> control;ComPtr<IAudioSessionControl2> process;ComPtr<IAudioMeterInformation> meter;DWORD id=0;
        if(SUCCEEDED(sessions->GetSession(i,&control))&&SUCCEEDED(control.As(&process))&&SUCCEEDED(process->GetProcessId(&id))&&id==pid&&SUCCEEDED(control.As(&meter))){float value=0;meter->GetPeakValue(&value);result=std::max(result,value);}}
    return result;
}
static DWORD findGame(const wchar_t* name){HANDLE h=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(h==INVALID_HANDLE_VALUE)return 0;PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);DWORD result=0;
    if(Process32FirstW(h,&entry))do{if(!_wcsicmp(entry.szExeFile,name)){result=entry.th32ProcessID;break;}}while(Process32NextW(h,&entry));CloseHandle(h);return result;}
static void require(bool valid,const char* text){if(!valid)throw std::runtime_error(text);}
int wmain(int argc,wchar_t** argv){try{
    startup();
    if(argc==3&&!wcscmp(argv[1],L"--watch")){float maxPeak=0;DWORD game=argv[2][0]>=L'0'&&argv[2][0]<=L'9'?wcstoul(argv[2],nullptr,10):0;for(UINT i=0;i<550;++i){if(!game)game=findGame(argv[2]);if(game)maxPeak=std::max(maxPeak,peak(game));if(maxPeak>.0001f)break;Sleep(100);}printf("Game PID %lu: audio session peak %.6f\n",game,maxPeak);shutdown();require(maxPeak>.0001f,"game audio session remained silent");puts("PASS: actual game audio reaches Windows output session.");return 0;}
    require(quick_startup(1,0,44100,16,2)!=0,"open XAudio2 output");
    PCM tone;tone.format={WAVE_FORMAT_PCM,1,44100,88200,2,16,0};tone.bytes.resize(44100*2);
    for(UINT i=0;i<44100;++i){short value=short(8000*std::sin(i*6.28318530718*440/44100));memcpy(tone.bytes.data()+i*2,&value,2);}
    auto* s=allocate();require(s->set(std::move(tone)),"tone source");register_EOS_callback((HSAMPLE)s,complete);s->start();
    float maximum=0;for(UINT i=0;i<10;++i){Sleep(50);maximum=std::max(maximum,peak(GetCurrentProcessId()));}
    require(maximum>.001f,"native source output is silent");stop_sample((HSAMPLE)s);UINT before=s->position();Sleep(80);require(s->position()==before,"pause failed");resume_sample((HSAMPLE)s);
    for(UINT i=0;i<30&&!finished;++i){Sleep(50);pump();}require(finished==1,"EOS callback missing or duplicated");release(s);
    // Known IMA ADPCM block: zero predictor and zero codes produce zero PCM.
    unsigned char ima[52]{};memcpy(ima,"RIFF",4);put32(ima+4,44);memcpy(ima+8,"WAVEfmt ",8);put32(ima+16,20);put16(ima+20,WAVE_FORMAT_IMA_ADPCM);put16(ima+22,1);put32(ima+24,22050);put16(ima+32,8);put16(ima+34,4);put16(ima+36,2);put16(ima+38,9);memcpy(ima+40,"data",4);put32(ima+44,4);
    PCM decoded;require(decodeWav(ima,sizeof(ima),decoded)&&decoded.bytes.size()==2,"IMA decoder");
    for(int i=1;i<argc;++i){int n=WideCharToMultiByte(CP_ACP,0,argv[i],-1,nullptr,0,nullptr,nullptr);std::vector<char> name(n);WideCharToMultiByte(CP_ACP,0,argv[i],-1,name.data(),n,nullptr,nullptr);PCM asset;require(loadFile(name.data(),asset),"game asset decode");printf("Asset decoded: %s, %u Hz, %u channels, %zu PCM bytes\n",name.data(),asset.format.nSamplesPerSec,asset.format.nChannels,asset.bytes.size());auto* h=allocate();require(h->set(std::move(asset)),"asset voice");h->spatial=true;h->pos[0]=100;h->apply();h->start();Sleep(100);release(h);}
    shutdown();require(handles.empty()&&!engine&&!master,"audio shutdown resources");printf("PASS: audible output peak %.4f, PCM/ADPCM, pause/resume, EOS callback and cleanup.\n",maximum);return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL: %s\n",error.what());shutdown();return 1;}}
