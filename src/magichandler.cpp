#include "PCH.h"
#include "magichandler.h"

using namespace SKSE;
using namespace SKSE::log;
using namespace std::literals;

namespace MSCO::Magic {

    //grab spell casting hand item by flag
    static RE::MagicSystem::CastingSource HandToSource(Hand hand) {
        //the mapping is inverted for hand nodes for whatever reason???
        switch (hand) {
            case Hand::Right:
                //return RE::MagicSystem::CastingSource::kLeftHand;
                return RE::MagicSystem::CastingSource::kRightHand;
            case Hand::Left:
                return RE::MagicSystem::CastingSource::kLeftHand;
                //return RE::MagicSystem::CastingSource::kRightHand;
            default:
                log::warn("HandtoSource() default case switch");
                return RE::MagicSystem::CastingSource::kRightHand;
        }
    }

    RE::MagicItem* GetEquippedSpellHand(RE::Actor* actor, Hand hand) { 
        if (!actor) {
            log::warn("null actor passed to GetEquippedSpellForHand()");
            return nullptr;
        }
        auto& rd = actor->GetActorRuntimeData();
        //auto idx = static_cast<std::size_t>(hand);
        // std::size_t idx = (hand == Hand::Right) ? 0 : 1;
        /*auto spell = rd.selectedSpells[idx];*/
        /*if (!spell) {
            log::warn("NO SPELLS FROM rd.selectedSpells[idx]");
            return nullptr;
        }*/
        std::size_t idx = static_cast<std::size_t>(hand);
        std::size_t other = idx ^ 1;

        RE::MagicItem* spell = nullptr;
        // try strict hand index first?
        if (rd.selectedSpells[idx]) {
            spell = rd.selectedSpells[idx];
            log::info("[MSCO] Using {}-hand spell: {}", hand == Hand::Right ? "right" : "left", spell->GetFullName());
        } else if (rd.selectedSpells[other]) {
            //check other hand index
            spell = rd.selectedSpells[other];
            log::info("[MSCO] {} hand had no spell, using {} hand spell: {}", 
                hand == Hand::Right ? "right" : "left",
                hand == Hand::Right ? "left" : "right", spell->GetFullName());
        } else {
            log::info("[MSCO] No spells in selectedSpells[0/1]");
            return nullptr;
        }
        return spell->As<RE::MagicItem>();
    }

    bool CastEquippedHand(RE::Actor* actor, Hand hand, bool dualCast) {
        if (!actor) {
            log::warn("No Actor for CastEquippedHand()");
            return false;
        }
        
        auto* spell = GetEquippedSpellHand(actor, hand);

        if (!spell) {
            log::warn("[MSCO] No spell equipped in {} hand", hand == Hand::Right ? "right" : "left");
            return false;
        }

        const auto source = HandToSource(hand);

        auto* caster = actor->GetMagicCaster(source);
        if (!caster) {
            log::info("[MSCO] Actor {} has no MagicCaster for source {}", actor->GetName(),
                         std::to_underlying(source));
            return false;
        }

        auto& rd = actor->GetActorRuntimeData();
        log::info("[MSCO] CastEquippedHand: actor={}, hand={}, sel[0]={}, sel[1]={}", actor->GetName(),
                  hand == Hand::Right ? "Right" : "Left",
                  rd.selectedSpells[0] ? rd.selectedSpells[0]->GetFullName() : "<none>",
                  rd.selectedSpells[1] ? rd.selectedSpells[1]->GetFullName() : "<none>");

        caster->SetDualCasting(dualCast);

        //auto& rd = actor->GetActorRuntimeData();

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

        //log::info("Casting {} on {} with (dualcasting = {})", spell->fullName, target, dualCast);
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