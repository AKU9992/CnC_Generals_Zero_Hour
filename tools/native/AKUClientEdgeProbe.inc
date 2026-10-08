        static bool akuEdgeProbeDone=false;
        if (!akuEdgeProbeDone && quitSeconds>0 && getenv("GENERALS_TEST_EDGE_SCROLL") &&
            timeGetTime()-quitTestStart>7000 && TheGameLogic && TheGameLogic->isInInteractiveGame() &&
            TheLookAtTranslator && TheTacticalView && TheInGameUI) {
            akuEdgeProbeDone=true;
            const Bool input=TheInGameUI->getInputEnabled();TheInGameUI->setInputEnabled(TRUE);
            TheTacticalView->lockUserControlUntilFrame(0);
            AsciiString edgePath=TheGlobalData->getPath_UserData();edgePath.concat("AKU9992-edge-test.csv");
            auto send=[&](const char* stage,int x,int y,bool captured) {
                _putenv_s("GENERALS_TEST_CAPTURED",captured ? "1" : "0");
                TheLookAtTranslator->resetModes();
                ICoord2D pos;pos.x=x;pos.y=y;
                GameMessage* move=TheMessageStream->appendMessage(GameMessage::MSG_RAW_MOUSE_POSITION);
                move->appendPixelArgument(pos);move->appendIntegerArgument(0);
                TheLookAtTranslator->translateGameMessage(move);
                Coord3D before=TheTacticalView->getPosition();
                GameMessage* tick=TheMessageStream->appendMessage(GameMessage::MSG_FRAME_TICK);
                TheLookAtTranslator->translateGameMessage(tick);
                Coord3D after=TheTacticalView->getPosition();
                Coord2D amount=TheInGameUI->getScrollAmount();
                FILE* out=nullptr;
                if(fopen_s(&out,edgePath.str(),"a")==0){
                    fprintf(out,"%s,%d,%d,%d,%.6f,%.6f,%d\n",stage,x,y,captured?1:0,amount.x,amount.y,before.x!=after.x || before.y!=after.y ? 1 : 0);fclose(out);
                }
            };
            int width=TheDisplay->getWidth(),height=TheDisplay->getHeight();
            auto edges=[&](const char* stage){
                send(stage,0,height/2,true);send(stage,width-1,height/2,true);
                send(stage,width/2,0,true);send(stage,width/2,height-1,true);
                send(stage,0,0,true);send(stage,width-1,0,true);
                send(stage,0,height-1,true);send(stage,width-1,height-1,true);
                send(stage,width/2,height/2,true);send(stage,0,height/2,false);
            };
            edges("initial");
            // Reproduce applying old disabled preference values from Options.ini.
            TheMouse->setCursorCaptureMode(0);
            TheLookAtTranslator->setScreenEdgeScrollMode(0);
            edges("legacy-settings");
            OptionPreferences edgePrefs;
            TheMouse->setCursorCaptureMode(edgePrefs.getCursorCaptureMode());
            TheLookAtTranslator->setScreenEdgeScrollMode(edgePrefs.getScreenEdgeScrollMode());
            edges("preferences");
            TheLookAtTranslator->resetModes();
            _putenv_s("GENERALS_TEST_CAPTURED","");
            TheInGameUI->setInputEnabled(input);
        }
