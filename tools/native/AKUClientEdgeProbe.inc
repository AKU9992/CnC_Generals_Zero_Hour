        static bool akuEdgeProbeDone=false;
        if (!akuEdgeProbeDone && quitSeconds>0 && getenv("GENERALS_TEST_EDGE_SCROLL") &&
            timeGetTime()-quitTestStart>7000 && TheLookAtTranslator && TheTacticalView && TheInGameUI) {
            akuEdgeProbeDone=true;
            const Bool input=TheInGameUI->getInputEnabled();TheInGameUI->setInputEnabled(TRUE);
            TheTacticalView->lockUserControlUntilFrame(0);
            auto send=[&](int x,int y,bool captured) {
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
                const bool motion=before.x!=after.x || before.y!=after.y;
                const bool command=amount.x!=0 || amount.y!=0;
                FILE* out=nullptr;
                if(fopen_s(&out,"AKU9992-edge-test.csv","a")==0){
                    fprintf(out,"%d,%d,%d,%d,%d\n",x,y,captured?1:0,command?1:0,motion?1:0);fclose(out);
                }
            };
            int width=TheDisplay->getWidth(),height=TheDisplay->getHeight();
            send(0,height/2,true);send(width-1,height/2,true);
            send(width/2,0,true);send(width/2,height-1,true);
            send(0,0,true);send(width/2,height/2,true);send(0,height/2,false);
            TheLookAtTranslator->resetModes();
            _putenv_s("GENERALS_TEST_CAPTURED","");
            TheInGameUI->setInputEnabled(input);
        }