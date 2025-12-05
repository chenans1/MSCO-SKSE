#include "PCH.h"
#include "magichandler.h"

using namespace SKSE;
using namespace SKSE::log;
using namespace std::literals;

namespace MSCO::Magic {

    //grab spell casting hand item by flag
    static RE::MagicSystem::CastingSource HandToSource(Hand hand) {
        switch (hand) {
            case Hand::Left:
                return RE::MagicSystem::CastingSource::kLeftHand;
            case Hand::Right:
            default:
                return RE::MagicSystem::CastingSource::kRightHand;
        }
    }

    RE::MagicItem* GetEquippedSpellHand(RE::Actor* actor, Hand hand) { 
        if (!actor) {
            log::info("WARNING: NO ACTOR PASSED TO GetEquippedSpellForHand()");
            return nullptr;
        }
        auto& rd = actor->GetActorRuntimeData();

        std::size_t idx = (hand == Hand::Right) ? 0 : 1;

        auto spell = rd.selectedSpells[idx];
        if (!spell) {
            log::info("NO SPELLS FROM rd.selectedSpells[idx]");
            return nullptr;
        }
        return spell->As<RE::MagicItem>();
    }

    bool CastEquippedHand(RE::Actor* actor, Hand hand, bool dualCast) {
        if (!actor) {
            log::info("No Actor for CastEquippedHand()");
            return false;
        }
        
        auto* spell = GetEquippedSpellHand(actor, hand);
        if (!spell) {
            log::info("[MSCO] No spell equipped in {} hand", hand == Hand::Right ? "right" : "left");
            return false;
        }

        const auto source = HandToSource(hand);

        auto* caster = actor->GetMagicCaster(source);
        if (!caster) {
            log::info("[MSCO] Actor {} has no MagicCaster for source {}", actor->GetName(),
                         std::to_underlying(source));
            return false;
        }

        caster->SetDualCasting(dualCast);

        auto& rd = actor->GetActorRuntimeData();

        //self targeted spells should target the caster
        RE::Actor* target = nullptr;

        if (spell->GetDelivery() == RE::MagicSystem::Delivery::kSelf) {
            // self-targeted spells: target is the actor
            target = actor;
        } else {
            // try combat target; if none, fall back to self
            target = rd.currentCombatTarget.get().get();
            if (!target) {
                target = actor;
            }
        }

        float effectiveness = 1.0f;
        float magnitudeOverride = 0.0f;

        //log::info("Casting {} on {} with (dualcasting = {})", spell->GetFullNameLength(), target, dualCast);
        caster->CastSpellImmediate(spell,              // spell
                                   false,              // noHitEffectArt
                                   target,            // target
                                   effectiveness,      // effectiveness
                                   false,               // hostileEffectivenessOnly
                                   magnitudeOverride,  // magnitudeOverride
                                   actor               // cause (blame the caster so XP/aggro work)
        );
        log::info("SUCESSFULLY CASTED");
        return true;
    }

}