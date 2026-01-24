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
        ImGuiMCP::Separator();
        ImGuiMCP::TextUnformatted("Charge time range (seconds)");
        ImGuiMCP::DragFloat("Shortest", &c.shortest, 0.01f, 0.0f, 5.0f, "%.3f");
        ImGuiMCP::DragFloat("Longest", &c.longest, 0.01f, 0.0f, 5.0f, "%.3f");
        ImGuiMCP::DragFloat("Base time (1.0x)", &c.basetime, 0.01f, 0.0f, 5.0f, "%.3f");

        ImGuiMCP::Separator();
        ImGuiMCP::TextUnformatted("Speed clamp");
        ImGuiMCP::DragFloat("Min speed", &c.minspeed, 0.01f, 0.01f, 5.0f, "%.3f");
        ImGuiMCP::DragFloat("Max speed", &c.maxspeed, 0.01f, 0.01f, 5.0f, "%.3f");

        if (c.expMode) {
            ImGuiMCP::Separator();
            ImGuiMCP::TextUnformatted("Exponential curve");
            ImGuiMCP::DragFloat("Exponent factor", &c.expFactor, 0.01f, 0.1f, 4.0f, "%.3f");
        }
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