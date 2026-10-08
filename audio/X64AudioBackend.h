// SPDX-License-Identifier: GPL-3.0-or-later
// Native x64 implementation of the Miles API subset used by the game.
// Original game event scheduling, priorities and distance attenuation remain in MilesAudioManager.
#pragma once
#include "mss/mss.h"
#include <xaudio2.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <shlwapi.h>
#include <wrl/client.h>
#include <vector>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <unordered_set>
#pragma comment(lib,"xaudio2.lib")
#pragma comment(lib,"mfplat.lib")
#pragma comment(lib,"mfreadwrite.lib")
#pragma comment(lib,"mfuuid.lib")
#pragma comment(lib,"shlwapi.lib")
#pragma comment(lib,"ole32.lib")
namespace nativeMiles {
using Microsoft::WRL::ComPtr;
inline void log(const char* text,HRESULT hr=S_OK) {FILE* f=nullptr;if(!fopen_s(&f,"GeneralsAudio.log","a")&&f){fprintf(f,"PID %lu: %s: 0x%08lX\n",GetCurrentProcessId(),text,(unsigned long)hr);fclose(f);}}
struct PCM {WAVEFORMATEX format{};std::vector<unsigned char> bytes;};
struct Wav {const unsigned char* format=nullptr;UINT formatSize=0;const unsigned char* data=nullptr;UINT size=0;};
inline UINT u32(const void* p){UINT v;memcpy(&v,p,4);return v;}
inline WORD u16(const void* p){WORD v;memcpy(&v,p,2);return v;}
inline bool wav(const void* image,UINT size,Wav& out) {
    if(!image || size<12) return false;const auto* p=(const unsigned char*)image;
    if(memcmp(p,"RIFF",4)||memcmp(p+8,"WAVE",4)) return false;
    for(UINT pos=12;pos<=size-8;) {UINT length=u32(p+pos+4);if(length>size-pos-8)return false;
        if(!memcmp(p+pos,"fmt ",4)){out.format=p+pos+8;out.formatSize=length;}
        if(!memcmp(p+pos,"data",4)){out.data=p+pos+8;out.size=length;}
        const UINT64 next=UINT64(pos)+8+length+(length&1);if(next>size)break;pos=UINT(next);
    } return out.format&&out.formatSize>=16&&out.data;
}
inline UINT imageSize(const void* image){if(!image||memcmp(image,"RIFF",4))return 0;UINT n=u32((const unsigned char*)image+4);return n<256*1024*1024 ? n+8:0;}
inline bool decodeWav(const void* image,UINT length,PCM& pcm) {
    Wav w;if(!wav(image,length,w))return false;
    const WORD tag=u16(w.format),channels=u16(w.format+2),bits=u16(w.format+14),align=u16(w.format+12);
    const UINT rate=u32(w.format+4);if(!channels||channels>2||!rate||!align)return false;
    pcm.format={WAVE_FORMAT_PCM,channels,rate,rate*channels*2,WORD(channels*2),16,0};
    if(tag==WAVE_FORMAT_PCM&&(bits==8||bits==16)) {
        if(bits==16)pcm.bytes.assign(w.data,w.data+w.size);
        else {pcm.bytes.resize(size_t(w.size)*2);for(UINT i=0;i<w.size;++i){short s=short((int(w.data[i])-128)*256);memcpy(pcm.bytes.data()+i*2,&s,2);}}
        return !pcm.bytes.empty();
    }
    if(tag!=WAVE_FORMAT_IMA_ADPCM||bits!=4||align<channels*4)return false;
    static const int steps[]={7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767};
    static const int changes[]={-1,-1,-1,-1,2,4,6,8};
    std::vector<short> samples;
    for(UINT start=0;start<w.size;start+=align) {
        const UINT n=std::min<UINT>(align,w.size-start);if(n<channels*4)break;
        const auto* block=w.data+start;int value[2]{},index[2]{};std::vector<short> lane[2];
        for(UINT c=0;c<channels;++c){value[c]=short(u16(block+c*4));index[c]=std::min<int>(block[c*4+2],88);lane[c].push_back(short(value[c]));}
        auto nibble=[&](UINT c,int code){int step=steps[index[c]],delta=step/8;if(code&1)delta+=step/4;if(code&2)delta+=step/2;if(code&4)delta+=step;value[c]=std::clamp(value[c]+((code&8)?-delta:delta),-32768,32767);index[c]=std::clamp(index[c]+changes[code&7],0,88);lane[c].push_back(short(value[c]));};
        UINT pos=channels*4;while(pos<n)for(UINT c=0;c<channels&&pos<n;++c)for(UINT b=0;b<4&&pos<n;++b){int v=block[pos++];nibble(c,v&15);nibble(c,v>>4);}
        size_t count=lane[0].size();if(channels==2)count=std::min(count,lane[1].size());
        for(size_t i=0;i<count;++i)for(UINT c=0;c<channels;++c)samples.push_back(lane[c][i]);
    }
    pcm.bytes.resize(samples.size()*2);memcpy(pcm.bytes.data(),samples.data(),pcm.bytes.size());return !pcm.bytes.empty();
}
inline bool decode(const std::vector<unsigned char>& file,PCM& pcm) {
    if(decodeWav(file.data(),UINT(file.size()),pcm))return true;
    ComPtr<IStream> stream;stream.Attach(SHCreateMemStream(file.data(),UINT(file.size())));if(!stream)return false;
    ComPtr<IMFByteStream> bytes;ComPtr<IMFSourceReader> reader;ComPtr<IMFMediaType> requested,current;
    HRESULT hr=MFCreateMFByteStreamOnStream(stream.Get(),&bytes);
    if(SUCCEEDED(hr))hr=MFCreateSourceReaderFromByteStream(bytes.Get(),nullptr,&reader);
    if(SUCCEEDED(hr))hr=MFCreateMediaType(&requested);
    if(SUCCEEDED(hr))hr=requested->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Audio);
    if(SUCCEEDED(hr))hr=requested->SetGUID(MF_MT_SUBTYPE,MFAudioFormat_PCM);
    if(SUCCEEDED(hr))hr=reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM,nullptr,requested.Get());
    if(SUCCEEDED(hr))hr=reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM,&current);
    WAVEFORMATEX* format=nullptr;UINT formatSize=0;
    if(SUCCEEDED(hr))hr=MFCreateWaveFormatExFromMFMediaType(current.Get(),&format,&formatSize);
    if(SUCCEEDED(hr)){pcm.format=*format;CoTaskMemFree(format);}
    while(SUCCEEDED(hr)) {DWORD flags=0;ComPtr<IMFSample> sample;hr=reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM,0,nullptr,&flags,nullptr,&sample);
        if(FAILED(hr)||flags&MF_SOURCE_READERF_ENDOFSTREAM)break;
        if(sample){ComPtr<IMFMediaBuffer> buffer;hr=sample->ConvertToContiguousBuffer(&buffer);BYTE* data=nullptr;DWORD count=0;
            if(SUCCEEDED(hr))hr=buffer->Lock(&data,nullptr,&count);if(SUCCEEDED(hr)){if(pcm.bytes.size()+count>256*1024*1024){buffer->Unlock();return false;}pcm.bytes.insert(pcm.bytes.end(),data,data+count);buffer->Unlock();}}
    }
    if(FAILED(hr))log("Audio decode failed",hr);return SUCCEEDED(hr)&&!pcm.bytes.empty();
}
inline ComPtr<IXAudio2> engine;inline IXAudio2MasteringVoice* master=nullptr;
inline bool mfStarted=false,comStarted=false;inline DIG_DRIVER driver{};
inline AIL_file_open_callback openFile=nullptr;inline AIL_file_close_callback closeFile=nullptr;
inline AIL_file_seek_callback seekFile=nullptr;inline AIL_file_read_callback readFile=nullptr;
struct Sample;inline std::unordered_set<Sample*> handles;
inline float listenerPos[3]{},listenerFace[3]{0,1,0};
inline h3DPOBJECT listener{};
struct Sample:IXAudio2VoiceCallback {
    PCM pcm;IXAudio2SourceVoice* voice=nullptr;float volume=1,pan=.5f;S32 rate=0,user[8]{};UINT loops=1;
    bool spatial=false;float pos[3]{};UINT beginFrame=0;UINT64 baseline=0;std::atomic<bool> playing{false},completed{false};
    std::atomic<AIL_sample_callback> sampleEnd{nullptr};std::atomic<AIL_3dsample_callback> spatialEnd{nullptr};std::atomic<AIL_stream_callback> streamEnd{nullptr};
    void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) override {} void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}
    // The game may replace/destroy the source in its EOS handler. Dispatch on
    // the game update thread; DestroyVoice is forbidden from an XAudio2 callback.
    void STDMETHODCALLTYPE OnStreamEnd() override {if(playing.exchange(false))completed=true;}
    void STDMETHODCALLTYPE OnBufferStart(void*) override {} void STDMETHODCALLTYPE OnBufferEnd(void*) override {} void STDMETHODCALLTYPE OnLoopEnd(void*) override {}
    void STDMETHODCALLTYPE OnVoiceError(void*,HRESULT hr) override {log("Source voice failed",hr);}
    void destroy(){playing=false;if(voice){voice->DestroyVoice();voice=nullptr;}completed=false;}
    ~Sample(){destroy();}
    void apply(){if(!voice)return;float p=pan;if(spatial){float dx=pos[0]-listenerPos[0],dy=pos[1]-listenerPos[1];float distance=std::sqrt(dx*dx+dy*dy);float norm=std::sqrt(listenerFace[0]*listenerFace[0]+listenerFace[1]*listenerFace[1]);if(distance>0&&norm>0)p=.5f+.5f*(dx*listenerFace[1]-dy*listenerFace[0])/(distance*norm);}
        p=std::clamp(p,0.f,1.f);voice->SetVolume(std::max(0.f,volume));float matrix[4]{};
        if(pcm.format.nChannels==1){matrix[0]=std::sqrt(1-p);matrix[1]=std::sqrt(p);}
        else {matrix[0]=std::min(1.f,2*(1-p));matrix[3]=std::min(1.f,2*p);}
        voice->SetOutputMatrix(master,pcm.format.nChannels,2,matrix);if(rate>0)voice->SetFrequencyRatio(std::clamp(float(rate)/pcm.format.nSamplesPerSec,.01f,4.f));
    }
    bool set(PCM&& data){destroy();pcm=std::move(data);beginFrame=0;rate=pcm.format.nSamplesPerSec;HRESULT hr=engine?engine->CreateSourceVoice(&voice,&pcm.format,0,4,this):E_UNEXPECTED;if(FAILED(hr)){log("Create source voice failed",hr);return false;}apply();return true;}
    void start(){if(!voice||pcm.bytes.empty())return;playing=false;completed=false;voice->Stop();voice->FlushSourceBuffers();XAUDIO2_VOICE_STATE state{};voice->GetState(&state);baseline=state.SamplesPlayed;
        XAUDIO2_BUFFER buffer{};buffer.Flags=XAUDIO2_END_OF_STREAM;buffer.AudioBytes=UINT(pcm.bytes.size());buffer.pAudioData=pcm.bytes.data();buffer.PlayBegin=std::min<UINT>(beginFrame,buffer.AudioBytes/pcm.format.nBlockAlign-1);
        if(!loops||loops>XAUDIO2_MAX_LOOP_COUNT+1){buffer.LoopCount=XAUDIO2_LOOP_INFINITE;}else if(loops>1)buffer.LoopCount=loops-1;
        if(buffer.LoopCount){buffer.LoopBegin=buffer.PlayBegin;buffer.LoopLength=buffer.AudioBytes/pcm.format.nBlockAlign-buffer.PlayBegin;}
        HRESULT hr=voice->SubmitSourceBuffer(&buffer);if(SUCCEEDED(hr)){playing=true;hr=voice->Start();}if(FAILED(hr))log("Submit audio failed",hr);
        static UINT logged=0;if(logged++<8)log(spatial?"3D game sound submitted":"2D/music game sound submitted",hr);
    }
    UINT position(){if(!voice||!pcm.format.nBlockAlign)return 0;XAUDIO2_VOICE_STATE s{};voice->GetState(&s);UINT frames=UINT(pcm.bytes.size()/pcm.format.nBlockAlign);return frames ? UINT((beginFrame+s.SamplesPlayed-baseline)%frames):0;}
};
template<class T> inline Sample* sample(T value){return reinterpret_cast<Sample*>(value);}
inline Sample* allocate(){auto* s=new Sample;handles.insert(s);return s;}
inline void release(Sample* s){if(s){handles.erase(s);delete s;}}
inline void pump(){std::vector<Sample*> pending;for(auto* s:handles)if(s->completed.exchange(false))pending.push_back(s);
    for(auto* s:pending){if(!handles.count(s))continue;if(auto cb=s->sampleEnd.load())cb((HSAMPLE)s);else if(auto cb=s->spatialEnd.load())cb((H3DSAMPLE)s);else if(auto cb=s->streamEnd.load())cb((HSTREAM)s);}}
