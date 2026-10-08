// Generated declarations for the pinned SDK interfaces. Implementations live in W3DNative8.cpp.
#pragma once
struct Base8Methods : public IDirect3D8 {
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj) override { unsupported("IDirect3D8::QueryInterface"); return E_NOTIMPL; }
    ULONG STDMETHODCALLTYPE AddRef() override { unsupported("IDirect3D8::AddRef"); return {}; }
    ULONG STDMETHODCALLTYPE Release() override { unsupported("IDirect3D8::Release"); return {}; }
    HRESULT STDMETHODCALLTYPE RegisterSoftwareDevice(void* pInitializeFunction) override { unsupported("IDirect3D8::RegisterSoftwareDevice"); return E_NOTIMPL; }
    UINT STDMETHODCALLTYPE GetAdapterCount() override { unsupported("IDirect3D8::GetAdapterCount"); return {}; }
    HRESULT STDMETHODCALLTYPE GetAdapterIdentifier(UINT Adapter,DWORD Flags,D3DADAPTER_IDENTIFIER8* pIdentifier) override { unsupported("IDirect3D8::GetAdapterIdentifier"); return E_NOTIMPL; }
    UINT STDMETHODCALLTYPE GetAdapterModeCount(UINT Adapter) override { unsupported("IDirect3D8::GetAdapterModeCount"); return {}; }
    HRESULT STDMETHODCALLTYPE EnumAdapterModes(UINT Adapter,UINT Mode,D3DDISPLAYMODE* pMode) override { unsupported("IDirect3D8::EnumAdapterModes"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetAdapterDisplayMode(UINT Adapter,D3DDISPLAYMODE* pMode) override { unsupported("IDirect3D8::GetAdapterDisplayMode"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CheckDeviceType(UINT Adapter,D3DDEVTYPE CheckType,D3DFORMAT DisplayFormat,D3DFORMAT BackBufferFormat,BOOL Windowed) override { unsupported("IDirect3D8::CheckDeviceType"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CheckDeviceFormat(UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT AdapterFormat,DWORD Usage,D3DRESOURCETYPE RType,D3DFORMAT CheckFormat) override { unsupported("IDirect3D8::CheckDeviceFormat"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CheckDeviceMultiSampleType(UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT SurfaceFormat,BOOL Windowed,D3DMULTISAMPLE_TYPE MultiSampleType) override { unsupported("IDirect3D8::CheckDeviceMultiSampleType"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CheckDepthStencilMatch(UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT AdapterFormat,D3DFORMAT RenderTargetFormat,D3DFORMAT DepthStencilFormat) override { unsupported("IDirect3D8::CheckDepthStencilMatch"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetDeviceCaps(UINT Adapter,D3DDEVTYPE DeviceType,D3DCAPS8* pCaps) override { unsupported("IDirect3D8::GetDeviceCaps"); return E_NOTIMPL; }
    HMONITOR STDMETHODCALLTYPE GetAdapterMonitor(UINT Adapter) override { unsupported("IDirect3D8::GetAdapterMonitor"); return {}; }
    HRESULT STDMETHODCALLTYPE CreateDevice(UINT Adapter,D3DDEVTYPE DeviceType,HWND hFocusWindow,DWORD BehaviorFlags,D3DPRESENT_PARAMETERS* pPresentationParameters,IDirect3DDevice8** ppReturnedDeviceInterface) override { unsupported("IDirect3D8::CreateDevice"); return E_NOTIMPL; }
};
struct Device8Methods : public IDirect3DDevice8 {
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj) override { unsupported("IDirect3DDevice8::QueryInterface"); return E_NOTIMPL; }
    ULONG STDMETHODCALLTYPE AddRef() override { unsupported("IDirect3DDevice8::AddRef"); return {}; }
    ULONG STDMETHODCALLTYPE Release() override { unsupported("IDirect3DDevice8::Release"); return {}; }
    HRESULT STDMETHODCALLTYPE TestCooperativeLevel() override { unsupported("IDirect3DDevice8::TestCooperativeLevel"); return E_NOTIMPL; }
    UINT STDMETHODCALLTYPE GetAvailableTextureMem() override { unsupported("IDirect3DDevice8::GetAvailableTextureMem"); return {}; }
    HRESULT STDMETHODCALLTYPE ResourceManagerDiscardBytes(DWORD Bytes) override { unsupported("IDirect3DDevice8::ResourceManagerDiscardBytes"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetDirect3D(IDirect3D8** ppD3D8) override { unsupported("IDirect3DDevice8::GetDirect3D"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetDeviceCaps(D3DCAPS8* pCaps) override { unsupported("IDirect3DDevice8::GetDeviceCaps"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetDisplayMode(D3DDISPLAYMODE* pMode) override { unsupported("IDirect3DDevice8::GetDisplayMode"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters) override { unsupported("IDirect3DDevice8::GetCreationParameters"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetCursorProperties(UINT XHotSpot,UINT YHotSpot,IDirect3DSurface8* pCursorBitmap) override { unsupported("IDirect3DDevice8::SetCursorProperties"); return E_NOTIMPL; }
    void STDMETHODCALLTYPE SetCursorPosition(UINT XScreenSpace,UINT YScreenSpace,DWORD Flags) override { unsupported("IDirect3DDevice8::SetCursorPosition");  }
    BOOL STDMETHODCALLTYPE ShowCursor(BOOL bShow) override { unsupported("IDirect3DDevice8::ShowCursor"); return {}; }
    HRESULT STDMETHODCALLTYPE CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters,IDirect3DSwapChain8** pSwapChain) override { unsupported("IDirect3DDevice8::CreateAdditionalSwapChain"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Reset(D3DPRESENT_PARAMETERS* pPresentationParameters) override { unsupported("IDirect3DDevice8::Reset"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Present(CONST RECT* pSourceRect,CONST RECT* pDestRect,HWND hDestWindowOverride,CONST RGNDATA* pDirtyRegion) override { unsupported("IDirect3DDevice8::Present"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetBackBuffer(UINT BackBuffer,D3DBACKBUFFER_TYPE Type,IDirect3DSurface8** ppBackBuffer) override { unsupported("IDirect3DDevice8::GetBackBuffer"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetRasterStatus(D3DRASTER_STATUS* pRasterStatus) override { unsupported("IDirect3DDevice8::GetRasterStatus"); return E_NOTIMPL; }
    void STDMETHODCALLTYPE SetGammaRamp(DWORD Flags,CONST D3DGAMMARAMP* pRamp) override { unsupported("IDirect3DDevice8::SetGammaRamp");  }
    void STDMETHODCALLTYPE GetGammaRamp(D3DGAMMARAMP* pRamp) override { unsupported("IDirect3DDevice8::GetGammaRamp");  }
    HRESULT STDMETHODCALLTYPE CreateTexture(UINT Width,UINT Height,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DTexture8** ppTexture) override { unsupported("IDirect3DDevice8::CreateTexture"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateVolumeTexture(UINT Width,UINT Height,UINT Depth,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DVolumeTexture8** ppVolumeTexture) override { unsupported("IDirect3DDevice8::CreateVolumeTexture"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateCubeTexture(UINT EdgeLength,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DCubeTexture8** ppCubeTexture) override { unsupported("IDirect3DDevice8::CreateCubeTexture"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateVertexBuffer(UINT Length,DWORD Usage,DWORD FVF,D3DPOOL Pool,IDirect3DVertexBuffer8** ppVertexBuffer) override { unsupported("IDirect3DDevice8::CreateVertexBuffer"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateIndexBuffer(UINT Length,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DIndexBuffer8** ppIndexBuffer) override { unsupported("IDirect3DDevice8::CreateIndexBuffer"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateRenderTarget(UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,BOOL Lockable,IDirect3DSurface8** ppSurface) override { unsupported("IDirect3DDevice8::CreateRenderTarget"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateDepthStencilSurface(UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,IDirect3DSurface8** ppSurface) override { unsupported("IDirect3DDevice8::CreateDepthStencilSurface"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateImageSurface(UINT Width,UINT Height,D3DFORMAT Format,IDirect3DSurface8** ppSurface) override { unsupported("IDirect3DDevice8::CreateImageSurface"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CopyRects(IDirect3DSurface8* pSourceSurface,CONST RECT* pSourceRectsArray,UINT cRects,IDirect3DSurface8* pDestinationSurface,CONST POINT* pDestPointsArray) override { unsupported("IDirect3DDevice8::CopyRects"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE UpdateTexture(IDirect3DBaseTexture8* pSourceTexture,IDirect3DBaseTexture8* pDestinationTexture) override { unsupported("IDirect3DDevice8::UpdateTexture"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetFrontBuffer(IDirect3DSurface8* pDestSurface) override { unsupported("IDirect3DDevice8::GetFrontBuffer"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetRenderTarget(IDirect3DSurface8* pRenderTarget,IDirect3DSurface8* pNewZStencil) override { unsupported("IDirect3DDevice8::SetRenderTarget"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetRenderTarget(IDirect3DSurface8** ppRenderTarget) override { unsupported("IDirect3DDevice8::GetRenderTarget"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetDepthStencilSurface(IDirect3DSurface8** ppZStencilSurface) override { unsupported("IDirect3DDevice8::GetDepthStencilSurface"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE BeginScene() override { unsupported("IDirect3DDevice8::BeginScene"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE EndScene() override { unsupported("IDirect3DDevice8::EndScene"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Clear(DWORD Count,CONST D3DRECT* pRects,DWORD Flags,D3DCOLOR Color,float Z,DWORD Stencil) override { unsupported("IDirect3DDevice8::Clear"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetTransform(D3DTRANSFORMSTATETYPE State,CONST D3DMATRIX* pMatrix) override { unsupported("IDirect3DDevice8::SetTransform"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetTransform(D3DTRANSFORMSTATETYPE State,D3DMATRIX* pMatrix) override { unsupported("IDirect3DDevice8::GetTransform"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE MultiplyTransform(D3DTRANSFORMSTATETYPE,CONST D3DMATRIX*) override { unsupported("IDirect3DDevice8::MultiplyTransform"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetViewport(CONST D3DVIEWPORT8* pViewport) override { unsupported("IDirect3DDevice8::SetViewport"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetViewport(D3DVIEWPORT8* pViewport) override { unsupported("IDirect3DDevice8::GetViewport"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetMaterial(CONST D3DMATERIAL8* pMaterial) override { unsupported("IDirect3DDevice8::SetMaterial"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetMaterial(D3DMATERIAL8* pMaterial) override { unsupported("IDirect3DDevice8::GetMaterial"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetLight(DWORD Index,CONST D3DLIGHT8*) override { unsupported("IDirect3DDevice8::SetLight"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetLight(DWORD Index,D3DLIGHT8*) override { unsupported("IDirect3DDevice8::GetLight"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE LightEnable(DWORD Index,BOOL Enable) override { unsupported("IDirect3DDevice8::LightEnable"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetLightEnable(DWORD Index,BOOL* pEnable) override { unsupported("IDirect3DDevice8::GetLightEnable"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetClipPlane(DWORD Index,CONST float* pPlane) override { unsupported("IDirect3DDevice8::SetClipPlane"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetClipPlane(DWORD Index,float* pPlane) override { unsupported("IDirect3DDevice8::GetClipPlane"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE State,DWORD Value) override { unsupported("IDirect3DDevice8::SetRenderState"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetRenderState(D3DRENDERSTATETYPE State,DWORD* pValue) override { unsupported("IDirect3DDevice8::GetRenderState"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE BeginStateBlock() override { unsupported("IDirect3DDevice8::BeginStateBlock"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE EndStateBlock(DWORD* pToken) override { unsupported("IDirect3DDevice8::EndStateBlock"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE ApplyStateBlock(DWORD Token) override { unsupported("IDirect3DDevice8::ApplyStateBlock"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CaptureStateBlock(DWORD Token) override { unsupported("IDirect3DDevice8::CaptureStateBlock"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE DeleteStateBlock(DWORD Token) override { unsupported("IDirect3DDevice8::DeleteStateBlock"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateStateBlock(D3DSTATEBLOCKTYPE Type,DWORD* pToken) override { unsupported("IDirect3DDevice8::CreateStateBlock"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetClipStatus(CONST D3DCLIPSTATUS8* pClipStatus) override { unsupported("IDirect3DDevice8::SetClipStatus"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetClipStatus(D3DCLIPSTATUS8* pClipStatus) override { unsupported("IDirect3DDevice8::GetClipStatus"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetTexture(DWORD Stage,IDirect3DBaseTexture8** ppTexture) override { unsupported("IDirect3DDevice8::GetTexture"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetTexture(DWORD Stage,IDirect3DBaseTexture8* pTexture) override { unsupported("IDirect3DDevice8::SetTexture"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetTextureStageState(DWORD Stage,D3DTEXTURESTAGESTATETYPE Type,DWORD* pValue) override { unsupported("IDirect3DDevice8::GetTextureStageState"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetTextureStageState(DWORD Stage,D3DTEXTURESTAGESTATETYPE Type,DWORD Value) override { unsupported("IDirect3DDevice8::SetTextureStageState"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE ValidateDevice(DWORD* pNumPasses) override { unsupported("IDirect3DDevice8::ValidateDevice"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetInfo(DWORD DevInfoID,void* pDevInfoStruct,DWORD DevInfoStructSize) override { unsupported("IDirect3DDevice8::GetInfo"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPaletteEntries(UINT PaletteNumber,CONST PALETTEENTRY* pEntries) override { unsupported("IDirect3DDevice8::SetPaletteEntries"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetPaletteEntries(UINT PaletteNumber,PALETTEENTRY* pEntries) override { unsupported("IDirect3DDevice8::GetPaletteEntries"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetCurrentTexturePalette(UINT PaletteNumber) override { unsupported("IDirect3DDevice8::SetCurrentTexturePalette"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetCurrentTexturePalette(UINT *PaletteNumber) override { unsupported("IDirect3DDevice8::GetCurrentTexturePalette"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType,UINT StartVertex,UINT PrimitiveCount) override { unsupported("IDirect3DDevice8::DrawPrimitive"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitive(D3DPRIMITIVETYPE,UINT minIndex,UINT NumVertices,UINT startIndex,UINT primCount) override { unsupported("IDirect3DDevice8::DrawIndexedPrimitive"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,UINT PrimitiveCount,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride) override { unsupported("IDirect3DDevice8::DrawPrimitiveUP"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,UINT MinVertexIndex,UINT NumVertexIndices,UINT PrimitiveCount,CONST void* pIndexData,D3DFORMAT IndexDataFormat,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride) override { unsupported("IDirect3DDevice8::DrawIndexedPrimitiveUP"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE ProcessVertices(UINT SrcStartIndex,UINT DestIndex,UINT VertexCount,IDirect3DVertexBuffer8* pDestBuffer,DWORD Flags) override { unsupported("IDirect3DDevice8::ProcessVertices"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateVertexShader(CONST DWORD* pDeclaration,CONST DWORD* pFunction,DWORD* pHandle,DWORD Usage) override { unsupported("IDirect3DDevice8::CreateVertexShader"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetVertexShader(DWORD Handle) override { unsupported("IDirect3DDevice8::SetVertexShader"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetVertexShader(DWORD* pHandle) override { unsupported("IDirect3DDevice8::GetVertexShader"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE DeleteVertexShader(DWORD Handle) override { unsupported("IDirect3DDevice8::DeleteVertexShader"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetVertexShaderConstant(DWORD Register,CONST void* pConstantData,DWORD ConstantCount) override { unsupported("IDirect3DDevice8::SetVertexShaderConstant"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetVertexShaderConstant(DWORD Register,void* pConstantData,DWORD ConstantCount) override { unsupported("IDirect3DDevice8::GetVertexShaderConstant"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetVertexShaderDeclaration(DWORD Handle,void* pData,DWORD* pSizeOfData) override { unsupported("IDirect3DDevice8::GetVertexShaderDeclaration"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetVertexShaderFunction(DWORD Handle,void* pData,DWORD* pSizeOfData) override { unsupported("IDirect3DDevice8::GetVertexShaderFunction"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetStreamSource(UINT StreamNumber,IDirect3DVertexBuffer8* pStreamData,UINT Stride) override { unsupported("IDirect3DDevice8::SetStreamSource"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetStreamSource(UINT StreamNumber,IDirect3DVertexBuffer8** ppStreamData,UINT* pStride) override { unsupported("IDirect3DDevice8::GetStreamSource"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetIndices(IDirect3DIndexBuffer8* pIndexData,UINT BaseVertexIndex) override { unsupported("IDirect3DDevice8::SetIndices"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetIndices(IDirect3DIndexBuffer8** ppIndexData,UINT* pBaseVertexIndex) override { unsupported("IDirect3DDevice8::GetIndices"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreatePixelShader(CONST DWORD* pFunction,DWORD* pHandle) override { unsupported("IDirect3DDevice8::CreatePixelShader"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPixelShader(DWORD Handle) override { unsupported("IDirect3DDevice8::SetPixelShader"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetPixelShader(DWORD* pHandle) override { unsupported("IDirect3DDevice8::GetPixelShader"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE DeletePixelShader(DWORD Handle) override { unsupported("IDirect3DDevice8::DeletePixelShader"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPixelShaderConstant(DWORD Register,CONST void* pConstantData,DWORD ConstantCount) override { unsupported("IDirect3DDevice8::SetPixelShaderConstant"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetPixelShaderConstant(DWORD Register,void* pConstantData,DWORD ConstantCount) override { unsupported("IDirect3DDevice8::GetPixelShaderConstant"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetPixelShaderFunction(DWORD Handle,void* pData,DWORD* pSizeOfData) override { unsupported("IDirect3DDevice8::GetPixelShaderFunction"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE DrawRectPatch(UINT Handle,CONST float* pNumSegs,CONST D3DRECTPATCH_INFO* pRectPatchInfo) override { unsupported("IDirect3DDevice8::DrawRectPatch"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE DrawTriPatch(UINT Handle,CONST float* pNumSegs,CONST D3DTRIPATCH_INFO* pTriPatchInfo) override { unsupported("IDirect3DDevice8::DrawTriPatch"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE DeletePatch(UINT Handle) override { unsupported("IDirect3DDevice8::DeletePatch"); return E_NOTIMPL; }
};
struct Surface8Methods : public IDirect3DSurface8 {
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj) override { unsupported("IDirect3DSurface8::QueryInterface"); return E_NOTIMPL; }
    ULONG STDMETHODCALLTYPE AddRef() override { unsupported("IDirect3DSurface8::AddRef"); return {}; }
    ULONG STDMETHODCALLTYPE Release() override { unsupported("IDirect3DSurface8::Release"); return {}; }
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice8** ppDevice) override { unsupported("IDirect3DSurface8::GetDevice"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid,CONST void* pData,DWORD SizeOfData,DWORD Flags) override { unsupported("IDirect3DSurface8::SetPrivateData"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid,void* pData,DWORD* pSizeOfData) override { unsupported("IDirect3DSurface8::GetPrivateData"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid) override { unsupported("IDirect3DSurface8::FreePrivateData"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetContainer(REFIID riid,void** ppContainer) override { unsupported("IDirect3DSurface8::GetContainer"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetDesc(D3DSURFACE_DESC *pDesc) override { unsupported("IDirect3DSurface8::GetDesc"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE LockRect(D3DLOCKED_RECT* pLockedRect,CONST RECT* pRect,DWORD Flags) override { unsupported("IDirect3DSurface8::LockRect"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE UnlockRect() override { unsupported("IDirect3DSurface8::UnlockRect"); return E_NOTIMPL; }
};
struct Texture8Methods : public IDirect3DTexture8 {
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj) override { unsupported("IDirect3DTexture8::QueryInterface"); return E_NOTIMPL; }
    ULONG STDMETHODCALLTYPE AddRef() override { unsupported("IDirect3DTexture8::AddRef"); return {}; }
    ULONG STDMETHODCALLTYPE Release() override { unsupported("IDirect3DTexture8::Release"); return {}; }
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice8** ppDevice) override { unsupported("IDirect3DTexture8::GetDevice"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid,CONST void* pData,DWORD SizeOfData,DWORD Flags) override { unsupported("IDirect3DTexture8::SetPrivateData"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid,void* pData,DWORD* pSizeOfData) override { unsupported("IDirect3DTexture8::GetPrivateData"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid) override { unsupported("IDirect3DTexture8::FreePrivateData"); return E_NOTIMPL; }
    DWORD STDMETHODCALLTYPE SetPriority(DWORD PriorityNew) override { unsupported("IDirect3DTexture8::SetPriority"); return {}; }
    DWORD STDMETHODCALLTYPE GetPriority() override { unsupported("IDirect3DTexture8::GetPriority"); return {}; }
    void STDMETHODCALLTYPE PreLoad() override { unsupported("IDirect3DTexture8::PreLoad");  }
    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override { unsupported("IDirect3DTexture8::GetType"); return {}; }
    DWORD STDMETHODCALLTYPE SetLOD(DWORD LODNew) override { unsupported("IDirect3DTexture8::SetLOD"); return {}; }
    DWORD STDMETHODCALLTYPE GetLOD() override { unsupported("IDirect3DTexture8::GetLOD"); return {}; }
    DWORD STDMETHODCALLTYPE GetLevelCount() override { unsupported("IDirect3DTexture8::GetLevelCount"); return {}; }
    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level,D3DSURFACE_DESC *pDesc) override { unsupported("IDirect3DTexture8::GetLevelDesc"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetSurfaceLevel(UINT Level,IDirect3DSurface8** ppSurfaceLevel) override { unsupported("IDirect3DTexture8::GetSurfaceLevel"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE LockRect(UINT Level,D3DLOCKED_RECT* pLockedRect,CONST RECT* pRect,DWORD Flags) override { unsupported("IDirect3DTexture8::LockRect"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE UnlockRect(UINT Level) override { unsupported("IDirect3DTexture8::UnlockRect"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE AddDirtyRect(CONST RECT* pDirtyRect) override { unsupported("IDirect3DTexture8::AddDirtyRect"); return E_NOTIMPL; }
};
struct VertexBuffer8Methods : public IDirect3DVertexBuffer8 {
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj) override { unsupported("IDirect3DVertexBuffer8::QueryInterface"); return E_NOTIMPL; }
    ULONG STDMETHODCALLTYPE AddRef() override { unsupported("IDirect3DVertexBuffer8::AddRef"); return {}; }
    ULONG STDMETHODCALLTYPE Release() override { unsupported("IDirect3DVertexBuffer8::Release"); return {}; }
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice8** ppDevice) override { unsupported("IDirect3DVertexBuffer8::GetDevice"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid,CONST void* pData,DWORD SizeOfData,DWORD Flags) override { unsupported("IDirect3DVertexBuffer8::SetPrivateData"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid,void* pData,DWORD* pSizeOfData) override { unsupported("IDirect3DVertexBuffer8::GetPrivateData"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid) override { unsupported("IDirect3DVertexBuffer8::FreePrivateData"); return E_NOTIMPL; }
    DWORD STDMETHODCALLTYPE SetPriority(DWORD PriorityNew) override { unsupported("IDirect3DVertexBuffer8::SetPriority"); return {}; }
    DWORD STDMETHODCALLTYPE GetPriority() override { unsupported("IDirect3DVertexBuffer8::GetPriority"); return {}; }
    void STDMETHODCALLTYPE PreLoad() override { unsupported("IDirect3DVertexBuffer8::PreLoad");  }
    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override { unsupported("IDirect3DVertexBuffer8::GetType"); return {}; }
    HRESULT STDMETHODCALLTYPE Lock(UINT OffsetToLock,UINT SizeToLock,BYTE** ppbData,DWORD Flags) override { unsupported("IDirect3DVertexBuffer8::Lock"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Unlock() override { unsupported("IDirect3DVertexBuffer8::Unlock"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetDesc(D3DVERTEXBUFFER_DESC *pDesc) override { unsupported("IDirect3DVertexBuffer8::GetDesc"); return E_NOTIMPL; }
};
struct IndexBuffer8Methods : public IDirect3DIndexBuffer8 {
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj) override { unsupported("IDirect3DIndexBuffer8::QueryInterface"); return E_NOTIMPL; }
    ULONG STDMETHODCALLTYPE AddRef() override { unsupported("IDirect3DIndexBuffer8::AddRef"); return {}; }
    ULONG STDMETHODCALLTYPE Release() override { unsupported("IDirect3DIndexBuffer8::Release"); return {}; }
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice8** ppDevice) override { unsupported("IDirect3DIndexBuffer8::GetDevice"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid,CONST void* pData,DWORD SizeOfData,DWORD Flags) override { unsupported("IDirect3DIndexBuffer8::SetPrivateData"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid,void* pData,DWORD* pSizeOfData) override { unsupported("IDirect3DIndexBuffer8::GetPrivateData"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid) override { unsupported("IDirect3DIndexBuffer8::FreePrivateData"); return E_NOTIMPL; }
    DWORD STDMETHODCALLTYPE SetPriority(DWORD PriorityNew) override { unsupported("IDirect3DIndexBuffer8::SetPriority"); return {}; }
    DWORD STDMETHODCALLTYPE GetPriority() override { unsupported("IDirect3DIndexBuffer8::GetPriority"); return {}; }
    void STDMETHODCALLTYPE PreLoad() override { unsupported("IDirect3DIndexBuffer8::PreLoad");  }
    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override { unsupported("IDirect3DIndexBuffer8::GetType"); return {}; }
    HRESULT STDMETHODCALLTYPE Lock(UINT OffsetToLock,UINT SizeToLock,BYTE** ppbData,DWORD Flags) override { unsupported("IDirect3DIndexBuffer8::Lock"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Unlock() override { unsupported("IDirect3DIndexBuffer8::Unlock"); return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetDesc(D3DINDEXBUFFER_DESC *pDesc) override { unsupported("IDirect3DIndexBuffer8::GetDesc"); return E_NOTIMPL; }
};
