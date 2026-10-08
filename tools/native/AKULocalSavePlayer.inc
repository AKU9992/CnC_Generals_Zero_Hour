// AKU9992: slot IPs are not serialized; restore the single local human without changing save format.
    if(xfer->getXferMode()==XFER_LOAD && m_inGame && getLocalSlotNum()<0){
        GameSlot* human=nullptr;int humans=0;
        for(int i=0;i<MAX_SLOTS;++i){if(m_slot[i] && m_slot[i]->getState()==SLOT_PLAYER){human=m_slot[i];++humans;}}
        if(humans==1)human->setIP(m_localIP);
    }
