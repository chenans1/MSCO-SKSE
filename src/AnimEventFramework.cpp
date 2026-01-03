#include "PCH.h"
#include "AnimEventFramework.h"

using namespace SKSE;
using namespace SKSE::log;
using namespace std::literals;

namespace MSCO {
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
        
        if (HandleEvent(a_event)) {
            //log::info("HandleEvent Blocked ProcessEvent()");
            return RE::BSEventNotifyControl::kContinue;
        }
        //HandleEvent(a_event);
        return _originalNPC(a_sink, a_event, a_eventSource);
    }

    AnimEventHook::EventResult AnimEventHook::ProcessEvent_PC(
        RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_sink, RE::BSAnimationGraphEvent* a_event,
        RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource) {
        //HandleEvent(a_event);
        if (HandleEvent(a_event)) {
            return RE::BSEventNotifyControl::kContinue;
        }
        return _originalPC(a_sink, a_event, a_eventSource);
    }

    //interrupts vanilla conc casting on transition
    bool AnimEventHook::HandleEvent(RE::BSAnimationGraphEvent* a_event) {
        if (!a_event || !a_event->holder || !a_event->tag.data()) {
            //log::warn("HandleEvent called with no event");
            return false;
        }
        auto* holder = const_cast<RE::TESObjectREFR*>(a_event->holder);
        auto* actor = holder ? holder->As<RE::Actor>() : nullptr;

        if (!actor) {
            //log::warn("HandleEvent called with no actor");
            return false;
        }
        
        //log::info("HandleEvent Called");
        const auto& tag = a_event->tag;
        const auto& payload = a_event->payload;

        //if We see the "CastingStateExit" Event we interrupt/clear both hands
        if (tag == "CastingStateExit"sv) {
            InterruptHand(actor, MSCO::Magic::Hand::Right);
            InterruptHand(actor, MSCO::Magic::Hand::Left);
            return false;
        }

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
            bool isMSCO = GetGraphBool(actor, "bIsMSCO");
            if (!isMSCO) {
                return false;
            }
            MSCO::Magic::CastEquippedHand(actor, MSCO::Magic::Hand::Right, false);
            log::info("Intercepted MRh_SpellFire_Event.{}", payload);
            return true;
        }
        if (tag == "MLh_SpellFire_Event"sv) {
            bool isMSCO = GetGraphBool(actor, "bIsMSCO");
            if (!isMSCO) {
                return false;
            }
            MSCO::Magic::CastEquippedHand(actor, MSCO::Magic::Hand::Left, false);
            log::info("Intercepted MLh_SpellFire_Event.{}", payload);
            return true;
        }
        //test: block NPC processEvent on begin cast if lock.
        MSCO::Magic::Hand beginHand{};
        if (IsBeginCastEvent(tag, beginHand)) {
            const char* lockName = (beginHand == MSCO::Magic::Hand::Right) ? "MSCO_right_lock" : "MSCO_left_lock";
            std::int32_t lock = 0;
            const bool ok = actor->GetGraphVariableInt(lockName, lock);

            if (ok && lock != 0) {
                return true;  //swallow the BeginCastLeft/Right Event
            }

            //check if we are sneaking nor not - don't do anything if we are
            if (actor->IsSneaking()) {
                return false;
            }

            RE::MagicItem* CurrentSpell = GetEquippedMagicItemForHand(actor, beginHand);
            if (!CurrentSpell) {
                log::warn("No CurrentSpell");
                return false;
            }
            bool isfnf = (CurrentSpell->GetCastingType() == RE::MagicSystem::CastingType::kFireAndForget);

            //send anim event depending on the thing
            if (isfnf) {
                //dual casting always occurs on begincastleft
                //if (beginHand == MSCO::Magic::Hand::Left) {
                //    //check if we are dual casting perhaps?
                //    const auto source = MSCO::Magic::HandToSource(MSCO::Magic::Hand::Left);
                //    auto* caster = actor->GetMagicCaster(source);

                //    RE::MagicSystem::CannotCastReason reason = RE::MagicSystem::CannotCastReason::kOK;
                //    float effect_strength = 1.0f;

                //    bool CanDualCast = caster->CheckCast(CurrentSpell, true, &effect_strength, &reason, false);
                //    //bool wantCastRight = GetGraphBool(actor, "bWantCastRight");

                //    if (wantCastRight && CanDualCast) {
                //        log::info("Dual Casting Detected");
                //        actor->NotifyAnimationGraph("MSCO_start_dual"sv);
                //        //actor->NotifyAnimationGraph("MLh_SpellReady_Event"sv);
                //        return true;
                //    }
                //}
                if (beginHand == MSCO::Magic::Hand::Left) {
                    //check if the spell we are looking at can be dual-casted or not
                    if (!CurrentSpell->GetNoDualCastModifications()) {
                        log::info("can dual cast the left hand spell");
                    }
                }
                
                actor->NotifyAnimationGraph((beginHand == MSCO::Magic::Hand::Right) ? "MSCO_start_right"sv : "MSCO_start_left"sv);
                //actor->NotifyAnimationGraph((beginHand == MSCO::Magic::Hand::Right) ? "MLh_SpellReady_Event"sv : "MRh_SpellReady_Event"sv);
                return true;
            }
            
            return false;
        }



        //interrupt conc spell in other hand if applicable
        MSCO::Magic::Hand firingHand{};
        if (!IsMSCOEvent(tag, firingHand)) {
            return false;
        }
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

        if (isMagicItemConcentration(otherItem)) {
            InterruptHand(actor, otherHand);
            //log::info("Interrupted Concentration Casting");
        }

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
    //gets the equipped magic item for hand
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