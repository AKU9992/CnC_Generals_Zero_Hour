#ifndef AKU_GAMEPLAY_PROBE_H
#define AKU_GAMEPLAY_PROBE_H
#include "GameNetwork/GameInfo.h"
#include "GameClient/MapUtil.h"
#include "Common/PlayerList.h"
#include "Common/Player.h"
#include "Common/PlayerTemplate.h"
#include "Common/RandomValue.h"
#include "Common/ThingFactory.h"
#include "Common/GameState.h"
#include "GameLogic/Object.h"
#include "GameLogic/AI.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Module/AIUpdate.h"
#include <algorithm>
#include <vector>
#include <cmath>
namespace {
struct AkuGameplayProbe {
 bool requested=false,started=false,spawned=false;UnsignedInt lastCrcFrame=0;std::vector<ObjectID> units;Coord3D center{};
 const char* kind=nullptr;
 void write(const char* name,const char* text){
  AsciiString path=TheGlobalData->getPath_UserData();path.concat(name);FILE* out=nullptr;
  if(fopen_s(&out,path.str(),"a")==0){fputs(text,out);fclose(out);}
 }
 void start(DWORD elapsed){
  if(!requested){requested=true;kind=getenv("AKU_TEST_SCENARIO");}
  if(!kind || !kind[0] || !getenv("GENERALS_TEST_QUIT_SECONDS"))return;
  if(!started && elapsed>2500 && TheMapCache && TheGameLogic){
   started=true;
   const char* chosen=getenv("AKU_TEST_MAP");AsciiString map=chosen && chosen[0]?chosen:"maps\\Tournament Lake\\Tournament Lake.map";
   const MapMetaData* metadata=TheMapCache->findMap(map);
   if(!metadata){write("AKU9992-scenario.log","Map unavailable.\n");return;}
   if(TheGameLogic->isInGame())TheGameLogic->clearGameData(FALSE);
   if(!TheSkirmishGameInfo)TheSkirmishGameInfo=NEW SkirmishGameInfo;
   TheSkirmishGameInfo->init();TheSkirmishGameInfo->reset();TheSkirmishGameInfo->clearSlotList();TheSkirmishGameInfo->enterGame();TheSkirmishGameInfo->setLocalIP(1);
   const int players=strcmp(kind,"MultiAI")==0?(std::min)(metadata->m_numPlayers,4):2;
   for(int i=0;i<MAX_SLOTS;++i){GameSlot slot;slot.setState(i==0?SLOT_PLAYER:i<players?SLOT_MED_AI:SLOT_CLOSED,UnicodeString(L"AKU9992"),i==0?1:0);slot.setColor(i);const char* factions[]={"FactionAmerica","FactionChina","FactionGLA"};int faction=-1;for(int n=0;n<ThePlayerTemplateStore->getPlayerTemplateCount();++n){if(ThePlayerTemplateStore->getNthPlayerTemplate(n)->getName()==factions[i%3]){faction=n;break;}}slot.setPlayerTemplate(faction);slot.setStartPos(i);TheSkirmishGameInfo->setSlot(i,slot);}
   TheSkirmishGameInfo->setMap(map);TheSkirmishGameInfo->setSeed(9992);TheSkirmishGameInfo->startGame(0);
   TheWritableGlobalData->m_mapName=map;InitRandom(9992);TheShell->hideShell();
   GameMessage* message=TheMessageStream->appendMessage(GameMessage::MSG_NEW_GAME);
   message->appendIntegerArgument(GAME_SKIRMISH);message->appendIntegerArgument(DIFFICULTY_NORMAL);message->appendIntegerArgument(0);message->appendIntegerArgument(30);
   write("AKU9992-scenario.log","Queued fixed-seed skirmish.\n");
  }
 }
 void step(){
  if(!kind || !started || !TheGameLogic->isInSkirmishGame() || !getenv("GENERALS_TEST_QUIT_SECONDS"))return;
  const UnsignedInt frame=TheGameLogic->getFrame();
  if(!spawned && frame>=2){
   spawned=true;Region3D extent{};TheTerrainLogic->getExtent(&extent);center.x=(extent.lo.x+extent.hi.x)*.5f;center.y=(extent.lo.y+extent.hi.y)*.5f;center.z=TheTerrainLogic->getGroundHeight(center.x,center.y);
   TheTacticalView->lockUserControlUntilFrame(0);TheTacticalView->lookAt(&center);
   ThePartitionManager->revealMapForPlayerPermanently(ThePlayerList->getLocalPlayer()->getPlayerIndex());
   const int count=strcmp(kind,"Battle")==0?200:strcmp(kind,"Movement")==0?24:strcmp(kind,"LargeBase")==0?100:0;
   for(int i=0;i<count;++i){
    const int side=strcmp(kind,"Battle")==0?i%2:0;Player* player=ThePlayerList->getNthPlayer(side+1);
    const ThingTemplate* type=TheThingFactory->findTemplate(side?"ChinaTankBattleMaster":strcmp(kind,"LargeBase")==0 && i%3==0?"AmericaPowerPlant":strcmp(kind,"LargeBase")==0 && i%3==1?"AmericaWarFactory":"AmericaTankCrusader",FALSE);if(!player || !type)continue;
    Object* object=TheThingFactory->newObject(type,player->getDefaultTeam());if(!object)continue;
    Coord3D pos=center;const float spacing=strcmp(kind,"LargeBase")==0?70.f:22.f;pos.x+=(side?150.f:-350.f)+spacing*((i/2)%10);pos.y+=-220.f+spacing*(i/20);pos.z=TheTerrainLogic->getGroundHeight(pos.x,pos.y);object->setPosition(&pos);units.push_back(object->getID());
    if(strcmp(kind,"Movement")==0 && object->getAIUpdateInterface()){Coord3D destination=pos;destination.x+=500;object->getAIUpdateInterface()->aiMoveToPosition(&destination,CMD_FROM_SCRIPT);}
   }
   if(strcmp(kind,"Battle")==0 && units.size()>1){for(size_t i=0;i<units.size();++i){Object* unit=TheGameLogic->findObjectByID(units[i]);Object* victim=TheGameLogic->findObjectByID(units[i%2?0:1]);if(unit && victim && unit->getAIUpdateInterface())unit->getAIUpdateInterface()->aiAttackObject(victim,1000,CMD_FROM_SCRIPT);}}
   char data[128];sprintf_s(data,"Skirmish active; players=%d; spawned_units=%u; seed=9992\n",ThePlayerList->getPlayerCount(),static_cast<unsigned>(units.size()));write("AKU9992-scenario.log",data);
  }
  if(frame && frame%60==0 && frame!=lastCrcFrame){
   lastCrcFrame=frame;const UnsignedInt crc=TheGameLogic->getCRC(CRC_RECALC);UINT count=0;for(Object* object=TheGameLogic->getFirstObject();object;object=object->getNextObject())++count;
   Coord3D pos{};if(!units.empty()){Object* object=TheGameLogic->findObjectByID(units[0]);if(object)pos=*object->getPosition();}
   char line[180];sprintf_s(line,"%u,%08X,%u,%.6f,%.6f,%.6f\n",frame,crc,count,pos.x,pos.y,pos.z);write("AKU9992-gameplay-crc.csv",line);
  }
  if(strcmp(kind,"Camera")==0){Coord3D pos=center;pos.x+=150.f*std::sin(float(frame)*.03f);pos.y+=150.f*std::cos(float(frame)*.03f);TheTacticalView->lookAt(&pos);}
  if(getenv("AKU_TEST_SAVE") && frame==300){
   const auto result=TheGameState->saveGame(AsciiString("AKU9992-test.sav"),UnicodeString(L"AKU9992"),SAVE_FILE_TYPE_NORMAL);
   char line[80];sprintf_s(line,"Save result %d at logic frame 300\n",static_cast<int>(result.saveCode));write("AKU9992-scenario.log",line);
  }
 }
};
AkuGameplayProbe& akuGameplayProbe(){static AkuGameplayProbe probe;return probe;}
}
#endif