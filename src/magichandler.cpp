#include "PCH.h"
#include "magichandler.h"
#include "soundhandler.h"

using namespace RE;
using namespace SKSE;
using namespace SKSE::log;
using namespace std::literals;

namespace MSCO::Magic {
    //grab spell casting hand source by flag
    static RE::MagicSystem::CastingSource HandToSource(Hand hand) {
        switch (hand) {
            case Hand::Right:
                return RE::MagicSystem::CastingSource::kRightHand;
            case Hand::Left:
                return RE::MagicSystem::CastingSource::kLeftHand;
            default:
                log::warn("HandtoSource() default case switch");
                return RE::MagicSystem::CastingSource::kRightHand;
        }
    }
    static void FlashMagickaMeter() {
        using func_t = void(RE::ActorValue);
        REL::Relocation<func_t> func{RELOCATION_ID(51907, 52845)};
        func(RE::ActorValue::kMagicka);
    }
    RE::MagicItem* GetEquippedSpellHand(RE::Actor* actor, Hand hand) { 
        if (!actor) {
            log::warn("null actor passed to GetEquippedSpellForHand()");
            return nullptr;
        }
        auto& rd = actor->GetActorRuntimeData();
        std::size_t idx = static_cast<std::size_t>(hand);
        std::size_t other = idx ^ 1;

        RE::MagicItem* spell = nullptr;
        // try strict hand index first?
        if (rd.selectedSpells[idx]) {
            spell = rd.selectedSpells[idx];
            //log::info("[MSCO] Using {}-hand spell: {}", hand == Hand::Right ? "right" : "left", spell->GetFullName());
        } else if (rd.selectedSpells[other]) {
            //check other hand index
            spell = rd.selectedSpells[other];
            /*log::info("[MSCO] {} hand had no spell, using {} hand spell: {}", 
                hand == Hand::Right ? "right" : "left",
                hand == Hand::Right ? "left" : "right", spell->GetFullName());*/
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
        // fetch the eequipped hand spells
        const auto source = HandToSource(hand);

        auto caster = actor->GetMagicCaster(source);
        if (!caster) {
            log::warn("[MSCO] Actor {} has no MagicCaster for source {}", actor->GetName(), std::to_underlying(source));
            return false;
        }
        
        caster->SetDualCasting(dualCast);

        ////get the spell cost and check if the actor can actually cast spell
        float spellCost = spell->CalculateMagickaCost(actor);
        
        if (dualCast) {
            // Apply GSMT (fMagicDualCastingCostMult)
            constexpr auto dualCostGMST = "fMagicDualCastingCostMult";
            if (auto* settings = RE::GameSettingCollection::GetSingleton()) {
                if (auto* gmst = settings->GetSetting(dualCostGMST)) {
                    spellCost *= gmst->data.f;
                }
            }
        }
        if (spellCost < 0.0f) {spellCost = 0.0f;} //handle weird negative values
        
        RE::ActorValueOwner* actorAV = actor->AsActorValueOwner();
        if (!actorAV) {
            log::warn("no actor value");
            return false;
        }

        float curMagicka = actorAV->GetActorValue(RE::ActorValue::kMagicka);
        if (spellCost > 0.0f && curMagicka + 0.1f < spellCost) {
            //log::info("cannot cast has {} magicka < {} cost", curMagicka, spellCost);
            //need to implement some hud flashing stuff and play fail sound
            RE::HUDMenu::FlashMeter(RE::ActorValue::kMagicka);
            //FlashMagickaMeter();
            return false;
        }

        if (spellCost > 0.0f) {
           /* actorAV->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kMagicka, -spellCost);*/
            actorAV->DamageActorValue(RE::ActorValue::kMagicka, spellCost);
        }

        //self targeted spells should target the caster
        RE::Actor* target = nullptr;
        auto& rd = actor->GetActorRuntimeData();
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

        //only play release sound here. charge and ready should be handled somewhere else.
        //fetch sound first
        RE::BGSSoundDescriptorForm* releaseSound = MSCO::Sound::GetMGEFSound(spell);
        if (releaseSound) {
            // Fire and forget; your helper already handles nulls/engine quirks
            MSCO::Sound::play_sound(actor, releaseSound);
        }
        caster->CastSpellImmediate(spell,              // spell
                                   false,              // noHitEffectArt
                                   target,            // target
                                   effectiveness,      // effectiveness
                                   false,               // hostileEffectivenessOnly
                                   magnitudeOverride,  // magnitudeOverride
                                   actor               // cause (blame the caster so XP/aggro work)
        );
        //log::info("SUCESSFULLY CASTED");
        caster->SetDualCasting(false); //just in case
        RE::ConsoleLog::GetSingleton()->Print("Registered AnimationEventSink on player? {}", spell);
        return true;
    }

}