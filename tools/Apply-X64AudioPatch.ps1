[CmdletBinding()]
param([Parameter(Mandatory)][string]$SourcePath)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$expected=Join-Path $repo '.build/community-reference-x64/GeneralsGameCode-b805c12ee1aedc0a4b241006803b8e04bbf288a6'
if([IO.Path]::GetFullPath($SourcePath).TrimEnd('\') -ne [IO.Path]::GetFullPath($expected).TrimEnd('\')){throw 'Only the pinned x64 game source may be patched.'}
$miles=Join-Path $SourcePath 'Dependencies/Miles'
Copy-Item -LiteralPath (Join-Path $repo 'audio/X64AudioBackend.h') -Destination (Join-Path $miles 'X64AudioBackend.h') -Force
$cmakePath=Join-Path $miles 'CMakeLists.txt'
$cmake=[IO.File]::ReadAllText($cmakePath)
if(-not $cmake.Contains('target_compile_features(deps_miles PRIVATE cxx_std_17)')) {
    [IO.File]::WriteAllText($cmakePath,$cmake+"`ntarget_compile_features(deps_miles PRIVATE cxx_std_17)`ntarget_compile_definitions(deps_miles PRIVATE NOMINMAX)`n")
}
$path=Join-Path $miles 'MilesLoader.cpp'
$text=[IO.File]::ReadAllText($path).Replace("`r`n","`n")
$names=@('startup','shutdown','quick_startup','quick_handles','set_file_callbacks','allocate_sample_handle','allocate_3D_sample_handle','release_sample_handle','release_3D_sample_handle','init_sample','set_sample_file','set_3D_sample_file','start_sample','start_3D_sample','stop_sample','stop_3D_sample','resume_sample','resume_3D_sample','end_sample','end_3D_sample','set_sample_volume_pan','sample_volume_pan','set_3D_sample_volume','set_sample_playback_rate','sample_playback_rate','set_3D_sample_playback_rate','set_sample_user_data','sample_user_data','set_3D_user_data','register_EOS_callback','register_3D_EOS_callback','register_stream_callback','set_sample_loop_count','sample_loop_count','set_3D_sample_loop_count','set_stream_loop_count','stream_loop_count','sample_ms_position','stream_ms_position','set_sample_ms_position','set_stream_ms_position','open_stream','close_stream','start_stream','pause_stream','set_stream_volume_pan','stream_volume_pan','set_stream_playback_rate','stream_playback_rate','quick_load_and_play','quick_set_volume','quick_unload','enumerate_3D_providers','open_3D_provider','open_3D_listener','set_3D_position','set_3D_orientation','WAV_info','decompress_ADPCM','mem_free_lock')
$bindings=foreach($name in $names){if(-not $text.Contains('AIL_'+$name+'_t')){throw ('Unknown Miles signature: '+$name)};'    AIL_'+$name+'Ptr = &nativeMiles::'+$name+';'}
$bindings+=@('    AIL_3D_sample_volumePtr = &nativeMiles::sample_3D_volume;','    AIL_3D_sample_playback_ratePtr = &nativeMiles::sample_3D_playback_rate;','    AIL_3D_user_dataPtr = &nativeMiles::sample_3D_user_data;','    AIL_3D_sample_loop_countPtr = &nativeMiles::sample_3D_loop_count;')
if(-not $text.Contains('// generals-mods native x64 audio')) {
    $text=$text.Replace('#include "mss/mss.h"',"#include `"mss/mss.h`"`n#if defined(_WIN64)`n#include `"X64AudioBackend.h`" // generals-mods native x64 audio`n#endif")
    $text=$text.Replace('bool MilesLoader::isLoaded()',"#if defined(_WIN64)`nstatic void bindNativeAudio()`n{`n"+($bindings -join "`n")+"`n}`n#endif`n`nbool MilesLoader::isLoaded()")
    $text=$text.Replace('return Module != HMODULE(nullptr);',"return Module != HMODULE(nullptr);`n#elif defined(_WIN64)`n    return ReferenceCount > 0 && !Failed;")
    $text=$text.Replace("#else`n`tFailed = true;`n`treturn false;`n#endif", "#elif defined(_WIN64)`n    bindNativeAudio();`n    return true;`n#else`n`tFailed = true;`n`treturn false;`n#endif")
    [IO.File]::WriteAllText($path,$text)
}
$text=[IO.File]::ReadAllText($path)
if(-not $text.Contains('void generalsAudioPump()')){[IO.File]::AppendAllText($path,"`n#if defined(_WIN64)`nvoid generalsAudioPump() { nativeMiles::pump(); }`n#endif`n")}
$managerPath=Join-Path $SourcePath 'Core/GameEngineDevice/Source/MilesAudioDevice/MilesAudioManager.cpp'
$manager=[IO.File]::ReadAllText($managerPath).Replace("`r`n","`n")
if(-not $manager.Contains('generalsAudioPump();')){
    $manager="#if defined(_WIN64)`nvoid generalsAudioPump();`n#endif`n"+$manager
    $manager=$manager.Replace("void MilesAudioManager::update()`n{","void MilesAudioManager::update()`n{`n#if defined(_WIN64)`n    generalsAudioPump();`n#endif")
}
$manager=$manager.Replace('(UnsignedInt) sampleCompleted','(std::uintptr_t) sampleCompleted').Replace('(UnsignedInt) sample3DCompleted','(std::uintptr_t) sample3DCompleted').Replace('(UnsignedInt) streamCompleted','(std::uintptr_t) streamCompleted')
[IO.File]::WriteAllText($managerPath,$manager)
foreach($relative in @('Core/GameEngine/Include/Common/GameAudio.h','Core/GameEngineDevice/Include/MilesAudioDevice/MilesAudioManager.h','Core/GameEngineDevice/Source/MilesAudioDevice/MilesAudioManager.cpp')){
    $file=Join-Path $SourcePath $relative
    $content=[IO.File]::ReadAllText($file)
    $content=[regex]::Replace($content,'(notifyOfAudioCompletion\(\s*|findPlayingAudioFrom\(\s*)UnsignedInt','$1std::uintptr_t')
    if(-not $content.Contains('#include <cstdint>')){$content="#include <cstdint>`n"+$content}
    [IO.File]::WriteAllText($file,$content)
}
