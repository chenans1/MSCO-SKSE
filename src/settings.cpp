#include "PCH.h"
#include "settings.h"
#include <SKSEMenuFramework.h>
#include <SimpleIni.h>

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
        ImGuiMCP::Checkbox("NPCs use MSCO Animations", &c.NPCAllowed);
        ImGuiMCP::Checkbox("Player uses MSCO Animations", &c.PlayerAllowed);
        ImGuiMCP::Separator();
        ImGuiMCP::TextUnformatted("Charge time range (seconds)");
        ImGuiMCP::DragFloat("Shortest", &c.shortest, 0.01f, 0.0f, 5.0f, "%.3f");
        ImGuiMCP::DragFloat("Longest", &c.longest, 0.01f, 0.0f, 5.0f, "%.3f");
        ImGuiMCP::DragFloat("Base time (Charge time = 1.0x speed)", &c.basetime, 0.01f, 0.0f, 5.0f, "%.3f");

        ImGuiMCP::Separator();
        ImGuiMCP::TextUnformatted("Speed clamp");
        ImGuiMCP::DragFloat("Min speed", &c.minspeed, 0.01f, 0.01f, 3.0f, "%.3f");
        ImGuiMCP::DragFloat("Max speed", &c.maxspeed, 0.01f, 0.01f, 3.0f, "%.3f");

        if (c.expMode) {
            ImGuiMCP::Separator();
            ImGuiMCP::TextUnformatted("Exponential curve");
            ImGuiMCP::DragFloat("Exponent factor", &c.expFactor, 0.01f, 0.1f, 1.0f, "%.3f");
        }
        if (c.NPCAllowed) {
            ImGuiMCP::Separator();
            ImGuiMCP::TextUnformatted("NPC Additional Multiplier");
            ImGuiMCP::DragFloat("NPC Animation Speed Mult", &c.NPCFactor, 0.01f, 0.1f, 1.0f, "%.3f");
        }

        const float tFast = c.shortest;
        const float tBase = c.basetime;
        const float tSlow = c.longest;
        const float tvanilla = 0.5f;
        auto previewSpeed = [&](float t) {
            if (c.expMode) {
                float s = std::pow(c.basetime / std::max(t, 0.001f), c.expFactor);
                return std::clamp(s, c.minspeed, c.maxspeed);
            } else {
                // linear mapping similar to your getSpeed()
                if (std::abs(t - c.basetime) < 1e-6f) return 1.0f;
                if (t < c.basetime) {
                    float denom = (c.basetime - c.shortest);
                    if (denom <= 1e-6f) return 1.0f;
                    float u = (c.basetime - t) / denom;
                    return 1.0f + u * (c.maxspeed - 1.0f);
                } else {
                    float denom = (c.longest - c.basetime);
                    if (denom <= 1e-6f) return 1.0f;
                    float v = (t - c.basetime) / denom;
                    return 1.0f - v * (1.0f - c.minspeed);
                }
            }
        };

        ImGuiMCP::Text("Shortest %.3f -> %.3fx", tFast, previewSpeed(tFast));
        ImGuiMCP::Text("Base     %.3f -> %.3fx", tBase, previewSpeed(tBase));
        ImGuiMCP::Text("Vanilla %.3f -> %.3fx", tvanilla, previewSpeed(tvanilla));
        ImGuiMCP::Text("Longest  %.3f -> %.3fx", tSlow, previewSpeed(tSlow));
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

    void load() { 
        CSimpleIniA ini;
    }
}