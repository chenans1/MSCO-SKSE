#include "PCH.h"
#include "magichandler.h"
#include "soundhandler.h"

using namespace RE;
using namespace SKSE;
using namespace SKSE::log;
using namespace std::literals;

namespace MSCO::Magic {
    //grab spell casting hand source by flag
    RE::MagicSystem::CastingSource HandToSource(Hand hand) {
        switch (hand) {
            case Hand::Right:
                return RE::MagicSystem::CastingSource::kRightHand;
            case Hand::Left:
                return RE::MagicSystem::CastingSource::kLeftHand;
            default:
                log::warn("[MagicHandler] HandtoSource() default case switch");
                return RE::MagicSystem::CastingSource::kRightHand;
        }
    }

    RE::MagicItem* GetEquippedSpellHand(RE::Actor* actor, Hand hand) { 
        if (!actor) {
            log::warn("[MagicHandler] null actor passed to GetEquippedSpellForHand()");
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
            log::info("[MagicHandler] No spells in selectedSpells[0/1]");
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
            log::warn("[MagicHandler] No spell equipped in {} hand", hand == Hand::Right ? "right" : "left");
            return false;
        }
        // fetch the eequipped hand spells
        const auto source = HandToSource(hand);

        auto caster = actor->GetMagicCaster(source);
        if (!caster) {
            log::warn("[MagicHandler] Actor {} has no MagicCaster for source {}", actor->GetName(), std::to_underlying(source));
            return false;
        }
        
        caster->SetDualCasting(dualCast);

        //get the spell cost and check if the actor can actually cast spell
        float spellCost = spell->CalculateMagickaCost(actor);
        
        if (dualCast) {
            spellCost = RE::MagicFormulas::CalcDualCastCost(spellCost);
        }
        if (spellCost < 0.0f) {spellCost = 0.0f;} //handle weird negative values
        
        RE::ActorValueOwner* actorAV = actor->AsActorValueOwner();
        if (!actorAV) {
            log::warn("[MagicHandler] No actor value");
            return false;
        }

        float curMagicka = actorAV->GetActorValue(RE::ActorValue::kMagicka);
        if (spellCost > 0.0f && curMagicka + 0.1f < spellCost) {
            //log::info("cannot cast has {} magicka < {} cost", curMagicka, spellCost);
            RE::HUDMenu::FlashMeter(RE::ActorValue::kMagicka);
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
        //fetch sound and play
        /*RE::BGSSoundDescriptorForm* releaseSound = MSCO::Sound::GetMGEFSound(spell);
        if (releaseSound) MSCO::Sound::play_sound(actor, releaseSound);*/
        const int slot =
            (hand == MSCO::Magic::Hand::Left) ? RE::Actor::SlotTypes::kLeftHand : RE::Actor::SlotTypes::kRightHand;

        auto* actorCaster = rd.magicCasters[slot];
        actorCaster->PrepareSound(RE::MagicSystem::SoundID::kRelease, spell); //Doesn't seem to actually do anything, probably something to do with detection events?
        actorCaster->PlayReleaseSound(spell);  // idk what this does but maybe it sends a detection event? no clue

        caster->CastSpellImmediate(spell,              // spell
                                   false,              // noHitEffectArt
                                   target,            // target
                                   effectiveness,      // effectiveness
                                   false,               // hostileEffectivenessOnly
                                   magnitudeOverride,  // magnitudeOverride
                                   actor               // cause (blame the caster so XP/aggro work)
        );
        
        //actorCaster->SpellCast(true, 1 ,spell);
        //log::info("SUCESSFULLY CASTED");
        caster->SetDualCasting(false); //just in case
        
        return true;
    }

    bool ConsumeResource(RE::MagicSystem::CastingSource source, RE::Actor* actor, RE::MagicItem* spell, bool dualCast, float costmult) {
        if (!actor || !spell) return false;
        auto* enchItem = spell->As<RE::EnchantmentItem>();
        const bool isEnchantment = enchItem != nullptr;
        if (isEnchantment) {  // assume it's a staff in this case
            const bool isLeftHand = source == RE::MagicSystem::CastingSource::kLeftHand;
            if (auto* equippedObj = actor->GetEquippedObject(isLeftHand)) {
                if (auto* weapon = equippedObj->As<RE::TESObjectWEAP>()) {
                    if (!weapon->IsStaff()) {
                        log::warn("[consumeResource] Not staff, ignore");
                        return false;
                    }
                } else {
                    log::warn("[consumeResource] No Weapon on Enchantment, ignore");
                    return false;
                }
            }
            float spellCost = enchItem->CalculateMagickaCost(actor);
            // float spellCost = static_cast<float>(enchItem->data.costOverride);

            if (dualCast) {  // staves dont dual cast but I might want this feature someday
                spellCost = RE::MagicFormulas::CalcDualCastCost(spellCost);
            }
            spellCost = spellCost * costmult;

            RE::ActorValueOwner* actorAV = actor->AsActorValueOwner();
            if (!actorAV) {
                log::warn("[consumeResource] No actor value");
                return false;
            }

            if (spellCost > 0.0f) {
                auto avToDamage = (isLeftHand) ? RE::ActorValue::kLeftItemCharge : RE::ActorValue::kRightItemCharge;
                actorAV->DamageActorValue(avToDamage, spellCost);
            }
            return true;

        } else {
            RE::ActorValueOwner* actorAV = actor->AsActorValueOwner();
            if (!actorAV) {
                log::warn("[consumeResource] No actor value");
                return false;
            }
            float spellCost = spell->CalculateMagickaCost(actor);

            if (dualCast) {
                spellCost = RE::MagicFormulas::CalcDualCastCost(spellCost);
            }

            spellCost = spellCost * costmult;  // apply costmult param
            if (spellCost < 0.0f) spellCost = 0.0f;
            float curMagicka = actorAV->GetActorValue(RE::ActorValue::kMagicka);
            if (spellCost > 0.0f && curMagicka < spellCost) {
                // log::info("cannot cast has {} magicka < {} cost", curMagicka, spellCost);
                //RE::HUDMenu::FlashMeter(RE::ActorValue::kMagicka);
                return false;
            }
            if (spellCost > 0.0f) actorAV->DamageActorValue(RE::ActorValue::kMagicka, spellCost);
            return true;
        }
    }

    bool Spellfire(RE::MagicSystem::CastingSource source, RE::Actor* actor, RE::MagicItem* spell, bool dualCast, float magmult, RE::MagicSystem::CastingSource outputSource) {
        if (!actor) {
            log::warn("No Actor for spellfire()");
            return false;
        }
        if (!spell) {
            log::warn("No Spell for spellfire()");
            return false;
        }
        auto caster = actor->GetMagicCaster(source);
        if (!caster) {
            log::warn("Spellfire() called on {} has no MagicCaster for source {}", actor->GetName(),
                      std::to_underlying(source));
            return false;
        }
        auto& rd = actor->GetActorRuntimeData();
        const int slot = (source == RE::MagicSystem::CastingSource::kLeftHand) ? RE::Actor::SlotTypes::kLeftHand
                                                                               : RE::Actor::SlotTypes::kRightHand;
        auto* actorCaster = rd.magicCasters[slot];

        actorCaster->SetDualCasting(dualCast);  // i dont think this is necessary for normal dualcasting
        /*bool validCast = consumeResource(source, actor, spell, dualCast, costmult);

        if (!validCast) {
            log::info("[spellfire] invalid cast - dont fire");
            return false;
        }*/

        RE::Actor* target = nullptr;
        if (spell->GetDelivery() == RE::MagicSystem::Delivery::kSelf) {
            target = actor;  // self-targeted spells: target is the actor
        } else {
            // try combat target; if none, fall back to self
            target = rd.currentCombatTarget.get().get();
            if (!target) target = actor;
        }
        // send spell casting event
        if (auto ScriptEventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton()) {
            if (auto RefHandle = actor->CreateRefHandle()) {
                ScriptEventSourceHolder->SendSpellCastEvent(RefHandle.get(), spell->formID);
            }
        }
        // log::info("spellfire state: {}", logState2(actor, source));
        actorCaster->PrepareSound(
            RE::MagicSystem::SoundID::kRelease,
            spell);  // Doesn't seem to actually do anything, probably something to do with detection events?
        actorCaster->PlayReleaseSound(spell);  // idk what this does but maybe it sends a detection event? no clue

        RE::BGSSoundDescriptorForm* releaseSound = MSCO::Sound::GetMGEFSound(spell);
        if (releaseSound) MSCO::Sound::play_sound(actor, releaseSound);

        // apply damage buff i think
        float effectiveness = 1.0f;
        float magOverride = 0.0f;
        /*if (auto* effect = spell->GetCostliestEffectItem()) {
            magOverride = effect->GetMagnitude() * magmult;
        }*/
        // log::info("spellfire event: source={}, magOverride = {}", (source ==
        // RE::MagicSystem::CastingSource::kLeftHand) ? "leftHand" : "rightHand", magOverride);

        // test node

        /*valid node names:
        NPC L MagicNode [LMag]
        NPC R MagicNode [RMag]*/
        //if (auto root = actor->Get3D()) {
        //    static constexpr std::string_view NodeNames[2] = {"NPC L MagicNode [LMag]"sv, "NPC R MagicNode [RMag]"sv};
        //    const auto& nodeName = (outputSource == RE::MagicSystem::CastingSource::kLeftHand) ? NodeNames[0]   // LMag
        //                                                                                       : NodeNames[1];  // RMag
        //    auto bone = root->GetObjectByName(nodeName);
        //    if (auto output_node = bone->AsNode()) {
        //        actorCaster->magicNode = output_node;
        //        /*log::info("spellfire: replaced node");
        //        auto* node = actorCaster->GetMagicNode();
        //        LogNodeBasic(node, "ActorCaster magicNode");*/
        //    }
        //}

        // actorCaster->magicNode = RE::MagicSystem::CastingSource::kLeftHand;
        //actorCaster->state.set(RE::MagicCaster::State::k);
        actorCaster->CastSpellImmediate(spell,   // spell
                                        false,   // noHitEffectArt
                                        target,  // target
                                        1.0f,    // effectiveness
                                        false,   // hostileEffectivenessOnly
                                        0.0f,    // magnitudeOverride dunno what this does not sure
                                        actor    // cause (blame the caster so XP/aggro work)
        );
        log::info("spellfire event: source={}", (source == RE::MagicSystem::CastingSource::kLeftHand) ? "leftHand" : "rightHand");
        actorCaster->FinishCast();
        // caster->SetDualCasting(false);
        return true;
    }

    using RequestCast = void (*)(RE::MagicCaster* caster);
    static inline RequestCast RequestCastImpl = nullptr;

    static void RequestCast_Hook(RE::MagicCaster* caster) {
        if (caster->state.get() >= RE::MagicCaster::State::kUnk02 && caster->state.get() <= RE::MagicCaster::State::kCharging) {
            log::info("denied requestcastimpl() function call");
            return;
        }
        if (RequestCastImpl) {
            //log::info("requestcastimpl() accepted");
            RequestCastImpl(caster);
        }
        
    }

    void Install() {
        REL::Relocation<std::uintptr_t> vtbl{RE::VTABLE_ActorMagicCaster[0]};
        // return original vfunc address?
        const std::uintptr_t orig = vtbl.write_vfunc(0x3, &RequestCast_Hook);
        // convert raw address -> function pointer
        RequestCastImpl = reinterpret_cast<RequestCast>(orig);
    }
}