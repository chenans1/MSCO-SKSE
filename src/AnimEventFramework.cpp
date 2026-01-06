#include "PCH.h"
#include "AnimEventFramework.h"
#include "PayloadHandler.h"

using namespace SKSE;
using namespace SKSE::log;
using namespace std::literals;

namespace MSCO {
    static const char* SafeName(const RE::NiAVObject* obj) {
        if (!obj) return "<null>";
        // NiAVObject usually has BSFixedString name
        // In CommonLib: obj->name.c_str()
        const auto& nm = obj->name;
        const char* s = nm.c_str();
        return (s && s[0]) ? s : "<noname>";
    }

    static void LogNodeBasic(const RE::NiAVObject* obj, std::string_view label) {
        if (!obj) {
            log::info("[Node] {}: <null>", label);
            return;
        }

        // AsNode() returns NiNode* if it is a node (or nullptr)
        auto* node = const_cast<RE::NiAVObject*>(obj)->AsNode();

        log::info("[Node] {}: ptr={} name='{}' isNode={} rtti={}", label, fmt::ptr(obj), SafeName(obj), node != nullptr,
                  fmt::ptr(obj->GetRTTI()));  // optional: RTTI pointer
    }

    std::string_view logState(RE::Actor* actor, MSCO::Magic::Hand hand) {
        if (!actor) return "Invalid actor";

        const auto source = MSCO::Magic::HandToSource(hand);
        auto* caster = actor->GetMagicCaster(source);
        if (!caster) return "No MagicCaster";

        using S = RE::MagicCaster::State;
        const S state = caster->state.get();

        switch (state) {
            case S::kNone:
                return "None";
            case S::kUnk01:
                return "Unk01";
            case S::kUnk02:
                return "Unk02";
            case S::kReady:
                return "Ready";
            case S::kUnk04:
                return "Unk04";
            case S::kCharging:
                return "Charging";
            case S::kCasting:
                return "Casting";
            case S::kUnk07:
                return "Unk07";
            case S::kUnk08:
                return "Interrupt";
            case S::kUnk09:
                return "Interrupt/Deselect";
            default:
                return "Unknown";
        }
    }

    std::string_view logState2(RE::Actor* actor, RE::MagicSystem::CastingSource source) {
        if (!actor) return "Invalid actor";
        auto* caster = actor->GetMagicCaster(source);
        if (!caster) return "No MagicCaster";

        using S = RE::MagicCaster::State;
        const S state = caster->state.get();

        switch (state) {
            case S::kNone:
                return "None";
            case S::kUnk01:
                return "Unk01";
            case S::kUnk02:
                return "Unk02";
            case S::kReady:
                return "Ready";
            case S::kUnk04:
                return "Unk04";
            case S::kCharging:
                return "Charging";
            case S::kCasting:
                return "Casting";
            case S::kUnk07:
                return "Unk07";
            case S::kUnk08:
                return "Interrupt";
            case S::kUnk09:
                return "Interrupt/Deselect";
            default:
                return "Unknown";
        }
    }

    //handles magicka/staff charge consumption -> returns final effectiveness mult
    bool consumeResource(RE::MagicSystem::CastingSource source, RE::Actor* actor, RE::MagicItem* spell, bool dualCast, float costmult) {
        auto* enchItem = spell->As<RE::EnchantmentItem>();
        const bool isEnchantment = enchItem != nullptr;
        if (isEnchantment) { //assume it's a staff in this case
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
            //float spellCost = static_cast<float>(enchItem->data.costOverride);

            if (dualCast) {  //staves dont dual cast but I might want this feature someday
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
                RE::HUDMenu::FlashMeter(RE::ActorValue::kMagicka);
                return false;
            }
            if (spellCost > 0.0f) actorAV->DamageActorValue(RE::ActorValue::kMagicka, spellCost);
            return true;
        }
    }

