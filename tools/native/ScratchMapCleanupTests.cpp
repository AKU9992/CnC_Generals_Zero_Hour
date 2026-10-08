#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <filesystem>
#include <fstream>
struct AsciiString{
 std::string value;AsciiString()=default;AsciiString(std::string v):value(std::move(v)){}
 const char* str()const{return value.c_str();}void concat(const char* s){value+=s;}
};
struct FileSystem{bool createDirectory(AsciiString p){return CreateDirectoryA(p.str(),nullptr)!=0;}};
struct GameState{AsciiString directory;AsciiString getSaveDirectory(){return directory;}};
struct GameStateMap{void clearScratchPadMaps();};
FileSystem fs;FileSystem* TheFileSystem=&fs;GameState state;GameState* TheGameState=&state;
#include "AKUScratchMapCleanup.inc"
enum { XFER_LOAD, XFER_SAVE, SLOT_CLOSED, SLOT_PLAYER };
struct Xfer{int mode;int getXferMode(){return mode;}};
struct GameSlot{int state=SLOT_CLOSED;unsigned ip=0;int getState(){return state;}void setIP(unsigned v){ip=v;}};
constexpr int MAX_SLOTS=8;
struct Info{
 bool m_inGame=true;unsigned m_localIP=9992;GameSlot slots[MAX_SLOTS];GameSlot* m_slot[MAX_SLOTS];
 Info(){for(int i=0;i<MAX_SLOTS;++i)m_slot[i]=&slots[i];}
 int getLocalSlotNum(){for(int i=0;i<MAX_SLOTS;++i)if(slots[i].state==SLOT_PLAYER && slots[i].ip==m_localIP)return i;return -1;}
 void restore(Xfer* xfer){
#include "AKULocalSavePlayer.inc"
 }
};
int main(){
 namespace f=std::filesystem;
 const f::path root=f::current_path()/"AKU9992-cleanup-test";
 f::create_directories(root);std::ofstream(root/"protected.map")<<"outside";
 state.directory=AsciiString((root/"Save").string()+"\\");
 char before[32768]{},after[32768]{};GetCurrentDirectoryA(sizeof(before),before);
 GameStateMap maps;maps.clearScratchPadMaps();
 if(!f::is_directory(root/"Save"))return 1;
 f::create_directories(root/"Save"/"nested");
 std::ofstream(root/"Save"/"remove.map")<<"temporary";
 std::ofstream(root/"Save"/"keep.sav")<<"save";
 std::ofstream(root/"Save"/"nested"/"keep.map")<<"nested";
 maps.clearScratchPadMaps();GetCurrentDirectoryA(sizeof(after),after);
 if(strcmp(before,after) || f::exists(root/"Save"/"remove.map") || !f::exists(root/"protected.map") || !f::exists(root/"Save"/"keep.sav") || !f::exists(root/"Save"/"nested"/"keep.map"))return 2;
 Xfer load{XFER_LOAD};Info single;single.slots[0].state=SLOT_PLAYER;single.restore(&load);if(single.getLocalSlotNum()!=0)return 3;
 Info multi;multi.slots[0].state=multi.slots[1].state=SLOT_PLAYER;multi.restore(&load);if(multi.getLocalSlotNum()!=-1)return 4;
 Info known;known.slots[2].state=SLOT_PLAYER;known.slots[2].ip=9992;known.restore(&load);if(known.getLocalSlotNum()!=2)return 5;
 Xfer save{XFER_SAVE};Info untouched;untouched.slots[0].state=SLOT_PLAYER;untouched.restore(&save);if(untouched.slots[0].ip)return 6;
 puts("PASS Save directory creation, scoped map cleanup, protected files and unchanged working directory");
 puts("PASS saved local slot reconstruction: single/multiple human, existing match and write mode");
 return 0;
}
