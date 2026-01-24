#include "PCH.h"
#include "settings.h"
#include <SKSEMenuFramework.h>
#include <SimpleIni.h>

using namespace SKSE;
using namespace SKSE::log;
using namespace SKSE::stl;

static bool ini_bool(CSimpleIniA& ini, const char* section, const char* key, bool def) {
    return ini.GetLongValue(section, key, def ? 1L : 0L) != 0;
}
static float ini_float(CSimpleIniA& ini, const char* section, const char* key, float def) {
    return static_cast<float>(ini.GetDoubleValue(section, key, def));
}

namespace settings {
    static config g_cfg{};

    config& Get() { return g_cfg; }
    
    void __stdcall RenderMenuPage() { 
        auto& c = Get();
        static bool s_dirty = false;
        //bool changed = false;
        s_dirty |= ImGuiMCP::Checkbox("Enable charge mechanic", &c.chargeMechanicOn);
        s_dirty |= ImGuiMCP::Checkbox("Use exponential mode", &c.expMode);
        s_dirty |= ImGuiMCP::Checkbox("NPCs use MSCO Animations", &c.NPCAllowed);
        s_dirty |= ImGuiMCP::Checkbox("Player uses MSCO Animations", &c.PlayerAllowed);
        s_dirty |= ImGuiMCP::Checkbox("Enable Log", &c.log);
        ImGuiMCP::Separator();
        s_dirty |= ImGuiMCP::DragFloat("Shortest", &c.shortest, 0.01f, 0.0f, 5.0f, "%.3f");
        s_dirty |= ImGuiMCP::DragFloat("Longest", &c.longest, 0.01f, 0.0f, 5.0f, "%.3f");
        s_dirty |= ImGuiMCP::DragFloat("Base time (Charge time = 1.0x speed)", &c.basetime, 0.01f, 0.0f, 5.0f, "%.3f");
        ImGuiMCP::Separator();
        s_dirty |= ImGuiMCP::DragFloat("Min speed", &c.minspeed, 0.01f, 0.01f, 3.0f, "%.3f");
        s_dirty |= ImGuiMCP::DragFloat("Max speed", &c.maxspeed, 0.01f, 0.01f, 3.0f, "%.3f");

        if (c.expMode) {
            s_dirty |= ImGuiMCP::DragFloat("Exponent factor", &c.expFactor, 0.01f, 0.1f, 1.0f, "%.3f");
        }
        if (c.NPCAllowed) {
            s_dirty |= ImGuiMCP::DragFloat("NPC Animation Speed Mult", &c.NPCFactor, 0.01f, 0.1f, 1.0f, "%.3f");
        }
        ImGuiMCP::Separator();
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

        ImGuiMCP::Separator();

        if (s_dirty) {
            ImGuiMCP::TextUnformatted("Unsaved changes");
        }

        if (ImGuiMCP::Button("Save")) {
            save();
            s_dirty = false;
        }
        ImGuiMCP::SameLine();
        if (ImGuiMCP::Button("Revert")) {
            load();
            s_dirty = false;
        }
    }

    static void validate(config& c) {
        c.shortest = std::max(0.0f, c.shortest);
        c.longest = std::max(c.shortest, c.longest);
        c.basetime = std::clamp(c.basetime, c.shortest, c.longest);

        // Speeds
        c.minspeed = std::max(0.01f, c.minspeed);
        c.maxspeed = std::max(c.minspeed, c.maxspeed);

        if (c.minspeed > c.maxspeed) {
            std::swap(c.minspeed, c.maxspeed);
        }

        // NPC factor
        c.NPCFactor = std::max(0.0f, c.NPCFactor);
    }

    void RegisterMenu() {
        if (!SKSEMenuFramework::IsInstalled()) {
            SKSE::log::warn("SKSE Menu Framework not installed; skipping menu registration.");
            return;
        }
        log::info("RegisterMenu Installed()");
        SKSEMenuFramework::SetSection("MSCO");
        SKSEMenuFramework::AddSectionItem("Settings", RenderMenuPage);
    }

