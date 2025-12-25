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

    void AnimEventHook::HandleEvent(RE::BSAnimationGraphEvent* a_event) {
        if (!a_event || !a_event->holder || !a_event->tag.data()) {
            log::warn("HandleEvent called with no event");
            return;
        }
        auto* holder = const_cast<RE::TESObjectREFR*>(a_event->holder);
        auto* actor = holder ? holder->As<RE::Actor>() : nullptr;
        //auto* actor = a_event->holder->As<RE::Actor>();
        if (!actor) {
            log::warn("HandleEvent called with no actor");
            return;
        }

        // Debug: sample tags from player only
        if (actor->IsPlayerRef()) {
            log::info("[AnimHook] sample tag='{}'", a_event->tag.c_str());
        }
        //log::info("HandleEvent Called");
        //const std::string_view tag{a_event->tag.c_str()};
        MSCO::Magic::Hand firingHand{};
        const auto& tag = a_event->tag;
        //not one of the fire and forget animation events. 
        if (!IsFnFStartEvent(tag, firingHand)) {
            return;
        }
        log::info("Detected Fire and Forget Anim event");
        const auto otherHand =
            (firingHand == MSCO::Magic::Hand::Right) 
            ? MSCO::Magic::Hand::Left : MSCO::Magic::Hand::Right;

        auto* otherItem = MSCO::Magic::GetEquippedSpellHand(actor, otherHand);
        auto* otherSpell = otherItem ? otherItem->As<RE::SpellItem>() : nullptr;
        if (!otherSpell) {
            log::info("No otherSpell");
            return;
        }

        if (otherSpell->GetCastingType() != RE::MagicSystem::CastingType::kConcentration) {
            log::info("Otherhand is not concentration");
            return;
        }
        InterruptHand(actor, otherHand);

    }

    //sets the other firing hand and checks
    bool AnimEventHook::IsFnFStartEvent(const RE::BSFixedString& tag, MSCO::Magic::Hand& outFiringHand) {
        if (tag == "MRh_SpellAimedStart"sv || tag == "MRh_SpellSelfStart"sv) {
            outFiringHand = MSCO::Magic::Hand::Right;
            return true;
        }
        if (tag == "MLh_SpellAimedStart"sv || tag == "MLh_SpellSelfStart"sv) {
            outFiringHand = MSCO::Magic::Hand::Left;
            return true;
        }
        return false;
    }

    //interrupts the spell at specified hand source. 
    void AnimEventHook::InterruptHand(RE::Actor* actor, MSCO::Magic::Hand hand) { 
        const auto otherSource = MSCO::Magic::HandToSource(hand);
        if (auto* caster = actor->GetMagicCaster(otherSource); caster) {
            caster->InterruptCast(false);
            log::info("InterruptedCast");
        }
        //do nothing if else.
    }
}