[CmdletBinding()]
param([Parameter(Mandatory)][string]$SourcePath)
$ErrorActionPreference = 'Stop'
$expected = Join-Path (Split-Path $PSScriptRoot -Parent) '.build\community-reference-x64\GeneralsGameCode-b805c12ee1aedc0a4b241006803b8e04bbf288a6'
if ([IO.Path]::GetFullPath($SourcePath) -ne [IO.Path]::GetFullPath($expected)) { throw 'Only the isolated x64 source may be patched.' }
$helper = @'
// generals-mods texture failure diagnostics
#include <atomic>
#include <cstdio>
static void traceTextureFailure(const char* stage, const char* name, long code = 0)
{
    char enabled[2] = {};
    if (!GetEnvironmentVariableA("GENERALS_X64_STARTUP_TRACE", enabled, sizeof(enabled))) return;
    static std::atomic<unsigned> count{0};
    if (count.fetch_add(1) >= 1024) return;
    FILE* log = nullptr;
    if (fopen_s(&log, "GeneralsTextureFailures.log", "a") == 0 && log) {
        fprintf(log, "PID %lu: %s code %ld file %s\n", GetCurrentProcessId(),stage,code,name);
        fclose(log);
    }
}

'@
$patches = @(
    @('Core\Libraries\Source\WWVegas\WW3D2\textureloader.cpp', 'void TextureLoadTaskClass::Apply_Missing_Texture()', 'D3DTexture = MissingTexture::_Get_Missing_Texture();', 'traceTextureFailure("Missing texture", Texture->Get_Full_Path().str());'),
    @('Core\Libraries\Source\WWVegas\WWLib\targa.cpp', 'long Targa_Error_Handler(long load_err,const char* filename)', 'switch (load_err) {', 'if (load_err) traceTextureFailure("TGA failure", filename, load_err);'),
    @('Core\Libraries\Source\WWVegas\WW3D2\dx8wrapper.cpp', 'IDirect3DSurface8 * DX8Wrapper::_Create_DX8_Surface(const char *filename_)', 'return MissingTexture::_Create_Missing_Surface();', 'traceTextureFailure("Missing surface", filename_);')
)
foreach ($patch in $patches) {
    $path = Join-Path $SourcePath $patch[0]
    $text = [IO.File]::ReadAllText($path)
    if ($text.Contains('// generals-mods texture failure diagnostics')) { continue }
    $text = $text.Replace($patch[1], $helper + $patch[1])
    if ($patch[0].EndsWith('dx8wrapper.cpp')) {
        $text = $text.Replace("if (!myfile2->Is_Available())`r`n`t`t`t`t" + $patch[2], 'if (!myfile2->Is_Available()) { ' + $patch[3] + ' ' + $patch[2] + ' }')
        $text = $text.Replace("if (!myfile2->Is_Available())`n`t`t`t`t" + $patch[2], 'if (!myfile2->Is_Available()) { ' + $patch[3] + ' ' + $patch[2] + ' }')
    } else { $text = $text.Replace($patch[2], $patch[3] + "`n" + $patch[2]) }
    [IO.File]::WriteAllText($path,$text)
}
