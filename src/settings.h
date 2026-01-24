#pragma once

namespace settings {
    struct config {
        float shortest = 0.0;
        float longest = 2.0;
        float basetime = 0.23;
        float minspeed = 0.6;
        float maxspeed = 1.25;
        float expFactor = 0.17;
        bool chargeMechanicOn = true;
        bool expMode = true;
        bool NPCAllowed = true;
        bool PlayerAllowed = true;
        float NPCFactor = 1.0;
    };

    config& Get();
    void RegisterMenu();
    void __stdcall RenderMenuPage();
}