#pragma once

namespace settings {
    struct config {
        float shortest = 0.0;
        float longest = 2.0;
        float basetime = 0.15;
        float minspeed = 0.7;
        float maxspeed = 1.2;
        float expFactor = 0.17;
        bool chargeMechanicOn = true;
        bool expMode = true;
        bool NPCAllowed = true;
        bool PlayerAllowed = true;
        bool log = false;
        float NPCFactor = 1.0;
    };

    struct ChargeSpeedCFG {
        float shortest;
        float longest;
        float baseTime;
        float minSpeed;
        float maxSpeed;
        float expFactor;
        float npcFactor;
        bool expMode;
        bool enabled;
    };

    config& Get();
    void RegisterMenu();
    void __stdcall RenderMenuPage();
    void load();
    void save();

    //accessors
    bool IsConvertChargeTime(); 
    bool IsExpMode();
    bool IsNPCAllowed();
    bool IsPlayerAllowed();
    bool IsLogEnabled();
    float GetNPCFactor();

    ChargeSpeedCFG GetChargeSpeedCFG();
}