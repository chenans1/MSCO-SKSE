#pragma once
#include "SKSEMenuFramework.h"

using namespace SKSE;
using namespace SKSE::log;
using namespace std::literals;

namespace settings {
    struct config {
        float shortest = 0.0;
        float longest = 1.5;
        float basetime = 0.5;
        float minspeed = 0.5;
        float maxspeed = 0.5;
        float expFactor = 1.0;
        bool chargeMechanicOn = true;
        bool expMode = true;
        bool NPCAllowed = true;
        bool PlayerAllowed = true;
        float NPCFactor = 1.0;
    };

    void RegisterMenu();
    void __stdcall RenderMenuPage();
}