#include "settings.h"
#include "PCH.h"

using namespace SKSE;
using namespace SKSE::log;
using namespace SKSE::stl;
using namespace std::literals;

namespace settings {
    static config g_cfg{};

    config& Get() { return g_cfg; }

    void __stdcall RenderMenuPage() { 
        
    }

    void RegisterMenu() {
        if (!SKSEMenuFramework::IsInstalled()) {
            SKSE::log::warn("[MSCO] SKSE Menu Framework not installed; skipping menu registration.");
            return;
        }
        log::info("RegisterMenu Installed()");
        SKSEMenuFramework::SetSection("MSCO");
        SKSEMenuFramework::AddSectionItem("Settings", RenderMenuPage);
    }
}