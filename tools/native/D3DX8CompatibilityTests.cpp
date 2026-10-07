#include <d3dx8.h>
#include <wrl/client.h>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#pragma warning(push)
#pragma warning(disable: 4244 4267) // Legacy helpers; retain strict warnings in this test.
#include <Precompiled/CppMacros.h>
#include "ddsfile.h"
#pragma warning(pop)
using Microsoft::WRL::ComPtr;
static void check(HRESULT value, const char* operation) {
    if (FAILED(value)) { fprintf(stderr, "%s: %08lX\n", operation, static_cast<unsigned long>(value)); throw std::runtime_error(operation); }
}
int wmain(int argc, wchar_t** argv)
{
    try {
        if (argc < 2) return 2;
        D3DXMATRIX scaling, translation, combined, inverse, identity;
        if (!D3DXMatrixScaling(&scaling, 2, 3, 4) || !D3DXMatrixTranslation(&translation, 5, 6, 7) ||
            !D3DXMatrixMultiply(&combined, &scaling, &translation)) throw std::runtime_error("matrix runtime");
        float determinant = 0;
        if (!D3DXMatrixInverse(&inverse, &determinant, &combined) || !D3DXMatrixMultiply(&identity, &combined, &inverse)) throw std::runtime_error("inverse runtime");
        for (int row = 0; row < 4; ++row) for (int column = 0; column < 4; ++column)
            if (fabsf(identity.m[row][column] - (row == column ? 1.f : 0.f)) > 0.0001f) throw std::runtime_error("matrix inverse mismatch");
        if (fabsf(determinant - 24.f) > .001f) throw std::runtime_error("determinant mismatch");
        D3DXVECTOR3 position(1, 2, 3); D3DXVECTOR4 transformed;
        if (!D3DXVec3Transform(&transformed, &position, &combined) || transformed.x != 7 || transformed.y != 12 || transformed.z != 19) throw std::runtime_error("vector transform mismatch");
        puts("PASS: D3DX8 x64 matrix multiplication, inverse, determinant and vertex transforms");
        const char source[] = "ps.1.1\nmov r0, v0\n";
        ComPtr<ID3DXBuffer> shader;
        check(D3DXAssembleShader(source, sizeof(source) - 1, 0, nullptr, &shader, nullptr), "shader assembler");
        if (*static_cast<DWORD*>(shader->GetBufferPointer()) != D3DPS_VERSION(1,1)) throw std::runtime_error("shader version mismatch");
        HMODULE bridge = LoadLibraryExW(argv[1], nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!bridge) throw std::runtime_error("bridge load");
        using Create = IDirect3D8* (WINAPI*)(UINT);
        auto create = reinterpret_cast<Create>(GetProcAddress(bridge, "Direct3DCreate8"));
        if (!create) throw std::runtime_error("bridge entry point");
        ComPtr<IDirect3D8> renderer; renderer.Attach(create(D3D_SDK_VERSION));
        if (!renderer) throw std::runtime_error("renderer create");
        WNDCLASSW windowClass = {}; windowClass.lpfnWndProc = DefWindowProcW;
        windowClass.hInstance = GetModuleHandleW(nullptr); windowClass.lpszClassName = L"D3DX8CompatTest";
        if (!RegisterClassW(&windowClass)) throw std::runtime_error("window class");
        HWND window = CreateWindowW(windowClass.lpszClassName, L"", WS_POPUP, 0,0,64,64,nullptr,nullptr,windowClass.hInstance,nullptr);
        if (!window) throw std::runtime_error("test window");
        D3DPRESENT_PARAMETERS options = {}; options.BackBufferWidth = 64; options.BackBufferHeight = 64;
        options.BackBufferFormat = D3DFMT_A8R8G8B8; options.BackBufferCount = 1;
        options.SwapEffect = D3DSWAPEFFECT_DISCARD; options.Windowed = TRUE; options.hDeviceWindow = window;
        ComPtr<IDirect3DDevice8> device;
        check(renderer->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&options,&device), "device create");
        ComPtr<IDirect3DTexture8> texture;
        check(D3DXCreateTexture(device.Get(),64,64,0,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture), "D3DX texture create");
        D3DLOCKED_RECT pixels = {}; check(texture->LockRect(0,&pixels,nullptr,0), "texture lock");
        for (int y = 0; y < 64; ++y) for (int x = 0; x < 64; ++x)
            reinterpret_cast<DWORD*>(static_cast<char*>(pixels.pBits) + y * pixels.Pitch)[x] = 0xFF336699;
        check(texture->UnlockRect(0), "texture unlock");
        check(D3DXFilterTexture(texture.Get(),nullptr,0,D3DX_FILTER_BOX), "mip filter");
        if (texture->GetLevelCount() != 7) throw std::runtime_error("mip count mismatch");
        check(texture->LockRect(6,&pixels,nullptr,D3DLOCK_READONLY), "last mip lock");
        const DWORD lastPixel = *static_cast<DWORD*>(pixels.pBits);
        check(texture->UnlockRect(6), "last mip unlock");
        if (lastPixel != 0xFF336699) throw std::runtime_error("mip pixel mismatch");
        check(device->SetTexture(0, texture.Get()), "bind wrapped texture");
        check(device->SetTexture(0, nullptr), "unbind wrapped texture");
        for (int sample = 2; sample < argc; ++sample) {
            FILE* input = nullptr;
            if (_wfopen_s(&input, argv[sample], L"rb") || !input) throw std::runtime_error("DDS sample open");
            char magic[4] = {}; LegacyDDSURFACEDESC2 header{};
            const bool read = fread(magic,1,4,input) == 4 && fread(&header,1,sizeof(header),input) == sizeof(header);
            fclose(input);
            if (!read || memcmp(magic,"DDS ",4) || header.Size != sizeof(header) || !header.Width || !header.Height)
                throw std::runtime_error("game DDS header does not match the on-disk layout");
            char filename[MAX_PATH] = {};
            if (!WideCharToMultiByte(CP_ACP,WC_NO_BEST_FIT_CHARS,argv[sample],-1,filename,MAX_PATH,nullptr,nullptr))
                throw std::runtime_error("DDS sample path");
            ComPtr<IDirect3DTexture8> loaded;
            D3DXIMAGE_INFO info{};
            check(D3DXCreateTextureFromFileExA(device.Get(),filename,D3DX_DEFAULT,D3DX_DEFAULT,1,0,D3DFMT_A8R8G8B8,
                D3DPOOL_MANAGED,D3DX_FILTER_NONE,D3DX_FILTER_NONE,0,&info,nullptr,&loaded), "retail DDS GPU load");
            if (info.Width != header.Width || info.Height != header.Height) throw std::runtime_error("DDS dimensions mismatch");
            D3DLOCKED_RECT texels{};
            check(loaded->LockRect(0,&texels,nullptr,D3DLOCK_READONLY),"retail DDS texel readback");
            bool nonPlaceholder = false;
            for (UINT y = 0; y < info.Height; ++y) {
                const DWORD* row = reinterpret_cast<const DWORD*>(static_cast<const char*>(texels.pBits) + y * texels.Pitch);
                for (UINT x = 0; x < info.Width; ++x) if ((row[x] & 0xFFFFFF) != 0xFF00FF) nonPlaceholder = true;
            }
            check(loaded->UnlockRect(0), "retail DDS unlock");
            if (!nonPlaceholder) throw std::runtime_error("DDS decoded entirely as the magenta placeholder");
            std::printf("PASS: retail DDS header 124 bytes, %ux%u, format 0x%08X and GPU texel readback\n",header.Width,header.Height,header.PixelFormat.FourCC);
        }
        DWORD pixelHandle = 0;
        check(device->CreatePixelShader(static_cast<const DWORD*>(shader->GetBufferPointer()), &pixelHandle), "create assembled shader");
        check(device->DeletePixelShader(pixelHandle), "delete assembled shader");
        texture.Reset(); device.Reset(); renderer.Reset(); shader.Reset();
        DestroyWindow(window); UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance); FreeLibrary(bridge);
        puts("PASS: D3DX8 x64 shader assembly, wrapped GPU texture and filtered mip pixel readback");
        return 0;
    } catch (const std::exception& error) { fprintf(stderr,"FAIL: %s\n",error.what()); return 1; }
}