inline bool loadFile(const char* name,PCM& pcm){std::vector<unsigned char> file;void* handle=nullptr;
    if(openFile&&openFile(name,&handle)){S32 size=seekFile(handle,0,2);seekFile(handle,0,0);if(size>0&&size<256*1024*1024){file.resize(size);UINT done=0;while(done<file.size()){U32 n=readFile(handle,file.data()+done,U32(file.size()-done));if(!n)break;done+=n;}file.resize(done);}closeFile(handle);}
    else {FILE* f=nullptr;if(!fopen_s(&f,name,"rb")&&f){fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);if(size>0&&size<256*1024*1024){file.resize(size);file.resize(fread(file.data(),1,size,f));}fclose(f);}}
    if(file.empty()){log("Audio asset read failed");return false;}return decode(file,pcm);
}
inline S32 startup(){HRESULT hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED);comStarted=SUCCEEDED(hr);hr=MFStartup(MF_VERSION,MFSTARTUP_LITE);mfStarted=SUCCEEDED(hr);return 1;}
inline S32 quick_startup(S32,S32,U32,S32,S32){if(engine)return 1;HRESULT hr=XAudio2Create(&engine);if(SUCCEEDED(hr))hr=engine->CreateMasteringVoice(&master,2);log("Native x64 XAudio2 output initialized",hr);return SUCCEEDED(hr);}
inline void shutdown(){while(!handles.empty())release(*handles.begin());if(master){master->DestroyVoice();master=nullptr;}engine.Reset();if(mfStarted){MFShutdown();mfStarted=false;}if(comStarted){CoUninitialize();comStarted=false;}log("Native audio shutdown completed");}
inline void quick_handles(HDIGDRIVER* d,HMDIDRIVER* m,HDLSDEVICE* l){if(d)*d=engine?&driver:nullptr;if(m)*m=nullptr;if(l)*l=nullptr;}
inline void set_file_callbacks(AIL_file_open_callback o,AIL_file_close_callback c,AIL_file_seek_callback s,AIL_file_read_callback r){openFile=o;closeFile=c;seekFile=s;readFile=r;}
inline HSAMPLE allocate_sample_handle(HDIGDRIVER){return (HSAMPLE)allocate();}
inline H3DSAMPLE allocate_3D_sample_handle(HPROVIDER){auto* s=allocate();s->spatial=true;return (H3DSAMPLE)s;}
inline void release_sample_handle(HSAMPLE h){release(sample(h));} inline void release_3D_sample_handle(H3DSAMPLE h){release(sample(h));}
inline void init_sample(HSAMPLE h){auto* s=sample(h);s->destroy();s->rate=0;s->loops=1;s->beginFrame=0;}
inline S32 set_sample_file(HSAMPLE h,const void* image,S32){PCM pcm;return decodeWav(image,imageSize(image),pcm)&&sample(h)->set(std::move(pcm));}
inline S32 set_3D_sample_file(H3DSAMPLE h,const void* image){return set_sample_file((HSAMPLE)h,image,0);}
inline void start_sample(HSAMPLE h){sample(h)->start();} inline void start_3D_sample(H3DSAMPLE h){sample(h)->start();}
inline void stop_sample(HSAMPLE h){if(sample(h)->voice)sample(h)->voice->Stop();} inline void stop_3D_sample(H3DSAMPLE h){stop_sample((HSAMPLE)h);}
inline void resume_sample(HSAMPLE h){if(sample(h)->voice)sample(h)->voice->Start();} inline void resume_3D_sample(H3DSAMPLE h){resume_sample((HSAMPLE)h);}
inline void end_sample(HSAMPLE h){sample(h)->playing=false;if(sample(h)->voice){sample(h)->voice->Stop();sample(h)->voice->FlushSourceBuffers();}}
inline void end_3D_sample(H3DSAMPLE h){end_sample((HSAMPLE)h);}
inline void set_sample_volume_pan(HSAMPLE h,F32 v,F32 p){auto* s=sample(h);s->volume=v;s->pan=p;s->apply();}
inline void sample_volume_pan(HSAMPLE h,F32* v,F32* p){if(v)*v=sample(h)->volume;if(p)*p=sample(h)->pan;}
inline void set_3D_sample_volume(H3DSAMPLE h,F32 v){sample(h)->volume=v;sample(h)->apply();} inline F32 sample_3D_volume(H3DSAMPLE h){return sample(h)->volume;}
inline void set_sample_playback_rate(HSAMPLE h,S32 r){sample(h)->rate=r;sample(h)->apply();} inline S32 sample_playback_rate(HSAMPLE h){return sample(h)->rate;}
inline void set_3D_sample_playback_rate(H3DSAMPLE h,S32 r){set_sample_playback_rate((HSAMPLE)h,r);} inline S32 sample_3D_playback_rate(H3DSAMPLE h){return sample(h)->rate;}
inline void set_sample_user_data(HSAMPLE h,U32 i,S32 v){if(i<8)sample(h)->user[i]=v;} inline S32 sample_user_data(HSAMPLE h,U32 i){return i<8?sample(h)->user[i]:0;}
inline void set_3D_user_data(H3DPOBJECT h,U32 i,S32 v){if(h!=&listener)set_sample_user_data((HSAMPLE)h,i,v);} inline S32 sample_3D_user_data(H3DPOBJECT h,U32 i){return h!=&listener?sample_user_data((HSAMPLE)h,i):0;}
inline AIL_sample_callback register_EOS_callback(HSAMPLE h,AIL_sample_callback cb){return sample(h)->sampleEnd.exchange(cb);}
inline AIL_3dsample_callback register_3D_EOS_callback(H3DSAMPLE h,AIL_3dsample_callback cb){return sample(h)->spatialEnd.exchange(cb);}
inline AIL_stream_callback register_stream_callback(HSTREAM h,AIL_stream_callback cb){return sample(h)->streamEnd.exchange(cb);}
inline void set_sample_loop_count(HSAMPLE h,S32 n){sample(h)->loops=n;} inline S32 sample_loop_count(HSAMPLE h){return sample(h)->loops;}
inline void set_3D_sample_loop_count(H3DSAMPLE h,U32 n){sample(h)->loops=n;} inline U32 sample_3D_loop_count(H3DSAMPLE h){return sample(h)->loops;}
inline void set_stream_loop_count(HSTREAM h,S32 n){sample(h)->loops=n;} inline S32 stream_loop_count(HSTREAM h){return sample(h)->loops;}
inline void sample_ms_position(HSAMPLE h,S32* total,S32* current){auto* s=sample(h);if(total)*total=s->pcm.format.nAvgBytesPerSec?S32(UINT64(s->pcm.bytes.size())*1000/s->pcm.format.nAvgBytesPerSec):0;if(current)*current=s->pcm.format.nSamplesPerSec?S32(UINT64(s->position())*1000/s->pcm.format.nSamplesPerSec):0;}
inline void stream_ms_position(HSTREAM h,S32* t,S32* c){sample_ms_position((HSAMPLE)h,t,c);}
inline void set_sample_ms_position(HSAMPLE h,S32 p){auto* s=sample(h);s->beginFrame=UINT(std::max<S32>(p,0))*s->pcm.format.nSamplesPerSec/1000;if(s->playing)s->start();}
inline void set_stream_ms_position(HSTREAM h,S32 p){set_sample_ms_position((HSAMPLE)h,p);}
inline HSTREAM open_stream(HDIGDRIVER,const char* name,S32){PCM pcm;if(!loadFile(name,pcm))return nullptr;auto* s=allocate();if(!s->set(std::move(pcm))){release(s);return nullptr;}return (HSTREAM)s;}
inline void close_stream(HSTREAM h){release(sample(h));} inline void start_stream(HSTREAM h){sample(h)->start();}
inline void pause_stream(HSTREAM h,S32 on){if(on)stop_sample((HSAMPLE)h);else resume_sample((HSAMPLE)h);}
inline void set_stream_volume_pan(HSTREAM h,F32 v,F32 p){set_sample_volume_pan((HSAMPLE)h,v,p);} inline void stream_volume_pan(HSTREAM h,F32* v,F32* p){sample_volume_pan((HSAMPLE)h,v,p);}
inline void set_stream_playback_rate(HSTREAM h,S32 r){set_sample_playback_rate((HSAMPLE)h,r);} inline S32 stream_playback_rate(HSTREAM h){return sample(h)->rate;}
inline HAUDIO quick_load_and_play(const char* name,U32 loops,S32){auto h=open_stream(nullptr,name,0);if(h){sample(h)->loops=loops;sample(h)->start();}return (HAUDIO)h;}
inline void quick_set_volume(HAUDIO h,F32 v,F32 p){if(h)set_sample_volume_pan((HSAMPLE)h,v,p);} inline void quick_unload(HAUDIO h){release(sample(h));}
inline S32 enumerate_3D_providers(HPROENUM* next,HPROVIDER* dest,char** name){static char provider[]="Miles Fast 2D Positional Audio";if(!next||*next)return 0;*next=1;*dest=&driver;*name=provider;return 1;}
inline M3DRESULT open_3D_provider(HPROVIDER){return engine?0:1;} inline H3DPOBJECT open_3D_listener(HPROVIDER){return &listener;}
inline void set_3D_position(H3DPOBJECT h,F32 x,F32 y,F32 z){float* p=h==&listener?listenerPos:sample(h)->pos;p[0]=x;p[1]=y;p[2]=z;if(h==&listener){for(auto* s:handles)if(s->spatial)s->apply();}else sample(h)->apply();}
inline void set_3D_orientation(H3DPOBJECT h,F32 x,F32 y,F32 z,F32,F32,F32){if(h==&listener){listenerFace[0]=x;listenerFace[1]=y;listenerFace[2]=z;}}
inline S32 WAV_info(const void* image,AILSOUNDINFO* info){if(!info)return 0;*info={};Wav w;if(!wav(image,imageSize(image),w))return 0;info->format=u16(w.format);info->channels=u16(w.format+2);info->rate=u32(w.format+4);info->bits=u16(w.format+14);info->block_size=u16(w.format+12);info->data_ptr=w.data;info->data_len=w.size;info->initial_ptr=image;info->samples=info->bits?U32(UINT64(w.size)*8/(info->bits*info->channels)):0;return 1;}
inline void put16(unsigned char* p,WORD n){memcpy(p,&n,2);} inline void put32(unsigned char* p,UINT n){memcpy(p,&n,4);}
inline S32 decompress_ADPCM(const AILSOUNDINFO* info,void** out,U32* size){if(!info||!out||!size)return 0;*out=nullptr;*size=0;PCM pcm;if(!decodeWav(info->initial_ptr,imageSize(info->initial_ptr),pcm))return 0;*size=U32(pcm.bytes.size()+44);auto* p=(unsigned char*)malloc(*size);if(!p)return 0;memcpy(p,"RIFF",4);put32(p+4,*size-8);memcpy(p+8,"WAVEfmt ",8);put32(p+16,16);memcpy(p+20,&pcm.format,16);memcpy(p+36,"data",4);put32(p+40,UINT(pcm.bytes.size()));memcpy(p+44,pcm.bytes.data(),pcm.bytes.size());*out=p;return 1;}
inline void mem_free_lock(void* p){free(p);}
} // namespace nativeMiles
