#include "PCH.h"
#include "settings.h"
#include <SKSEMenuFramework.h>

using namespace SKSE;
using namespace SKSE::log;
using namespace SKSE::stl;

namespace settings {
    static config g_cfg{};

    config& Get() { return g_cfg; }

    void __stdcall RenderMenuPage() { 
        auto& c = Get();

        //bool changed = false;
        ImGuiMCP::TextUnformatted("MSCO Settings");
        ImGuiMCP::Separator();

        ImGuiMCP::Checkbox("Enable charge mechanic", &c.chargeMechanicOn);
        ImGuiMCP::Checkbox("Use exponential mode", &c.expMode);
        ImGuiMCP::Checkbox("Allow NPCs", &c.NPCAllowed);
        ImGuiMCP::Checkbox("Allow Player", &c.PlayerAllowed);
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