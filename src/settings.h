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

    config& Get();
    void RegisterMenu();
    void __stdcall RenderMenuPage();
    void load();
    void save();
    bool IsLogEnabled();
}