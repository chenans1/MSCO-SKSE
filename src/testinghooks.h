#pragma once
#include "RE/A/ActorMagicCaster.h"  // if you have it
#include "RE/M/MagicCaster.h"
#include "SKSE/SKSE.h"

namespace MSCO::Hooks {
    class CastSpellImmediateHook {
    public:
        static void Install() {
            {
                REL::Relocation<std::uintptr_t> vtbl{RE::VTABLE_MagicCaster[0]};
                _origMagicCaster = reinterpret_cast<fn_t>(vtbl.write_vfunc(0x1, &CastSpellImmediate_MagicCaster));
            }

            {
                REL::Relocation<std::uintptr_t> vtbl{RE::VTABLE_ActorMagicCaster[0]};
                _origActorMagicCaster =
                    reinterpret_cast<fn_t>(vtbl.write_vfunc(0x1, &CastSpellImmediate_ActorMagicCaster));
            }

            SKSE::log::info("[MSCO] CastSpellImmediateHook installed");
        };

    private:
        using fn_t = void (*)(RE::MagicCaster*, RE::MagicItem*, bool, RE::TESObjectREFR*, float, bool, float,
                              RE::Actor*);

        static void CastSpellImmediate_MagicCaster(RE::MagicCaster* a_this, RE::MagicItem* a_spell,
                                                   bool a_noHitEffectArt, RE::TESObjectREFR* a_target,
                                                   float a_effectiveness, bool a_hostileEffectivenessOnly,
                                                   float a_magnitudeOverride, RE::Actor* a_blameActor) {
            LogCall("MagicCaster", a_this, a_spell, a_noHitEffectArt, a_target, a_effectiveness,
                    a_hostileEffectivenessOnly, a_magnitudeOverride, a_blameActor);

            _origMagicCaster(a_this, a_spell, a_noHitEffectArt, a_target, a_effectiveness, a_hostileEffectivenessOnly,
                             a_magnitudeOverride, a_blameActor);
        }

        static void CastSpellImmediate_ActorMagicCaster(RE::MagicCaster* a_this, RE::MagicItem* a_spell,
                                                        bool a_noHitEffectArt, RE::TESObjectREFR* a_target,
                                                        float a_effectiveness, bool a_hostileEffectivenessOnly,
                                                        float a_magnitudeOverride, RE::Actor* a_blameActor) {
            LogCall("ActorMagicCaster", a_this, a_spell, a_noHitEffectArt, a_target, a_effectiveness,
                    a_hostileEffectivenessOnly, a_magnitudeOverride, a_blameActor);

            _origActorMagicCaster(a_this, a_spell, a_noHitEffectArt, a_target, a_effectiveness,
                                  a_hostileEffectivenessOnly, a_magnitudeOverride, a_blameActor);
        }

        static void LogCall(const char* which, RE::MagicCaster* a_this, RE::MagicItem* a_spell, bool a_noHitEffectArt,
                            RE::TESObjectREFR* a_target, float a_effectiveness, bool a_hostileEffectivenessOnly,
                            float a_magnitudeOverride, RE::Actor* a_blameActor) {
            auto* casterActor = a_this ? a_this->GetCasterAsActor() : nullptr;

            SKSE::log::info(
                "[MSCO] CastSpellImmediate({}) caster={} spell={}({:08X}) type={} castType={} source={} "
                "target={} eff={} hostileOnly={} magOverride={} blame={}",
                which, casterActor ? casterActor->GetDisplayFullName() : "<null>",
                a_spell ? a_spell->GetName() : "<null>", a_spell ? a_spell->formID : 0,
                a_spell ? static_cast<int>(a_spell->GetSpellType()) : -1,
                a_spell ? static_cast<int>(a_spell->GetCastingType()) : -1,
                a_this ? static_cast<int>(a_this->GetCastingSource()) : -1, a_target ? a_target->GetName() : "<null>",
                a_effectiveness, a_hostileEffectivenessOnly, a_magnitudeOverride,
                a_blameActor ? a_blameActor->GetDisplayFullName() : "<null>");
        }

        inline static fn_t _origMagicCaster{nullptr};
        inline static fn_t _origActorMagicCaster{nullptr};
    };
}
