[CmdletBinding()]
param([Parameter(Mandatory)][string]$SourcePath)
$ErrorActionPreference = 'Stop'
$repositoryPath = Split-Path $PSScriptRoot -Parent
$expectedPath = Join-Path $repositoryPath '.build\d3d8to9-reference-x64\d3d8to9-255338f698c8270b537f0a91a13f795f4f988250'
if ([IO.Path]::GetFullPath($SourcePath).TrimEnd('\') -ne [IO.Path]::GetFullPath($expectedPath).TrimEnd('\')) { throw 'Only the pinned x64 bridge may be patched.' }
Copy-Item -LiteralPath (Join-Path $repositoryPath 'renderer\BridgePointerHandles.h') -Destination (Join-Path $SourcePath 'source\BridgePointerHandles.h') -Force
Copy-Item -LiteralPath (Join-Path $repositoryPath 'renderer\BridgeResourceInterop.cpp') -Destination (Join-Path $SourcePath 'source\BridgeResourceInterop.cpp') -Force
$cmakePath = Join-Path $SourcePath 'CMakeLists.txt'
$cmake = [IO.File]::ReadAllText($cmakePath)
if (-not $cmake.Contains('source/BridgeResourceInterop.cpp')) {
    [IO.File]::WriteAllText($cmakePath, $cmake + "`n" + 'target_sources(d3d8to9 PRIVATE source/BridgeResourceInterop.cpp)' + "`n")
}
$headerPath = Join-Path $SourcePath 'source\d3d8to9.hpp'
$header = [IO.File]::ReadAllText($headerPath)
if (-not $header.Contains('BridgePointerHandles pointerHandles_')) {
    $header = $header.Replace('#include "d3d8.hpp"', '#include "d3d8.hpp"' + "`n" + '#include "BridgePointerHandles.h"')
    $header = $header.Replace('std::unordered_set<DWORD> PixelShaderHandles, VertexShaderHandles, StateBlockTokens;', 'std::unordered_set<DWORD> PixelShaderHandles, VertexShaderHandles, StateBlockTokens;' + "`n" + '    BridgePointerHandles pointerHandles_;')
    [IO.File]::WriteAllText($headerPath, $header)
}
$path = Join-Path $SourcePath 'source\d3d8to9_device.cpp'
$text = [IO.File]::ReadAllText($path)
if ($text.Contains('// generals-mods x64 opaque handles')) { return }
$changes = @(
    @('HRESULT hr = ProxyInterface->EndStateBlock(reinterpret_cast<IDirect3DStateBlock9**>(pToken));', "IDirect3DStateBlock9* block = nullptr;`n`t*pToken = 0;`n`tHRESULT hr = ProxyInterface->EndStateBlock(&block);`n`tif (SUCCEEDED(hr)) *pToken = pointerHandles_.add(block);"),
    @('HRESULT hr = ProxyInterface->CreateStateBlock(Type, reinterpret_cast<IDirect3DStateBlock9 **>(pToken));', "IDirect3DStateBlock9* block = nullptr;`n`t*pToken = 0;`n`tHRESULT hr = ProxyInterface->CreateStateBlock(Type, &block);`n`tif (SUCCEEDED(hr)) *pToken = pointerHandles_.add(block);"),
    @('reinterpret_cast<IDirect3DStateBlock9 *>(Token)->Apply()', 'static_cast<IDirect3DStateBlock9 *>(pointerHandles_.get(Token))->Apply()'),
    @('reinterpret_cast<IDirect3DStateBlock9 *>(Token)->Capture()', 'static_cast<IDirect3DStateBlock9 *>(pointerHandles_.get(Token))->Capture()'),
    @('reinterpret_cast<IDirect3DStateBlock9 *>(Token)->Release()', 'static_cast<IDirect3DStateBlock9 *>(pointerHandles_.take(Token))->Release()'),
    @("assert((reinterpret_cast<DWORD>(ShaderInfo) & 1) == 0);`r`n`t`t`tconst DWORD ShaderMagic = reinterpret_cast<DWORD>(ShaderInfo) >> 1;`r`n`r`n`t`t`t*pHandle = ShaderMagic | 0x80000000;", '*pHandle = pointerHandles_.add(ShaderInfo);'),
    @("const DWORD handleMagic = Handle << 1;`r`n`t`tVertexShaderInfo *const ShaderInfo = reinterpret_cast<VertexShaderInfo *>(handleMagic);", "VertexShaderInfo *const ShaderInfo = static_cast<VertexShaderInfo *>(pointerHandles_.get(Handle));`n`t`tif (!ShaderInfo) return D3DERR_INVALIDCALL;"),
    @("const DWORD HandleMagic = Handle << 1;`r`n`tVertexShaderInfo *const ShaderInfo = reinterpret_cast<VertexShaderInfo *>(HandleMagic);", 'VertexShaderInfo *const ShaderInfo = static_cast<VertexShaderInfo *>(pointerHandles_.take(Handle));'),
    @("const DWORD HandleMagic = Handle << 1;`r`n`tIDirect3DVertexShader9 *VertexShaderInterface = reinterpret_cast<VertexShaderInfo *>(HandleMagic)->Shader;", "auto* shaderInfo = static_cast<VertexShaderInfo *>(pointerHandles_.get(Handle));`n`tif (!shaderInfo || !shaderInfo->Shader) return D3DERR_INVALIDCALL;`n`tIDirect3DVertexShader9 *VertexShaderInterface = shaderInfo->Shader;"),
    @('hr = ProxyInterface->CreatePixelShader(static_cast<const DWORD *>(Assembly->GetBufferPointer()), reinterpret_cast<IDirect3DPixelShader9 **>(pHandle));', "IDirect3DPixelShader9* shader = nullptr;`n`t*pHandle = 0;`n`thr = ProxyInterface->CreatePixelShader(static_cast<const DWORD *>(Assembly->GetBufferPointer()), &shader);`n`tif (SUCCEEDED(hr)) *pHandle = pointerHandles_.add(shader);"),
    @('const HRESULT hr = ProxyInterface->SetPixelShader(reinterpret_cast<IDirect3DPixelShader9 *>(Handle));', "auto* shader = static_cast<IDirect3DPixelShader9 *>(pointerHandles_.get(Handle));`n`tif (Handle && !shader) return D3DERR_INVALIDCALL;`n`tconst HRESULT hr = ProxyInterface->SetPixelShader(shader);"),
    @('reinterpret_cast<IDirect3DPixelShader9 *>(Handle)->Release()', 'static_cast<IDirect3DPixelShader9 *>(pointerHandles_.take(Handle))->Release()'),
    @('IDirect3DPixelShader9 *const PixelShaderInterface = reinterpret_cast<IDirect3DPixelShader9 *>(Handle);', "IDirect3DPixelShader9 *const PixelShaderInterface = static_cast<IDirect3DPixelShader9 *>(pointerHandles_.get(Handle));`n`tif (!PixelShaderInterface) return D3DERR_INVALIDCALL;")
)
# Compare normalized line endings: the generic bridge patch already rewrites some files.
$text = $text.Replace("`r`n", "`n")
foreach ($change in $changes) {
    $old = $change[0].Replace("`r`n", "`n")
    if ([regex]::Matches($text, [regex]::Escape($old)).Count -ne 1) { throw ('Unexpected x64 handle layout: ' + $old) }
    $text = $text.Replace($old, $change[1])
}
$text = '// generals-mods x64 opaque handles' + "`n" + $text
[IO.File]::WriteAllText($path, $text)
