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
        HandleEvent(a_event);
        return _originalNPC(a_sink, a_event, a_eventSource);
    }

    AnimEventHook::EventResult AnimEventHook::ProcessEvent_PC(
        RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_sink, RE::BSAnimationGraphEvent* a_event,
        RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource) {
        HandleEvent(a_event);
        return _originalPC(a_sink, a_event, a_eventSource);
    }

    //interrupts vanilla conc casting on transition
    void AnimEventHook::HandleEvent(RE::BSAnimationGraphEvent* a_event) {
        if (!a_event || !a_event->holder || !a_event->tag.data()) {
            log::warn("HandleEvent called with no event");
            return;
        }
        auto* holder = const_cast<RE::TESObjectREFR*>(a_event->holder);
        auto* actor = holder ? holder->As<RE::Actor>() : nullptr;

        if (!actor) {
            log::warn("HandleEvent called with no actor");
            return;
        }
        
        //log::info("HandleEvent Called");
        /*MSCO::Magic::Hand firingHand{};
        const auto& tag = a_event->tag;
        
        if (!IsBeginCastEvent(tag, firingHand)) {
            return;
        }

        const bool castingMSCO = GetGraphBool(actor, "bCastingMSCO", false);
        if (!castingMSCO) {
            return;
        }
        log::info("bCastingMSCO Detected");
        const bool castingVanilla = GetGraphBool(actor, "bCastingVanilla", false);
        if (!castingVanilla) {
            return;
        }*/
        //log::info("Detected Fire and Forget Anim event");

        //const bool castingVanilla = GetGraphBool(actor, "bCastingVanilla", false);
        //if (!castingVanilla) {
        //    return;
        //}
        
        //const auto otherHand = OtherHand(firingHand);
       /* const auto otherHand =
            (firingHand == MSCO::Magic::Hand::Right) 
            ? MSCO::Magic::Hand::Left : MSCO::Magic::Hand::Right;
        const auto otherSource = MSCO::Magic::HandToSource(otherHand);*/
        /*if (auto* caster = actor->GetMagicCaster(otherSource)) {
            caster->InterruptCast(false);
        }*/
        const auto& tag = a_event->tag;
        if (tag == "EndLeftVanilla"sv) {
            auto* caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kLeftHand);
            caster->InterruptCast(false);
            log::info("Interrupted Left");
        }

        if (tag == "EndRightVanilla"sv) {
            auto* caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kRightHand);
            caster->InterruptCast(false);
            log::info("Interrupted Right");
        }
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
            log::info("InterruptedCast");
        }
        //do nothing if else.
    }

    //grab the input bool
    bool AnimEventHook::GetGraphBool(RE::Actor* actor, const char* name, bool defaultValue) {
        bool v = defaultValue;
        if (actor) {
            actor->GetGraphVariableBool(name, v);
            //actor->GetGraphVariableBool(name, v);
        }
        return v;
        /*bool value = defaultValue;
        if (!actor) {
            return defaultValue;
        }

        const bool ok = actor->GetGraphVariableBool(name, value);
        log::info("GraphBool '{}' ok={} value={}", name, ok, value);
        return ok;
        return ok ? value : defaultValue;*/
    }
}