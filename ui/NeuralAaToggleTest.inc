// Opt-in integration test: real menu controls, renderer and preference file.
void generalsTestNeuralToggleTick() {
    static const bool enabled=[] { char value[8]={}; return GetEnvironmentVariableA("GENERALS_TEST_NEURAL_TOGGLE",value,sizeof(value)) && value[0]=='1'; }();
    if(!enabled) return;
    static DWORD next=GetTickCount()+5000;
    static Int stage=0;
    if(stage>=9 || GetTickCount()<next || TheGlobalData->m_loadScreenRender) return;
    next=GetTickCount()+1500;
    auto record=[](const char* label,Bool passed) {
        FILE* file=nullptr;
        if(fopen_s(&file,"GeneralsNeuralToggleTest.log","a")==0 && file) {
            fprintf(file,"PID %lu: %s: %s\n",GetCurrentProcessId(),passed ? "PASS":"FAIL",label); fclose(file);
        }
    };
    if(stage==0) {
        WindowLayout* layout=TheShell->getOptionsLayout(TRUE);
        if(!layout) {record("create options menu",FALSE);stage=8;return;}
        layout->runInit(); layout->hide(FALSE); layout->bringForward();

        record("toggle button created",neuralToggleButton!=nullptr);
    }
    if(!neuralToggleButton || !pref) {record("live menu controls",FALSE);stage=8;return;}
    auto click=[] {
        TheWindowManager->winSendSystemMsg(neuralToggleButton->winGetOwner(),GBM_SELECTED,
            reinterpret_cast<WindowMsgData>(neuralToggleButton),0);
    };
    auto choose=[](Int choice) {
        GadgetComboBoxSetSelectedPos(comboBoxAntiAliasing,choice);
        TheWindowManager->winSendSystemMsg(neuralToggleButton->winGetOwner(),GCM_SELECTED,
            reinterpret_cast<WindowMsgData>(comboBoxAntiAliasing),0);
    };
    if(stage==0) { choose(OptionPreferences::AntiAliasingMode_MSAA_8X); choose(OptionPreferences::AntiAliasingMode_Count+1); }
    else if(stage==3) choose(OptionPreferences::AntiAliasingMode_Count);
    else if(stage<8) click();
    else {
        comboBoxAntiAliasing->winBringToTop();
        TheWindowManager->winSendInputMsg(comboBoxAntiAliasing,GWM_LEFT_UP,0,0);
        record("dropdown open above settings panels",!GadgetComboBoxGetListBox(comboBoxAntiAliasing)->winIsHidden() &&
            comboBoxAntiAliasing->winGetParent()==neuralToggleButton->winGetOwner());
        record("toggle integration completed",TRUE); ++stage; return;
    }
    const Bool applied=applyNeuralSelection(*pref);
    const auto expected=(stage==1 || stage==4 || stage==6) ? Display::NEURAL_AA_OFF:
        stage<3 ? Display::NEURAL_AA_DLAA:Display::NEURAL_AA_DLSS_QUALITY;
    const AsciiString expectedPreference=expected==Display::NEURAL_AA_OFF ? "Off":
        expected==Display::NEURAL_AA_DLAA ? "DLAA":"DLSSQuality";
    char label[100]; sprintf_s(label,"stage %d renderer and pending preference",stage);
    record(label,applied && TheDisplay->getNeuralAAMode()==expected && (*pref)["NeuralAntiAliasing"]==expectedPreference);
    if(expected==Display::NEURAL_AA_OFF) record("standard MSAA restored",(*pref)["AntiAliasing"]=="8" && TheWritableGlobalData->m_antiAliasLevel==8);
    if(stage==6) {
        pref->write();
        OptionPreferences saved;
        record("disabled state and remembered DLSS persisted",saved["NeuralAntiAliasing"]=="Off" && saved["NeuralAntiAliasingLastMode"]=="DLSSQuality");
        delete pref; pref=nullptr;
        TheShell->destroyOptionsLayout(); neuralToggleButton=nullptr;
        auto* reopened=TheShell->getOptionsLayout(TRUE);
        reopened->runInit(); reopened->hide(FALSE); reopened->bringForward();

        Int choice=-1; GadgetComboBoxGetSelectedPos(comboBoxAntiAliasing,&choice);
        record("reopened menu restores standard AA and remembered DLSS",choice==OptionPreferences::AntiAliasingMode_MSAA_8X && rememberedNeuralChoice==OptionPreferences::AntiAliasingMode_Count);
    }
    if(stage==7) {
        pref->write(); OptionPreferences saved;
        record("enabled DLSS state persisted",saved["NeuralAntiAliasing"]=="DLSSQuality" && saved["AntiAliasing"]=="0");
    }
    ++stage;
}
