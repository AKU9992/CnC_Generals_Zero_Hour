// generals-mods neural AA on/off toggle
static GameWindow* neuralToggleButton = nullptr;
static Int rememberedNeuralChoice = OptionPreferences::AntiAliasingMode_Count + 1;
static Int rememberedStandardChoice = OptionPreferences::AntiAliasingMode_MSAA_4X;
static Bool isNeuralChoice(Int choice) {
    return choice >= OptionPreferences::AntiAliasingMode_Count && choice < OptionPreferences::AntiAliasingMode_Count + 2;
}
static void updateNeuralToggle() {
    if (!neuralToggleButton) return;
    Int choice = -1; GadgetComboBoxGetSelectedPos(comboBoxAntiAliasing, &choice);
    if (isNeuralChoice(choice)) rememberedNeuralChoice = choice;
    else if (choice > 0 && choice < OptionPreferences::AntiAliasingMode_Count) rememberedStandardChoice = choice;
    GadgetButtonSetText(neuralToggleButton, UnicodeString(isNeuralChoice(choice) ? L"\u0412\u041a\u041b" : L"\u0412\u042b\u041a\u041b"));
    neuralToggleButton->winEnable(TheDisplay->supportsNeuralAA(Display::NEURAL_AA_DLAA) ||
        TheDisplay->supportsNeuralAA(Display::NEURAL_AA_DLSS_QUALITY));
}
static void createNeuralToggle(const AsciiString& rememberedMode, const AsciiString& standardMode) {
    const auto id = TheNameKeyGenerator->nameToKey("OptionsMenu.wnd:ButtonNeuralToggle");
    neuralToggleButton = TheWindowManager->winGetWindowFromId(nullptr, id);
    rememberedNeuralChoice = OptionPreferences::AntiAliasingMode_Count + (rememberedMode == "DLSSQuality" ? 0 : 1);
    Int standard = atoi(standardMode.str());
    rememberedStandardChoice = standard > 0 && standard < OptionPreferences::AntiAliasingMode_Count
        ? standard : OptionPreferences::AntiAliasingMode_MSAA_4X;
    if (standardMode.isEmpty()) {
        Int selected = -1; GadgetComboBoxGetSelectedPos(comboBoxAntiAliasing, &selected);
        if (selected > 0 && selected < OptionPreferences::AntiAliasingMode_Count) rememberedStandardChoice = selected;
    }
    if (!neuralToggleButton && comboBoxAntiAliasing) {
        GameWindow* prototype = TheWindowManager->winGetWindowFromId(nullptr,
            TheNameKeyGenerator->nameToKey("OptionsMenu.wnd:ButtonAccept"));
        if (!prototype) return;
        Int x, y, width, height;
        comboBoxAntiAliasing->winGetPosition(&x, &y); comboBoxAntiAliasing->winGetSize(&width, &height);
        // Retail Generals hides the AA row. Share the brightness row instead of
        // exposing that row underneath the overlapping control settings panel.
        GameWindow* gamma = TheWindowManager->winGetWindowFromId(nullptr,
            TheNameKeyGenerator->nameToKey("OptionsMenu.wnd:SliderGamma"));
        GameWindow* gammaLabel = TheWindowManager->winGetWindowFromId(nullptr,
            TheNameKeyGenerator->nameToKey("OptionsMenu.wnd:GammaLabel"));
        if (comboBoxAntiAliasing->winIsHidden() && gamma && gammaLabel) {
            Int gx, gy, gw, gh, lx, ly, lw, lh;
            gamma->winGetPosition(&gx, &gy); gamma->winGetSize(&gw, &gh);
            gammaLabel->winGetPosition(&lx, &ly); gammaLabel->winGetSize(&lw, &lh);
            const Int gammaWidth = gw / 3, spacing = MAX(2, gh / 5);
            gamma->winSetSize(gammaWidth, gh);
            TheWindowManager->winSendSystemMsg(gamma, GGM_RESIZED, gammaWidth, gh);
            gammaLabel->winSetSize(gammaWidth + gx - lx, lh);
            x = gx + gammaWidth + spacing; y = gy; width = gw - gammaWidth - spacing; height = gh;
            comboBoxAntiAliasing->winSetParent(gamma->winGetParent());
            comboBoxAntiAliasing->winSetPosition(x, y);
            comboBoxAntiAliasing->winHide(FALSE);
            WinInstanceData caption;
            caption.m_style = GWS_STATIC_TEXT;
            caption.m_id = TheNameKeyGenerator->nameToKey("OptionsMenu.wnd:NeuralAaLabel");
            const auto* labelLook = gammaLabel->winGetInstanceData();
            caption.m_enabledText = labelLook->m_enabledText;
            caption.m_disabledText = labelLook->m_disabledText;
            caption.m_hiliteText = labelLook->m_hiliteText;
            TextData textData = {};
            GameWindow* label = TheWindowManager->gogoGadgetStaticText(gamma->winGetParent(),
                WIN_STATUS_ENABLED, x, ly, width, lh, &caption, &textData, gammaLabel->winGetFont(), FALSE);
            if (label) GadgetStaticTextSetText(label, UnicodeString(L"\u0421\u0433\u043b\u0430\u0436\u0438\u0432\u0430\u043d\u0438\u0435"));
        }
        const Int gap = MAX(2, height / 5), buttonWidth = width / 4;
        // Popups must be siblings of the settings panels, rather than children
        // of VideoParent, whose entire subtree is drawn underneath those panels.
        GameWindow* overlayParent = prototype->winGetOwner();
        Int screenX, screenY, parentX, parentY;
        comboBoxAntiAliasing->winGetScreenPosition(&screenX, &screenY);
        overlayParent->winGetScreenPosition(&parentX, &parentY);
        comboBoxAntiAliasing->winSetParent(overlayParent);
        x = screenX - parentX; y = screenY - parentY;
        comboBoxAntiAliasing->winSetPosition(x, y);
        WinInstanceData instance;
        instance.m_style = GWS_PUSH_BUTTON | GWS_MOUSE_TRACK;
        instance.m_id = id;
        const auto* look = prototype->winGetInstanceData();
        for (Int i = 0; i < MAX_DRAW_DATA; ++i) {
            instance.m_enabledDrawData[i] = look->m_enabledDrawData[i];
            instance.m_disabledDrawData[i] = look->m_disabledDrawData[i];
            instance.m_hiliteDrawData[i] = look->m_hiliteDrawData[i];
        }
        instance.m_enabledText = look->m_enabledText; instance.m_disabledText = look->m_disabledText;
        instance.m_hiliteText = look->m_hiliteText;
        neuralToggleButton = TheWindowManager->gogoGadgetPushButton(comboBoxAntiAliasing->winGetParent(),
            WIN_STATUS_ENABLED | (prototype->winGetStatus() & WIN_STATUS_IMAGE),
            x + width - buttonWidth, y, buttonWidth, height, &instance, comboBoxAntiAliasing->winGetFont(), FALSE);
        if (!neuralToggleButton) return;
        WinInstanceData toggleCaption;
        toggleCaption.m_style = GWS_STATIC_TEXT;
        toggleCaption.m_id = TheNameKeyGenerator->nameToKey("OptionsMenu.wnd:NeuralToggleLabel");
        const auto* captionLook = gammaLabel ? gammaLabel->winGetInstanceData() : look;
        toggleCaption.m_enabledText = captionLook->m_enabledText;
        toggleCaption.m_disabledText = captionLook->m_disabledText;
        toggleCaption.m_hiliteText = captionLook->m_hiliteText;
        TextData toggleText = {}; toggleText.centered = TRUE;
        GameWindow* toggleLabel = TheWindowManager->gogoGadgetStaticText(overlayParent,
            WIN_STATUS_ENABLED, x + width - buttonWidth, y - height, buttonWidth, height,
            &toggleCaption, &toggleText, comboBoxAntiAliasing->winGetFont(), FALSE);
        if (toggleLabel) {
            GadgetStaticTextSetText(toggleLabel, UnicodeString(L"DLSS"));
            toggleLabel->winHide(comboBoxAntiAliasing->winIsHidden());
        }
        neuralToggleButton->winSetOwner(prototype->winGetOwner());
        neuralToggleButton->winHide(comboBoxAntiAliasing->winIsHidden());
        neuralToggleButton->winSetTooltip(UnicodeString(L"\u0412\u041a\u041b: DLSS/DLAA. \u0412\u042b\u041a\u041b: \u0441\u0442\u0430\u043d\u0434\u0430\u0440\u0442\u043d\u043e\u0435 \u0441\u0433\u043b\u0430\u0436\u0438\u0432\u0430\u043d\u0438\u0435 MSAA. \u041d\u0430\u0436\u043c\u0438\u0442\u0435 \u00ab\u041f\u0440\u0438\u043c\u0435\u043d\u0438\u0442\u044c\u00bb."));
        comboBoxAntiAliasing->winSetSize(width - buttonWidth - gap, height);
        TheWindowManager->winSendSystemMsg(comboBoxAntiAliasing, GGM_RESIZED, width - buttonWidth - gap, height);
    }
    comboBoxAntiAliasing->winBringToTop();
    GadgetComboBoxSetMaxDisplay(comboBoxAntiAliasing, OptionPreferences::AntiAliasingMode_Count + 2);
    updateNeuralToggle();
}
static void toggleNeuralSelection() {
    Int choice = -1; GadgetComboBoxGetSelectedPos(comboBoxAntiAliasing, &choice);
    if (isNeuralChoice(choice)) {
        rememberedNeuralChoice = choice; choice = rememberedStandardChoice;
    } else {
        choice = rememberedNeuralChoice;
        if (!TheDisplay->supportsNeuralAA(neuralModeForChoice(choice))) return;
    }
    lastAntiAliasingChoice = choice;
    GadgetComboBoxSetSelectedPos(comboBoxAntiAliasing, choice);
    updateNeuralToggle();
}
static Bool applyNeuralSelection(OptionPreferences& preferences) {
    Int choice = -1; GadgetComboBoxGetSelectedPos(comboBoxAntiAliasing, &choice);
    if (isNeuralChoice(choice)) {
        const auto mode = neuralModeForChoice(choice);
        if (!TheDisplay->supportsNeuralAA(mode) || !TheDisplay->setNeuralAAMode(mode)) {
            GadgetComboBoxSetSelectedPos(comboBoxAntiAliasing, lastAntiAliasingChoice);
            updateNeuralToggle(); return FALSE;
        }
        rememberedNeuralChoice = choice;
        TheWritableGlobalData->m_antiAliasLevel = WW3D::MULTISAMPLE_MODE_NONE;
        preferences["AntiAliasing"] = "0";
        preferences["NeuralAntiAliasing"] = mode == Display::NEURAL_AA_DLAA ? "DLAA" : "DLSSQuality";
    } else if (choice >= 0 && choice < OptionPreferences::AntiAliasingMode_Count &&
        TheDisplay->setNeuralAAMode(Display::NEURAL_AA_OFF)) {
        Int level = choice > 0 ? 1 << choice : 0;
        TheWritableGlobalData->m_antiAliasLevel = level;
        AsciiString value; value.format("%d", level);
        preferences["AntiAliasing"] = value; preferences["NeuralAntiAliasing"] = "Off";
    } else return FALSE;
    preferences["NeuralAntiAliasingLastMode"] =
        rememberedNeuralChoice == OptionPreferences::AntiAliasingMode_Count ? "DLSSQuality" : "DLAA";
    AsciiString standard; standard.format("%d", rememberedStandardChoice);
    preferences["NeuralAntiAliasingLastStandard"] = standard;
    lastAntiAliasingChoice = choice; updateNeuralToggle(); return TRUE;
}