    //uses castspell immediate, allows spell to fire out of any node, adjust damage, adjust cost.
    //also plays the release sound
    bool spellfire(RE::MagicSystem::CastingSource source, 
        RE::Actor * actor, RE::MagicItem* spell, bool dualCast, float costmult, float magmult) {
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
            log::warn("Spellfire() called on {} has no MagicCaster for source {}", actor->GetName(), std::to_underlying(source));
            return false;
        }
        auto& rd = actor->GetActorRuntimeData();
        const int slot = (source == RE::MagicSystem::CastingSource::kLeftHand) ? RE::Actor::SlotTypes::kLeftHand : RE::Actor::SlotTypes::kRightHand;
        auto* actorCaster = rd.magicCasters[slot];
        /*log::info("spellfire state before overwrite: {}", logState2(actor, source));
        caster->state.set(RE::MagicCaster::State::kUnk04);
        actorCaster->state.set(RE::MagicCaster::State::kUnk04);*/
        //set it again, but probably doesn't get unset
        //actorCaster->SetDualCasting(dualCast);
        actorCaster->SetDualCasting(dualCast);  // i dont think this is necessary for normal dualcasting
        bool validCast = consumeResource(source, actor, spell, dualCast, costmult);

        if (!validCast) {
            log::info("[spellfire] invalid cast - dont fire");
            return false;
        }

        RE::Actor* target = nullptr;
        if (spell->GetDelivery() == RE::MagicSystem::Delivery::kSelf) {
            target = actor; // self-targeted spells: target is the actor
        } else {
            // try combat target; if none, fall back to self
            target = rd.currentCombatTarget.get().get();
            if (!target) target = actor;
        }
        //send spell casting event
        if (auto ScriptEventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton()) {
            if (auto RefHandle = actor->CreateRefHandle()) {
                ScriptEventSourceHolder->SendSpellCastEvent(RefHandle.get(), spell->formID);
            }
        }
        //log::info("spellfire state: {}", logState2(actor, source));
        actorCaster->PrepareSound(RE::MagicSystem::SoundID::kRelease, spell);  // Doesn't seem to actually do anything, probably something to do with detection events?
        actorCaster->PlayReleaseSound(spell);  // idk what this does but maybe it sends a detection event? no clue

        RE::BGSSoundDescriptorForm* releaseSound = MSCO::Sound::GetMGEFSound(spell);
        if (releaseSound) MSCO::Sound::play_sound(actor, releaseSound);

        //apply damage buff i think
        float effectiveness = 1.0f;
        float magOverride = 0.0f;
        /*if (auto* effect = spell->GetCostliestEffectItem()) {
            magOverride = effect->GetMagnitude() * magmult;
        }
        log::info("spellfire event: source={}, magOverride = {}", (source == RE::MagicSystem::CastingSource::kLeftHand) ? "leftHand" : "rightHand", magOverride);*/

        //test node
        
        /*valid node names:
        NPC L MagicNode [LMag]
        NPC R MagicNode [RMag]*/
        if (auto root = actor->Get3D()) {
            static constexpr std::string_view NodeNames[2] = {"NPC L MagicNode [LMag]"sv, "NPC R MagicNode [RMag]"sv};
            auto bone = root->GetObjectByName(NodeNames[source == RE::MagicSystem::CastingSource::kLeftHand]);
            if (auto output_node = bone->AsNode()) {
                actorCaster->magicNode = output_node;
                log::info("spellfire: replaced node");
                auto* node = actorCaster->GetMagicNode();
                LogNodeBasic(node, "ActorCaster magicNode");
            }
        }
        
