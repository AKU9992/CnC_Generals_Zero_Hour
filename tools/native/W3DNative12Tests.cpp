#include <d3dx8.h>
#include <windows.h>
#include <cstdio>
#include <vector>
#include <stdexcept>
#include <wrl/client.h>
static void check(HRESULT hr,const char* operation){if(FAILED(hr)){char text[200];sprintf_s(text,"%s: 0x%08lX",operation,static_cast<unsigned long>(hr));throw std::runtime_error(text);}}
static void pixel(IDirect3DDevice8* device,UINT x,UINT y,UINT red,UINT green,UINT blue,const char* operation){
    check(device->EndScene(),"EndScene");check(device->Present(nullptr,nullptr,nullptr,nullptr),"Present");
    FILE* file=nullptr;if(fopen_s(&file,"W3DNative12Tests.native.bmp","rb")!=0)throw std::runtime_error("GPU capture missing");
    BITMAPFILEHEADER header{};BITMAPINFOHEADER info{};fread(&header,1,sizeof(header),file);fread(&info,1,sizeof(info),file);
    uint8_t value[4]{};fseek(file,header.bfOffBits+(y*640+x)*4,SEEK_SET);fread(value,1,4,file);fclose(file);
    if(abs(int(value[0])-int(blue))>3 || abs(int(value[1])-int(green))>3 || abs(int(value[2])-int(red))>3){
        char text[256];sprintf_s(text,"%s: actual RGB %u,%u,%u; expected %u,%u,%u",operation,value[2],value[1],value[0],red,green,blue);throw std::runtime_error(text);}
}
static Microsoft::WRL::ComPtr<IDirect3DTexture8> solid(IDirect3DDevice8* device,DWORD color){
    Microsoft::WRL::ComPtr<IDirect3DTexture8> texture;check(device->CreateTexture(4,4,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture),"solid texture");
    D3DLOCKED_RECT lock{};check(texture->LockRect(0,&lock,nullptr,0),"solid lock");
    for(UINT y=0;y<4;++y)for(UINT x=0;x<4;++x)reinterpret_cast<DWORD*>(static_cast<uint8_t*>(lock.pBits)+y*lock.Pitch)[x]=color;
    check(texture->UnlockRect(0),"solid unlock");return texture;
}
int main(){
    HINSTANCE instance=GetModuleHandleW(nullptr);WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=instance;wc.lpszClassName=L"W3DNative12Tests";
    if(!RegisterClassW(&wc))return 2;HWND window=CreateWindowExW(0,wc.lpszClassName,L"W3D native tests",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,instance,nullptr);
    HMODULE module=LoadLibraryExW(L"generals-native12.dll",nullptr,LOAD_LIBRARY_SEARCH_APPLICATION_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);if(!module || !window)return 2;int result=0;
    try{
        SetEnvironmentVariableA("GENERALS_CAPTURE_FRAME","W3DNative12Tests");SetEnvironmentVariableA("GENERALS_CAPTURE_EVERY_FRAME","1");
        auto create=reinterpret_cast<IDirect3D8* (WINAPI*)(UINT)>(GetProcAddress(module,"Direct3DCreate8"));if(!create)throw std::runtime_error("native entry missing");
        Microsoft::WRL::ComPtr<IDirect3D8> api;api.Attach(create(D3D_SDK_VERSION));Microsoft::WRL::ComPtr<IDirect3DDevice8> device;
        D3DPRESENT_PARAMETERS p{};p.BackBufferWidth=640;p.BackBufferHeight=480;p.BackBufferFormat=D3DFMT_X8R8G8B8;p.BackBufferCount=1;
        p.SwapEffect=D3DSWAPEFFECT_DISCARD;p.hDeviceWindow=window;p.Windowed=TRUE;p.EnableAutoDepthStencil=TRUE;p.AutoDepthStencilFormat=D3DFMT_D24S8;
        check(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&p,&device),"CreateDevice");
        Microsoft::WRL::ComPtr<IDirect3DTexture8> texture;check(device->CreateTexture(64,64,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture),"CreateTexture");
        D3DLOCKED_RECT lock{};check(texture->LockRect(0,&lock,nullptr,0),"LockRect");
        for(UINT y=0;y<64;++y)for(UINT x=0;x<64;++x)reinterpret_cast<DWORD*>(static_cast<uint8_t*>(lock.pBits)+y*lock.Pitch)[x]=0xff00ff00;
        check(texture->UnlockRect(0),"UnlockRect");check(device->SetTexture(0,texture.Get()),"SetTexture");
        check(device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1),"color stage");
        check(device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE),"texture argument");
        check(device->SetVertexShader(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1),"FVF");
        check(device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE),"cull state");
        struct V{float x,y,z,w;DWORD color;float u,v;};const V triangle[]={{80,420,.5f,1,0xffffffff,0,1},{320,60,.5f,1,0xffffffff,.5f,0},{560,420,.5f,1,0xffffffff,1,1}};
        check(device->BeginScene(),"BeginScene");check(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,0xff000000,1,0),"Clear");
        check(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,triangle,sizeof(V)),"W3D draw");check(device->EndScene(),"EndScene");
        SetEnvironmentVariableA("GENERALS_CAPTURE_FRAME","W3DNative12Tests");check(device->Present(nullptr,nullptr,nullptr,nullptr),"Present");SetEnvironmentVariableA("GENERALS_CAPTURE_FRAME",nullptr);
        FILE* file=nullptr;if(fopen_s(&file,"W3DNative12Tests.native.bmp","rb")!=0)throw std::runtime_error("GPU capture missing");
        BITMAPFILEHEADER header{};BITMAPINFOHEADER info{};fread(&header,1,sizeof(header),file);fread(&info,1,sizeof(info),file);std::vector<uint8_t> pixels(640*480*4);
        fseek(file,header.bfOffBits,SEEK_SET);fread(pixels.data(),1,pixels.size(),file);fclose(file);
        auto* center=pixels.data()+(240*640+320)*4;if(center[0]>2 || center[1]<250 || center[2]>2)throw std::runtime_error("native textured W3D pixel incorrect");
        std::puts("PASS W3D ABI: native texture Lock/Upload/SRV, FVF, textured draw, DXGI Present, GPU readback");
        SetEnvironmentVariableA("GENERALS_CAPTURE_FRAME","W3DNative12Tests");
        // Projected shadows and water reflection render into textures, then sample GPU contents.
        Microsoft::WRL::ComPtr<IDirect3DTexture8> reflection;Microsoft::WRL::ComPtr<IDirect3DSurface8> surface,back,depth;
        check(device->CreateTexture(640,480,1,D3DUSAGE_RENDERTARGET,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&reflection),"reflection texture");
        check(reflection->GetSurfaceLevel(0,&surface),"reflection surface");check(device->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&back),"backbuffer");
        check(device->GetDepthStencilSurface(&depth),"default depth");check(device->BeginScene(),"reflection scene");
        check(device->SetTexture(0,nullptr),"unbind texture");check(device->SetRenderTarget(surface.Get(),nullptr),"reflection bind");
        check(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xffff0000,1,0),"reflection clear");
        check(device->SetRenderTarget(back.Get(),depth.Get()),"restore backbuffer");check(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,0xff000000,1,0),"back clear");
        check(device->SetTexture(0,reflection.Get()),"sample reflection");check(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,triangle,sizeof(V)),"reflection draw");
        pixel(device.Get(),320,240,255,0,0,"GPU render-to-texture");std::puts("PASS native render-to-texture reflection/projected shadow");
        // Volume shadows: write stencil without color, then darken only the mask.
        check(device->BeginScene(),"stencil scene");check(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,0xff808080,1,0),"stencil clear");
        check(device->SetTexture(0,nullptr),"stencil texture");check(device->SetRenderState(D3DRS_ZENABLE,FALSE),"stencil z");
        check(device->SetRenderState(D3DRS_STENCILENABLE,TRUE),"stencil enabled");check(device->SetRenderState(D3DRS_STENCILPASS,D3DSTENCILOP_REPLACE),"stencil replace");
        check(device->SetRenderState(D3DRS_STENCILREF,1),"stencil reference");check(device->SetRenderState(D3DRS_COLORWRITEENABLE,0),"mask only");
        check(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,triangle,sizeof(V)),"stencil mask draw");
        check(device->SetRenderState(D3DRS_COLORWRITEENABLE,15),"color writes");check(device->SetRenderState(D3DRS_STENCILFUNC,D3DCMP_EQUAL),"stencil equal");
        check(device->SetRenderState(D3DRS_STENCILPASS,D3DSTENCILOP_KEEP),"stencil keep");
        auto black=solid(device.Get(),0xff000000);check(device->SetTexture(0,black.Get()),"shadow color");
        const V quad[]={{0,0,.5f,1,0xffffffff,0,0},{640,0,.5f,1,0xffffffff,1,0},{0,480,.5f,1,0xffffffff,0,1},
                        {640,0,.5f,1,0xffffffff,1,0},{640,480,.5f,1,0xffffffff,1,1},{0,480,.5f,1,0xffffffff,0,1}};
        check(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,2,quad,sizeof(V)),"shadow overlay");pixel(device.Get(),320,240,0,0,0,"stencil shadow center");
        FILE* stencilFile=nullptr;fopen_s(&stencilFile,"W3DNative12Tests.native.bmp","rb");fseek(stencilFile,54+(10*640+10)*4,SEEK_SET);uint8_t corner[4]{};fread(corner,1,4,stencilFile);fclose(stencilFile);
        if(corner[0]!=128 || corner[1]!=128 || corner[2]!=128)throw std::runtime_error("stencil leaked outside shadow mask");
        std::puts("PASS native stencil volume shadow mask and untouched outside pixel");check(device->SetRenderState(D3DRS_STENCILENABLE,FALSE),"disable stencil");
        auto waterFactory=reinterpret_cast<BOOL(WINAPI*)(IDirect3DDevice8*,DWORD*,DWORD*,DWORD*,DWORD*,DWORD*)>(GetProcAddress(module,"GeneralsNativeCreateWaterShaders"));
        DWORD waveVS=0,wavePS=0,riverPS=0,reflectionPS=0,gridPS=0;if(!waterFactory || !waterFactory(device.Get(),&waveVS,&wavePS,&riverPS,&reflectionPS,&gridPS))throw std::runtime_error("native water factory missing");
        auto base=solid(device.Get(),0xff204060),noise=solid(device.Get(),0xff202020),sparkle=solid(device.Get(),0xff404040),shroud=solid(device.Get(),0xff808080);
        check(device->SetTexture(0,base.Get()),"water base");check(device->SetTexture(1,noise.Get()),"water noise");check(device->SetTexture(2,sparkle.Get()),"water sparkles");check(device->SetTexture(3,shroud.Get()),"water shroud");
        check(device->SetPixelShader(gridPS),"native grid water shader");check(device->BeginScene(),"grid water scene");check(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,triangle,sizeof(V)),"grid water draw");
        pixel(device.Get(),320,240,20,36,52,"native water sparkles/shroud");
        check(device->SetPixelShader(riverPS),"native river shader");check(device->BeginScene(),"river scene");check(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,triangle,sizeof(V)),"river draw");
        pixel(device.Get(),320,240,168,200,232,"native river highlights");
        check(device->SetPixelShader(reflectionPS),"native reflection shader");const float reflectionAmount[4]={.5f,.5f,.5f,.5f};check(device->SetPixelShaderConstant(0,reflectionAmount,1),"reflection amount");
        check(device->BeginScene(),"reflection water scene");check(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,triangle,sizeof(V)),"reflection water draw");pixel(device.Get(),320,240,64,96,128,"native water reflection");
        check(device->SetPixelShader(wavePS),"native wave shader");check(device->SetTexture(0,black.Get()),"flat bump");check(device->SetTexture(1,reflection.Get()),"wave GPU reflection");check(device->SetTexture(2,nullptr),"wave alpha off");
        check(device->BeginScene(),"wave scene");check(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,triangle,sizeof(V)),"wave draw");pixel(device.Get(),320,240,255,0,0,"native wave reflection");
        std::puts("PASS native HLSL water: waves, river highlights, grid sparkles/shroud, reflection strength");
        // The wave vertex program consumes the existing W3D constant registers.
        const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};const float projectionCoordinates[4]={.5f,-.5f,.5f,.5f};
        check(device->SetVertexShader(waveVS),"native wave vertex program");check(device->SetVertexShaderConstant(2,identity,4),"wave matrix");
        check(device->SetVertexShaderConstant(6,projectionCoordinates,1),"wave reflection coordinates");
        struct WaveVertex{float x,y,z;DWORD color;float u,v;};const WaveVertex waveTriangle[]={{-.8f,-.8f,.5f,0xffffffff,0,1},{0,.8f,.5f,0xffffffff,.5f,0},{.8f,-.8f,.5f,0xffffffff,1,1}};
        check(device->BeginScene(),"native wave vertex scene");check(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,waveTriangle,sizeof(WaveVertex)),"native wave vertex draw");
        pixel(device.Get(),320,240,255,0,0,"native projected wave vertex");
        check(device->SetPixelShader(0),"reset pixel shader");
        check(device->SetVertexShader(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1),"restore FVF");
        // Signed V8U8 bump values must move the next-stage sample, not multiply color.
        Microsoft::WRL::ComPtr<IDirect3DTexture8> bump,strip;
        check(device->CreateTexture(1,1,1,0,D3DFMT_V8U8,D3DPOOL_MANAGED,&bump),"signed bump texture");
        check(bump->LockRect(0,&lock,nullptr,0),"bump lock");static_cast<uint8_t*>(lock.pBits)[0]=64;static_cast<uint8_t*>(lock.pBits)[1]=0;check(bump->UnlockRect(0),"bump unlock");
        check(device->CreateTexture(4,1,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&strip),"bump reflection strip");
        check(strip->LockRect(0,&lock,nullptr,0),"strip lock");auto* colors=static_cast<DWORD*>(lock.pBits);colors[0]=colors[1]=0xff00ff00;colors[2]=colors[3]=0xff0000ff;check(strip->UnlockRect(0),"strip unlock");
        check(device->SetTexture(0,bump.Get()),"bump bind");check(device->SetTexture(1,strip.Get()),"bump reflection bind");
        check(device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_BUMPENVMAP),"bump operation");float scale=1.5f;DWORD scaleBits=0;memcpy(&scaleBits,&scale,4);
        check(device->SetTextureStageState(0,D3DTSS_BUMPENVMAT00,scaleBits),"bump matrix");
        check(device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_SELECTARG1),"bump next stage");
        check(device->SetTextureStageState(1,D3DTSS_COLORARG1,D3DTA_TEXTURE),"bump next argument");
        check(device->BeginScene(),"bump scene");check(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,triangle,sizeof(V)),"bump draw");pixel(device.Get(),320,240,0,0,255,"signed bump coordinate distortion");
        std::puts("PASS signed V8U8 water bump distortion and projected wave vertex constants");
        check(device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1),"restore color stage");check(device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE),"disable stage 1");
        // DDS preserves compressed mip payloads; TGA decodes and generates CPU mips.
        std::vector<uint8_t> dds(128+56);memcpy(dds.data(),"DDS ",4);
        auto field=[&](UINT offset,DWORD value){memcpy(dds.data()+offset,&value,4);};field(4,124);field(8,0xa1007);field(12,8);field(16,8);field(20,32);field(28,4);field(76,32);field(80,4);field(84,D3DFMT_DXT1);field(108,0x401008);
        for(UINT offset=128;offset<dds.size();offset+=8)dds[offset]=31;
        FILE* asset=nullptr;fopen_s(&asset,"W3DNative12Tests.dds","wb");fwrite(dds.data(),1,dds.size(),asset);fclose(asset);
        Microsoft::WRL::ComPtr<IDirect3DTexture8> loaded;D3DXIMAGE_INFO imageInfo{};
        check(D3DXCreateTextureFromFileExA(device.Get(),"W3DNative12Tests.dds",D3DX_DEFAULT,D3DX_DEFAULT,D3DX_DEFAULT,0,D3DFMT_UNKNOWN,D3DPOOL_MANAGED,D3DX_DEFAULT,D3DX_DEFAULT,0,&imageInfo,nullptr,&loaded),"native DDS load");
        if(loaded->GetLevelCount()!=4 || imageInfo.Format!=D3DFMT_DXT1)throw std::runtime_error("DDS mip metadata changed");
        check(device->SetTexture(0,loaded.Get()),"DDS bind");check(device->BeginScene(),"DDS scene");check(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,triangle,sizeof(V)),"DDS draw");pixel(device.Get(),320,240,0,0,255,"DDS GPU sample");
        uint8_t tga[18+16]{};tga[2]=2;tga[12]=tga[14]=2;tga[16]=32;tga[17]=0x28;
        for(UINT offset=18;offset<sizeof(tga);offset+=4){tga[offset+1]=tga[offset+2]=tga[offset+3]=255;}
        fopen_s(&asset,"W3DNative12Tests.tga","wb");fwrite(tga,1,sizeof(tga),asset);fclose(asset);loaded.Reset();
        check(D3DXCreateTextureFromFileExA(device.Get(),"W3DNative12Tests.tga",D3DX_DEFAULT,D3DX_DEFAULT,D3DX_DEFAULT,0,D3DFMT_UNKNOWN,D3DPOOL_MANAGED,D3DX_DEFAULT,D3DX_DEFAULT,0,&imageInfo,nullptr,&loaded),"native TGA load");
        if(loaded->GetLevelCount()!=2)throw std::runtime_error("TGA mip generation failed");
        check(device->SetTexture(0,loaded.Get()),"TGA bind");check(device->SetTextureStageState(0,D3DTSS_MAXMIPLEVEL,1),"sample generated mip");
        check(device->BeginScene(),"TGA scene");check(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,triangle,sizeof(V)),"TGA draw");pixel(device.Get(),320,240,255,255,0,"TGA generated mip GPU sample");
        loaded.Reset();fopen_s(&asset,"W3DNative12Tests.dds","wb");fwrite(dds.data(),1,130,asset);fclose(asset);
        if(SUCCEEDED(D3DXCreateTextureFromFileExA(device.Get(),"W3DNative12Tests.dds",D3DX_DEFAULT,D3DX_DEFAULT,D3DX_DEFAULT,0,D3DFMT_UNKNOWN,D3DPOOL_MANAGED,D3DX_DEFAULT,D3DX_DEFAULT,0,nullptr,nullptr,&loaded)) || loaded)throw std::runtime_error("truncated DDS accepted");
        std::puts("PASS DDS compressed mip upload, TGA decoding/generated mip, truncated DDS rejection");
        // Material sources, specular, point attenuation and spot cones use native VS lighting.
        check(device->SetVertexShader(D3DFVF_XYZ|D3DFVF_NORMAL),"lit geometry FVF");check(device->SetTexture(0,nullptr),"lit texture off");
        check(device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE),"lit diffuse argument");
        check(device->SetRenderState(D3DRS_LIGHTING,TRUE),"lighting enabled");check(device->SetRenderState(D3DRS_COLORVERTEX,FALSE),"material color sources");
        check(device->SetRenderState(D3DRS_LOCALVIEWER,FALSE),"infinite viewer");check(device->SetRenderState(D3DRS_SPECULARENABLE,TRUE),"specular enabled");
        D3DMATERIAL8 material{};material.Diffuse={0,.4f,0,1};material.Ambient={1,0,0,1};material.Emissive={0,0,.2f,1};material.Specular={.25f,.25f,.25f,1};material.Power=4;
        check(device->SetMaterial(&material),"lit material");D3DLIGHT8 light{};light.Type=D3DLIGHT_DIRECTIONAL;light.Direction={0,0,-1};light.Diffuse={1,1,1,1};light.Specular={1,1,1,1};light.Ambient={.1f,.1f,.1f,1};
        check(device->SetLight(0,&light),"directional light");check(device->LightEnable(0,TRUE),"light enabled");
        struct LitVertex{float x,y,z,nx,ny,nz;};const LitVertex litTriangle[]={{-.8f,-.8f,.5f,0,0,1},{0,.8f,.5f,0,0,1},{.8f,-.8f,.5f,0,0,1}};
        check(device->BeginScene(),"lit scene");check(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,litTriangle,sizeof(LitVertex)),"lit draw");pixel(device.Get(),320,240,89,166,115,"diffuse/ambient/emissive/specular material");
        check(device->SetRenderState(D3DRS_SPECULARENABLE,FALSE),"disable specular");light.Type=D3DLIGHT_POINT;light.Position={0,0,1000};light.Range=2000;light.Attenuation0=2;
        check(device->SetLight(0,&light),"point light");check(device->BeginScene(),"point scene");check(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,litTriangle,sizeof(LitVertex)),"point draw");pixel(device.Get(),320,240,13,51,51,"point attenuation");
        light.Range=1;check(device->SetLight(0,&light),"point range");check(device->BeginScene(),"point range scene");check(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,litTriangle,sizeof(LitVertex)),"point range draw");pixel(device.Get(),320,240,0,0,51,"point range cutoff");
        light.Type=D3DLIGHT_SPOT;light.Range=2000;light.Theta=.5f;light.Phi=1;light.Falloff=1;
        check(device->SetLight(0,&light),"spot light");check(device->BeginScene(),"spot scene");check(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,litTriangle,sizeof(LitVertex)),"spot draw");pixel(device.Get(),320,240,13,51,51,"spot inner cone");
        light.Direction={1,0,0};check(device->SetLight(0,&light),"spot away");check(device->BeginScene(),"spot outside scene");check(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,litTriangle,sizeof(LitVertex)),"spot outside draw");pixel(device.Get(),320,240,0,0,51,"spot outer cone");
        std::puts("PASS material lighting/specular, point attenuation/range, spotlight cone");
        if(GetModuleHandleW(L"d3d8.dll") || GetModuleHandleW(L"d3d9.dll"))throw std::runtime_error("legacy graphics runtime loaded");
    }catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());result=1;}
    SetEnvironmentVariableA("GENERALS_CAPTURE_FRAME",nullptr);SetEnvironmentVariableA("GENERALS_CAPTURE_EVERY_FRAME",nullptr);
    FreeLibrary(module);DestroyWindow(window);UnregisterClassW(wc.lpszClassName,instance);return result;
}
