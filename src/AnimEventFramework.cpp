#include "PCH.h"
#include "AnimEventFramework.h"

using namespace SKSE;
using namespace SKSE::log;
using namespace std::literals;

namespace MSCO {
    /*static const char* StateName(RE::MagicCaster::State s) {
        using S = RE::MagicCaster::State;
        switch (s) {
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
    }*/

    void logState(RE::Actor* actor, MSCO::Magic::Hand hand) {
        const auto source = MSCO::Magic::HandToSource(hand);
        auto* caster = actor->GetMagicCaster(source);
        if (!caster) return;
        const RE::MagicCaster::State current_state = caster->state.get();
        
        using S = RE::MagicCaster::State;

        switch (current_state) {
            case S::kNone:
                log::info("State: None");
                return;
            case S::kUnk01:
                log::info("State: Unk01");
                return;
            case S::kUnk02:
                log::info("State: Unk02");
                return;
            case S::kReady:
                log::info("State: Ready");
                return;
            case S::kUnk04:
                log::info("State: Unk04");
                return;
            case S::kCharging:
                log::info("State: Charging");
                return;
            case S::kCasting:
                log::info("State: Casting");
                return;
            case S::kUnk07:
                log::info("State: Unk07");
                return;
            case S::kUnk08:
                log::info("State: Interrupt");
                return;
            case S::kUnk09:
                log::info("State: Interrupt/Deselect");
                return;
            default:
                log::info("Default State: None");
                return;
        }

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
       /* if (tag == "CastingStateExit"sv) {
            InterruptHand(actor, MSCO::Magic::Hand::Right);
            InterruptHand(actor, MSCO::Magic::Hand::Left);
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

        if (tag == "MRh_SpellFire_Event"sv) {
            //logState(actor, MSCO::Magic::Hand::Right);
            bool isMSCO = GetGraphBool(actor, "bIsMSCO");
            if (!isMSCO) return false;
            /*auto* rightCaster = actor->GetMagicCaster(MSCO::Magic::HandToSource(MSCO::Magic::Hand::Left));
            if (rightCaster && rightCaster->GetIsDualCasting()) {
                log::info("Right Dual Casting");
            }*/
            MSCO::Magic::CastEquippedHand(actor, MSCO::Magic::Hand::Right, false);
            log::info("Intercepted MRh_SpellFire_Event.{}", payload);
            return true;
        }
        if (tag == "MLh_SpellFire_Event"sv) {
            //logState(actor, MSCO::Magic::Hand::Left);
            bool isMSCO = GetGraphBool(actor, "bIsMSCO");
            if (!isMSCO) return false;
            /*auto* leftCaster = actor->GetMagicCaster(MSCO::Magic::HandToSource(MSCO::Magic::Hand::Right));
            if (leftCaster && leftCaster->GetIsDualCasting()) {
                log::info("Left Dual Casting");
            }*/
            MSCO::Magic::CastEquippedHand(actor, MSCO::Magic::Hand::Left, false);
            log::info("Intercepted MLh_SpellFire_Event.{}", payload);
            return true;
        }
        //test: block NPC processEvent on begin cast if lock.
        MSCO::Magic::Hand beginHand{};
        if (IsBeginCastEvent(tag, beginHand)) {
            //logState(actor, beginHand);
            const char* lockName = (beginHand == MSCO::Magic::Hand::Right) ? "MSCO_right_lock" : "MSCO_left_lock";
            std::int32_t lock = 0;
            const bool ok = actor->GetGraphVariableInt(lockName, lock);

            if (ok && lock != 0) return true;  // swallow the BeginCastLeft/Right Event
            if (actor->IsSneaking()) return false; //check if we are sneaking nor not - don't do anything if we are

            RE::MagicItem* CurrentSpell = GetEquippedMagicItemForHand(actor, beginHand);
            if (!CurrentSpell) {log::warn("No CurrentSpell"); return false;}
            bool isfnf = (CurrentSpell->GetCastingType() == RE::MagicSystem::CastingType::kFireAndForget);
            //test if dual cast flag is set?
            const auto source = MSCO::Magic::HandToSource(beginHand);
            auto* caster = actor->GetMagicCaster(source);
            //turns out the game actually already sets the dual casting value correctly by the time we intercept begincastleft
            bool wantDualCasting = caster->GetIsDualCasting();
            //log::info("{}", (caster->GetIsDualCasting()) ? "is dual casting" : "not dual casting");
            
            if (isfnf) {                
                if (wantDualCasting) {
                    actor->NotifyAnimationGraph("MSCO_start_dual"sv);
                    if (auto ScriptEventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton()) {
                        if (auto RefHandle = actor->CreateRefHandle()) {
                            ScriptEventSourceHolder->SendSpellCastEvent(RefHandle.get(), CurrentSpell->formID);
                        }
                    }
                    return true;
                }
                //send script event for reliability? not sure if needed
                if (auto ScriptEventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton()) {
                    if (auto RefHandle = actor->CreateRefHandle()) {
                        ScriptEventSourceHolder->SendSpellCastEvent(RefHandle.get(), CurrentSpell->formID);
                    }
                }
                actor->NotifyAnimationGraph((beginHand == MSCO::Magic::Hand::Right) ? "MSCO_start_right"sv : "MSCO_start_left"sv);
                //logState(actor, beginHand);
                return true;
            }
            return false;
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