        //actorCaster->magicNode = RE::MagicSystem::CastingSource::kLeftHand;
        actorCaster->CastSpellImmediate(spell,    // spell
                                   false,              // noHitEffectArt
                                   target,             // target
                                   effectiveness,  // effectiveness
                                   false,  // hostileEffectivenessOnly
                                   magOverride,    // magnitudeOverride dunno what this does not sure
                                   actor               // cause (blame the caster so XP/aggro work)
        );
        caster->SetDualCasting(false);
        return true;
    }

    void AnimEventHook::Install() {
        log::info("[MSCO] Installing AnimEventFramework hook...");

        REL::Relocation<std::uintptr_t> vtblNPC{RE::VTABLE_Character[2]};
        REL::Relocation<std::uintptr_t> vtblPC{RE::VTABLE_PlayerCharacter[2]};

        _originalNPC = vtblNPC.write_vfunc(0x1, ProcessEvent_NPC);
        _originalPC = vtblPC.write_vfunc(0x1, ProcessEvent_PC);

        log::info("[MSCO] ...AnimEventFramework hook installed");
    }

    //always returning the original process function to not mess with other functionality.
    AnimEventHook::EventResult AnimEventHook::ProcessEvent_NPC(
        RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_sink,
        RE::BSAnimationGraphEvent* a_event,
        RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource
    ) {
        if (HandleEvent(a_event)) return RE::BSEventNotifyControl::kContinue;
        return _originalNPC(a_sink, a_event, a_eventSource);
    }

    AnimEventHook::EventResult AnimEventHook::ProcessEvent_PC(
        RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_sink, RE::BSAnimationGraphEvent* a_event,
        RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource) {
        if (HandleEvent(a_event)) return RE::BSEventNotifyControl::kContinue;
        return _originalPC(a_sink, a_event, a_eventSource);
    }

    //interrupts vanilla conc casting on transition
    bool AnimEventHook::HandleEvent(RE::BSAnimationGraphEvent* a_event) {
        if (!a_event || !a_event->holder || !a_event->tag.data()) return false;
        auto* holder = const_cast<RE::TESObjectREFR*>(a_event->holder);
        auto* actor = holder ? holder->As<RE::Actor>() : nullptr;
        if (!actor) return false;
        
        //log::info("HandleEvent Called");
        const auto& tag = a_event->tag;
        const auto& payload = a_event->payload;

        //if We see the "CastingStateExit" Event we interrupt/clear both hands
        if (tag == "CastingStateExit"sv) {
            InterruptHand(actor, MSCO::Magic::Hand::Right);
            InterruptHand(actor, MSCO::Magic::Hand::Left);
            return false;
        }

        /*if (tag == "MSCOExit"sv) {
            auto& rd = actor->GetActorRuntimeData();
            auto* actorCasterRight = rd.magicCasters[RE::Actor::SlotTypes::kRightHand];
            auto* actorCasterLeft = rd.magicCasters[RE::Actor::SlotTypes::kLeftHand];
            if (!actorCasterRight || !actorCasterLeft) return false;
            actorCasterRight->state.set(RE::MagicCaster::State::kNone);
            actorCasterLeft->state.set(RE::MagicCaster::State::kNone);
            return false;
        }*/

        //on MCO_WinOpen Allow for the casting?
        if (tag == "MCO_WinOpen"sv) {
            actor->NotifyAnimationGraph("CastOkStart"sv);
            return false;
        }

        if (tag == "MCO_PowerWinOpen"sv) {
            actor->NotifyAnimationGraph("CastOkStart"sv);
            return false;
        }

        //spellfire event handlers
        if (tag == "MRh_SpellFire_Event"sv) {
            if (!GetGraphBool(actor, "bIsMSCO")) return false;
            RE::MagicItem* CurrentSpell = GetEquippedMagicItemForHand(actor, MSCO::Magic::Hand::Right);
            if (!CurrentSpell) {
                log::warn("No CurrentSpell");
                return true;
            }
            //spell fire stuff now
            //log::info("Intercepted MRh_SpellFire_Event.{}", payload);

            const auto parsed = MSCO::ParseSpellFire(payload);
            auto source = parsed.src.value_or(RE::MagicSystem::CastingSource::kRightHand);
            auto costMult = parsed.costMult.value_or(1.0f);
            auto magMult = parsed.magMult.value_or(1.0f);
            log::info("MRh_SpellFire payload='{}' -> src={} cost={} mag={}", 
                payload, std::to_underlying(source), costMult, magMult);
            spellfire(source, actor, CurrentSpell, GetGraphBool(actor, "bMSCODualCasting"), costMult, magMult);
            return true;
        }

        if (tag == "MLh_SpellFire_Event"sv) {
            if (!GetGraphBool(actor, "bIsMSCO")) return false;
            RE::MagicItem* CurrentSpell = GetEquippedMagicItemForHand(actor, MSCO::Magic::Hand::Left);
            if (!CurrentSpell) {
                log::warn("No CurrentSpell");
                return true;
            }
            // spell fire stuff now
            //log::info("Intercepted MLh_SpellFire_Event.{}", payload);

            const auto parsed = ParseSpellFire(payload);
            auto source = parsed.src.value_or(RE::MagicSystem::CastingSource::kLeftHand);
            auto costMult = parsed.costMult.value_or(1.0f);
            auto magMult = parsed.magMult.value_or(1.0f);
            log::info("MLh_SpellFire payload='{}' -> src={} cost={} mag={}", payload,
                      std::to_underlying(source), costMult, magMult);
            //spellfire(RE::MagicSystem::CastingSource::kLeftHand, actor, CurrentSpell, GetGraphBool(actor, "bMSCODualCasting"), 1.0f, 1.0f);
            spellfire(source, actor, CurrentSpell, GetGraphBool(actor, "bMSCODualCasting"), costMult, magMult);
            return true;
        }

        //BeginCastLeft/Right handler. Handles button suppresion and MSCO inputs
        MSCO::Magic::Hand beginHand{};
        if (IsBeginCastEvent(tag, beginHand)) {
            const char* lockName = (beginHand == MSCO::Magic::Hand::Right) ? "MSCO_right_lock" : "MSCO_left_lock";
            std::int32_t lock = 0;
            const bool ok = actor->GetGraphVariableInt(lockName, lock);

            if (ok && lock != 0) return true;  // swallow the BeginCastLeft/Right Event
            if (actor->IsSneaking()) return false; //check if we are sneaking nor not - don't do anything if we are

            //damage magicka by some very very small amount here to pause regen for balance purposes
            /*RE::ActorValueOwner* actorAV = actor->AsActorValueOwner();
            if (!actorAV) {
                log::warn("[AnimEventFramework] No actor value");
            } else {
                actorAV->DamageActorValue(RE::ActorValue::kMagicka, 0.001f);
            }*/

            RE::MagicItem* CurrentSpell = GetEquippedMagicItemForHand(actor, beginHand);
            if (!CurrentSpell) {log::warn("No CurrentSpell"); return false;}
            //bool isfnf = (CurrentSpell->GetCastingType() == RE::MagicSystem::CastingType::kFireAndForget);
            if (CurrentSpell->GetCastingType() != RE::MagicSystem::CastingType::kFireAndForget) return false;

            auto& rd = actor->GetActorRuntimeData();
            const int slot = (beginHand == MSCO::Magic::Hand::Left) ? RE::Actor::SlotTypes::kLeftHand : RE::Actor::SlotTypes::kRightHand;
            auto* actorCaster = rd.magicCasters[slot];
            
            //test if dual cast flag is set?
            const auto source = MSCO::Magic::HandToSource(beginHand);
            auto* caster = actor->GetMagicCaster(source);
            //turns out the game actually already sets the dual casting value correctly by the time we intercept begincastleft
            bool wantDualCasting = actorCaster->GetIsDualCasting();
            
            if (wantDualCasting) {
                actor->NotifyAnimationGraph("MSCO_start_dual"sv);
                caster->state.set(RE::MagicCaster::State::kUnk04);
                actorCaster->state.set(RE::MagicCaster::State::kUnk04);
                
                //log::info("state at MSCO_start_dual: {}", logState(actor, beginHand));
                /*RE::BGSSoundDescriptorForm* beginSound = MSCO::Sound::GetMGEFSound(CurrentSpell, RE::MagicSystem::SoundID::kCharge);
                if (beginSound) MSCO::Sound::play_sound(actor, beginSound);*/
                return true;
            }

            actor->NotifyAnimationGraph((beginHand == MSCO::Magic::Hand::Right) ? "MSCO_start_right"sv: "MSCO_start_left"sv);
            caster->state.set(RE::MagicCaster::State::kUnk04);
            actorCaster->state.set(RE::MagicCaster::State::kUnk04);
            //log::info("state at MSCO_start_left/right: {}", logState(actor, beginHand));
            /*RE::BGSSoundDescriptorForm* beginSound = MSCO::Sound::GetMGEFSound(CurrentSpell, RE::MagicSystem::SoundID::kCharge);
            if (beginSound) MSCO::Sound::play_sound(actor, beginSound);*/
            return true;
        }

        //interrupt conc spell in other hand if applicable
        MSCO::Magic::Hand firingHand{};
        if (!IsMSCOEvent(tag, firingHand)) return false;

        //log::info("msco casting event detected");
        const auto otherHand =
            (firingHand == MSCO::Magic::Hand::Right) 
            ? MSCO::Magic::Hand::Left : MSCO::Magic::Hand::Right;

        RE::MagicItem* otherItem = nullptr;

        otherItem = GetCurrentlyCastingMagicItem(actor, otherHand);
        if (!otherItem) {
            //log::info("no other item for GetCurrentlyCastingMagicItem()");
            return false;
        }
        //log::info("detected magic item");
        if (isMagicItemConcentration(otherItem)) InterruptHand(actor, otherHand);

        return false;
    }

    //the mrh_spellaimedstart type specific events do not appear in the event sinks - they are notifys, do workaround instead:
    bool AnimEventHook::IsBeginCastEvent(const RE::BSFixedString& tag, MSCO::Magic::Hand& outHand) {
        if (tag == "BeginCastRight"sv) {
            outHand = MSCO::Magic::Hand::Right;
            return true;
        }
        if (tag == "BeginCastLeft"sv) {
            outHand = MSCO::Magic::Hand::Left;
            return true;
        }
        return false;
    }

    bool AnimEventHook::IsMSCOEvent(const RE::BSFixedString& tag, MSCO::Magic::Hand& outHand) {
        if (tag == "RightMSCOStart"sv) {
            outHand = MSCO::Magic::Hand::Right;
            return true;
        }
        if (tag == "LeftMSCOStart"sv) {
            outHand = MSCO::Magic::Hand::Left;
            return true;
        }
        return false;
    }
    //gets the equipped magic item for hand. should handle staves (no idea for scrolls?)
    RE::MagicItem* AnimEventHook::GetEquippedMagicItemForHand(RE::Actor* actor, MSCO::Magic::Hand hand) { 
        if (!actor) {
            return nullptr;
        }
        //first, check for spell. 
        if (auto* form = MSCO::Magic::GetEquippedSpellHand(actor, hand)) {
            if (auto* spell = form->As<RE::SpellItem>()) {
                return spell;  // SpellItem is a MagicItem
            }
        }

        //then check for staff:
        const bool left = (hand == MSCO::Magic::Hand::Left);
        auto* equippedForm = actor->GetEquippedObject(left);
        auto* weap = equippedForm ? equippedForm->As<RE::TESObjectWEAP>() : nullptr;
        if (!weap) {
            return nullptr;
        }
        RE::EnchantmentItem* ench = nullptr;
        ench = weap->formEnchanting;

        if (!ench) {
            return nullptr;
        }

        return ench;
    }

    //can check for the source, im assumign some of the edits come from other hand?
    RE::MagicItem* AnimEventHook::GetCurrentlyCastingMagicItem(RE::Actor* actor, MSCO::Magic::Hand hand) {
        if (!actor) {
            return nullptr;
        }
        const auto source = MSCO::Magic::HandToSource(hand);
        auto* caster = actor->GetMagicCaster(source);
        if (!caster) {
            return nullptr;
        }

        return caster->currentSpell;
    }
    
    //turns out you can just get the casting item type ig
    bool AnimEventHook::isMagicItemConcentration(RE::MagicItem* item) {
        if (!item) {
            return false;
        }
        //handle spells

        return item->GetCastingType() == RE::MagicSystem::CastingType::kConcentration;
        /*if (auto* spell = item->As<RE::SpellItem>()) {
            return spell->GetCastingType() == RE::MagicSystem::CastingType::kConcentration;
        }*/
        /*for (auto& effect : item->effects) {
            auto* mgef = effect ? effect->baseEffect : nullptr;
            if (!mgef) {
                continue;
            }
            if (mgef->GetCastingType() == RE::MagicSystem::CastingType::kConcentration) {
                return true;
            }

        }*/

        //return false;
    }
    //check for fire and forget spell type.
    bool AnimEventHook::IsHandFireAndForget(RE::Actor* actor, MSCO::Magic::Hand hand) {
        auto* item = MSCO::Magic::GetEquippedSpellHand(actor, hand);
        auto* spell = item ? item->As<RE::SpellItem>() : nullptr;
        if (!spell) {
            return false;
        }
        return spell->GetCastingType() == RE::MagicSystem::CastingType::kFireAndForget;
    }

    //interrupts the spell at specified hand source. 
    void AnimEventHook::InterruptHand(RE::Actor* actor, MSCO::Magic::Hand hand) { 
        const auto source = MSCO::Magic::HandToSource(hand);
        if (auto* caster = actor->GetMagicCaster(source); caster) {
            caster->InterruptCast(false);
            //log::info("InterruptedCast");
        }
        //do nothing if else.
    }

    //grab the input bool
    bool AnimEventHook::GetGraphBool(RE::Actor* actor, const char* name, bool defaultValue) {
        bool v = defaultValue;
        if (actor) {
            actor->GetGraphVariableBool(name, v);
        }
        return v;
    }
}