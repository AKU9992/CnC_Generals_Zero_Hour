// AKU9992 fixed simulation catchup: bounded work; no discarded ticks.
        if (canUpdateGameLogic(FramePacer::IgnoreFrozenTime)) {
            const bool native=GetModuleHandleW(L"generals-native12.dll")!=nullptr;
            for(int tick=0;tick<(native?32:1);++tick){
                akuProfileCall(AkuLogic,[&]{TheGameLogic->UPDATE();});
                if(!TheFramePacer->isTimeFrozen())TheGameClient->step();
                if(!native || tick==31)break;
                if(TheNetwork){
                    // AKU9992 refresh network readiness per tick.
                    akuProfileCall(AkuNetwork,[&]{TheNetwork->UPDATE();});
                    if(!canUpdateNetworkGameLogic())break;
                    TheGameLogic->preUpdate();
                }else{
                    const Int fps=TheFramePacer->getActualLogicTimeScaleFps(FramePacer::IgnoreFrozenTime);
                    if(fps<=0 || fps>=RenderFpsPreset::UncappedFpsValue || TheFramePacer->isGameHalted())break;
                    const Real step=1.0f/fps;
                    if(m_logicTimeAccumulator<step)break;
                    m_logicTimeAccumulator-=step;
                    TheGameLogic->preUpdate();
                }
            }
        }