    void load() { 
        //constexpr auto path = L"Data/SKSE/Plugins/MSCO.ini";
        constexpr auto path = "Data/SKSE/Plugins/MSCO.ini";
        CSimpleIniA ini;

        ini.SetUnicode(false); 
        const SI_Error rc = ini.LoadFile(path);

        if (rc < 0) {
            // File missing or unreadable. Keep defaults in g_cfg.
            log::warn("Could not load ini '{}'. Using defaults.", path);
            return;
        }
        auto& c = Get();

        //General
        c.chargeMechanicOn = ini_bool(ini, "General", "ChargeMechanicOn", c.chargeMechanicOn);
        c.expMode = ini_bool(ini, "General", "ExpMode", c.expMode);
        c.NPCAllowed = ini_bool(ini, "General", "NPCAllowed", c.NPCAllowed);
        c.PlayerAllowed = ini_bool(ini, "General", "PlayerAllowed", c.PlayerAllowed);
        c.log = ini_bool(ini, "General", "Log", c.log);
        c.NPCFactor = ini_float(ini, "General", "NPCFactor", c.NPCFactor);

        //charge time 
        c.shortest = ini_float(ini, "ChargeTime", "Shortest", c.shortest);
        c.longest = ini_float(ini, "ChargeTime", "Longest", c.longest);
        c.basetime = ini_float(ini, "ChargeTime", "BaseTime", c.basetime);

        //anim speed
        c.minspeed = ini_float(ini, "SpeedClamp", "MinSpeed", c.minspeed);
        c.maxspeed = ini_float(ini, "SpeedClamp", "MaxSpeed", c.maxspeed);

        //exp factor
        c.expFactor = ini_float(ini, "Exp", "ExpFactor", c.expFactor);

        validate(c);
        SKSE::log::info("Settings loaded: ChargeOn={}, Exp={}, NPC={}, Player={}", c.chargeMechanicOn, c.expMode,
                        c.NPCAllowed, c.PlayerAllowed);

        log::info("Loaded ini Settings");
    }

    void save() {
        constexpr const char* path = "Data/SKSE/Plugins/MSCO.ini";
        auto& c = Get();
        CSimpleIniA ini;
        ini.SetUnicode(false);

        //on fail create new file 
        (void)ini.LoadFile(path);

        validate(c);

        //general 
        ini.SetLongValue("General", "ChargeMechanicOn", c.chargeMechanicOn ? 1 : 0);
        ini.SetLongValue("General", "ExpMode", c.expMode ? 1 : 0);
        ini.SetLongValue("General", "NPCAllowed", c.NPCAllowed ? 1 : 0);
        ini.SetLongValue("General", "PlayerAllowed", c.PlayerAllowed ? 1 : 0);
        ini.SetLongValue("General", "Log", c.log ? 1 : 0);
        ini.SetDoubleValue("General", "NPCFactor", static_cast<double>(c.NPCFactor), "%.3f");

        //charge time 
        ini.SetDoubleValue("ChargeTime", "Shortest", static_cast<double>(c.shortest), "%.3f");
        ini.SetDoubleValue("ChargeTime", "Longest", static_cast<double>(c.longest), "%.3f");
        ini.SetDoubleValue("ChargeTime", "BaseTime", static_cast<double>(c.basetime), "%.3f");

        //anim speed
        ini.SetDoubleValue("SpeedClamp", "MinSpeed", static_cast<double>(c.minspeed), "%.3f");
        ini.SetDoubleValue("SpeedClamp", "MaxSpeed", static_cast<double>(c.maxspeed), "%.3f");

        //exp factor
        ini.SetDoubleValue("Exp", "ExpFactor", static_cast<double>(c.expFactor), "%.3f");
        
        const SI_Error rc = ini.SaveFile(path);
        if (rc < 0) {
            log::error("Failed to save ini '{}'. SI_Error={}", path, static_cast<int>(rc));
            return;
        }

        log::info("Saved ini '{}'", path);
    }
}