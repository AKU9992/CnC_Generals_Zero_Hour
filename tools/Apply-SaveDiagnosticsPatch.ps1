param([Parameter(Mandatory)][string]$SourcePath)
$ErrorActionPreference='Stop'
foreach($edition in @('Generals','GeneralsMD')){
$p=Join-Path $SourcePath ($edition+'/Code/GameEngine/Source/Common/System/SaveGame/GameState.cpp')
$t=[IO.File]::ReadAllText($p)
if($t.Contains('AKU9992 load diagnostics')){continue}
$helper=@'
// AKU9992 load diagnostics: isolated tests only, subsystem tokens without filenames.
static void akuLoadTrace(const char* phase,const char* block,int status){
 if(!getenv("GENERALS_TEST_QUIT_SECONDS"))return;
 AsciiString path=TheGlobalData->getPath_UserData();path.concat("AKU9992-save-load.log");
 FILE* file=nullptr;if(fopen_s(&file,path.str(),"a")==0){fprintf(file,"%s,%s,%d\n",phase,block,status);fclose(file);}
}
'@
$t=$t.Replace('SaveCode GameState::loadGame(', $helper+[Environment]::NewLine+'SaveCode GameState::loadGame(')
$t=$t.Replace('xfer->xferSnapshot( blockInfo->snapshot );','akuLoadTrace(xfer->getXferMode()==XFER_LOAD?"load_block":"save_block",blockInfo->blockName.str(),0);'+[Environment]::NewLine+'                    xfer->xferSnapshot( blockInfo->snapshot );')
$a=$t.IndexOf('SaveCode GameState::loadGame(');$b=$t.IndexOf('void GameState::',$a);$part=$t.Substring($a,$b-$a)
$part=$part.Replace('catch( ... )','catch(XferStatus status){akuLoadTrace("load_error","snapshot",static_cast<int>(status));error=TRUE;}'+[Environment]::NewLine+'    catch( ... )')
$part=$part.Replace('catch (...)','catch(XferStatus status){akuLoadTrace("load_error","postprocess",static_cast<int>(status));error=TRUE;}'+[Environment]::NewLine+'    catch (...)')
$part=$part.Replace('return SC_INVALID_DATA;','akuLoadTrace("load_error","invalid_data",-1);'+[Environment]::NewLine+'        return SC_INVALID_DATA;')
$t=$t.Substring(0,$a)+$part+$t.Substring($b)
[IO.File]::WriteAllText($p,$t)
}
