[CmdletBinding()]
param([Parameter(Mandatory)][string]$SourcePath)
$ErrorActionPreference='Stop'
$repositoryPath=Split-Path $PSScriptRoot -Parent
$expectedPath=Join-Path $repositoryPath '.build\d3d8to9-reference-x64\d3d8to9-255338f698c8270b537f0a91a13f795f4f988250'
if([IO.Path]::GetFullPath($SourcePath).TrimEnd('\') -ne [IO.Path]::GetFullPath($expectedPath).TrimEnd('\')) {throw 'Only the pinned x64 bridge may be patched.'}
foreach($file in @('D3D9On12Resources.h','NativePresentation12.h','DlaaPass.h','NeuralResolve.h','StreamlineRuntime.h','MotionCapture9.h','NeuralRenderer9.h','NeuralBridgeExports.cpp')) {
    Copy-Item -LiteralPath (Join-Path $repositoryPath ('renderer\'+$file)) -Destination (Join-Path $SourcePath ('source\'+$file)) -Force
}
$path=Join-Path $SourcePath 'source\d3d8to9_device.cpp'
$text=[IO.File]::ReadAllText($path)
if(-not $text.Contains('// generals-mods temporal renderer')) {
    $changes=@(
        @('#include "D3D9On12Bridge.h"', ('#include "D3D9On12Bridge.h"'+"`n"+'#include "NeuralRenderer9.h"'+"`n"+'using namespace generals_mods; // generals-mods temporal renderer')),
        @('DWORD ExtraRefs = VertexShaderAndDeclarationCount + PixelShaderHandles.size() + StateBlockTokens.size();',('DWORD ExtraRefs = VertexShaderAndDeclarationCount + PixelShaderHandles.size() + StateBlockTokens.size() + neuralOwnedReferences(ProxyInterface);')),
        @('// Release shaders and state blocks when only one reference is left',('releaseNeuralRenderer(ProxyInterface);'+"`n"+'            // Release shaders and state blocks when only one reference is left')),
        @('delete ProxyAddressLookupTable;', ('releaseNeuralRenderer(ProxyInterface);'+"`n"+'    delete ProxyAddressLookupTable;')),
        @(('HRESULT STDMETHODCALLTYPE Direct3DDevice8::Reset(D3DPRESENT_PARAMETERS8 *pPresentationParameters)'+"`n"+'{'),('HRESULT STDMETHODCALLTYPE Direct3DDevice8::Reset(D3DPRESENT_PARAMETERS8 *pPresentationParameters)'+"`n"+'{'+"`n"+'    neuralRenderer(ProxyInterface).resetDevice();')),
        @('return ProxyInterface->SetTransform(State, pMatrix);',('return neuralRenderer(ProxyInterface).transform(State, pMatrix);')),
        @('return ProxyInterface->SetViewport(pViewport);',('if (!pViewport) return D3DERR_INVALIDCALL;'+"`n"+'    const auto scaled = neuralRenderer(ProxyInterface).scaledViewport(*pViewport);'+"`n"+'    return ProxyInterface->SetViewport(&scaled);')),
        @('return recordBridgeDraw(ProxyInterface->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount));',('neuralRenderer(ProxyInterface).beforeDraw();'+"`n"+'    HRESULT result=ProxyInterface->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount);'+"`n"+'    if(SUCCEEDED(result)) neuralRenderer(ProxyInterface).afterPrimitive(PrimitiveType, StartVertex, PrimitiveCount);'+"`n"+'    return recordBridgeDraw(result);')),
        @('return recordBridgeDraw(ProxyInterface->DrawIndexedPrimitive(PrimitiveType, CurrentBaseVertexIndex, MinIndex, NumVertices, StartIndex, PrimitiveCount));',('neuralRenderer(ProxyInterface).beforeDraw();'+"`n"+'    HRESULT result=ProxyInterface->DrawIndexedPrimitive(PrimitiveType, CurrentBaseVertexIndex, MinIndex, NumVertices, StartIndex, PrimitiveCount);'+"`n"+'    if(SUCCEEDED(result)) neuralRenderer(ProxyInterface).afterIndexed(PrimitiveType, CurrentBaseVertexIndex, MinIndex, NumVertices, StartIndex, PrimitiveCount);'+"`n"+'    return recordBridgeDraw(result);'))
    )
    $text=$text.Replace("`r`n","`n")
    foreach($change in $changes) {
        $count=[regex]::Matches($text,[regex]::Escape($change[0])).Count
        $expected=if($change[0].StartsWith('DWORD ExtraRefs')) {2} else {1}
        if($count -ne $expected) {throw ('Unexpected neural bridge anchor: '+$change[0])}
        $text=$text.Replace($change[0],$change[1])
    }
    [IO.File]::WriteAllText($path,$text)
}
$text=[IO.File]::ReadAllText($path)
if(-not $text.Contains('neuralRenderer(ProxyInterface).drawPrimitiveUP(')) {
    $text=$text.Replace('ProxyInterface->DrawPrimitiveUP(PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride)', 'neuralRenderer(ProxyInterface).drawPrimitiveUP(PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride)')
    $text=$text.Replace('ProxyInterface->DrawIndexedPrimitiveUP(PrimitiveType, MinVertexIndex, NumVertexIndices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride)', 'neuralRenderer(ProxyInterface).drawIndexedPrimitiveUP(PrimitiveType, MinVertexIndex, NumVertexIndices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride)')
    [IO.File]::WriteAllText($path,$text)
}
# Validate the viewport after converting native game coordinates to neural target coordinates.
$text=[IO.File]::ReadAllText($path)
$viewportBody=@'
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetViewport(const D3DVIEWPORT8 *pViewport)
{
    return neuralRenderer(ProxyInterface).setViewport(pViewport);
}

'@
$text=[regex]::Replace($text,'(?s)HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetViewport\(.*?(?=HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetViewport)', $viewportBody+"`n")
$text=$text.Replace('return ProxyInterface->GetViewport(pViewport);','return neuralRenderer(ProxyInterface).getViewport(pViewport);')
[IO.File]::WriteAllText($path,$text)
$path=Join-Path $SourcePath 'source\d3d8to9_vertex_buffer.cpp'
$text=[IO.File]::ReadAllText($path)
if(-not $text.Contains('captureVertexUnlock')) {
    $text=$text.Replace('#include "d3d8to9.hpp"','#include "d3d8to9.hpp"'+"`n"+'#include "MotionCapture9.h"'+"`n"+'using namespace generals_mods;')
    $text=$text.Replace('return ProxyInterface->Lock(OffsetToLock, SizeToLock, reinterpret_cast<void **>(ppbData), Flags);','HRESULT result=ProxyInterface->Lock(OffsetToLock, SizeToLock, reinterpret_cast<void **>(ppbData), Flags);'+"`n"+'    if(SUCCEEDED(result) && !(Flags & D3DLOCK_READONLY)) captureVertexLock(ProxyInterface,OffsetToLock,SizeToLock,*ppbData);'+"`n"+'    return result;')
    $text=$text.Replace('return ProxyInterface->Unlock();','captureVertexUnlock(ProxyInterface);'+"`n"+'    return ProxyInterface->Unlock();')
    $text=$text.Replace('return ProxyInterface->Release();','ULONG result=ProxyInterface->Release();'+"`n"+'    if(!result) forgetVertexBuffer(ProxyInterface);'+"`n"+'    return result;')
    [IO.File]::WriteAllText($path,$text)
}
$path=Join-Path $SourcePath 'CMakeLists.txt'
$text=[IO.File]::ReadAllText($path)
if(-not $text.Contains('source/NeuralBridgeExports.cpp')) {
    $include=(Join-Path $repositoryPath '.build\streamline-sdk-v2.14.1\include').Replace('\','/')
    $text+="`n"+'target_sources(d3d8to9 PRIVATE source/NeuralBridgeExports.cpp)'+"`n"+'target_include_directories(d3d8to9 PRIVATE "'+$include+'")'+"`n"+'target_compile_definitions(d3d8to9 PRIVATE NOMINMAX)'+"`n"+'target_link_libraries(d3d8to9 d3d12 d3dcompiler)'+"`n"
    [IO.File]::WriteAllText($path,$text)
}
if(-not $text.Contains('target_link_libraries(d3d8to9 dxgi)')) {
    $text+="`n"+'target_link_libraries(d3d8to9 dxgi)'+"`n"
    [IO.File]::WriteAllText($path,$text)
}
