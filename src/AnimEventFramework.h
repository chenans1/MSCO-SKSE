#pragma once
#include "PCH.h"
#include "magichandler.h"

namespace MSCO {
    class AnimEventHook {
    public: 
        static void Install();

     private:
        using EventResult = RE::BSEventNotifyControl;

        static EventResult ProcessEvent_NPC(
            RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_sink,
            RE::BSAnimationGraphEvent* a_event,
            RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource
        );

        static EventResult ProcessEvent_PC(
            RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_sink,
            RE::BSAnimationGraphEvent* a_event,
            RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource
        );

        //main logic function, detects FNF event, interrupts other hand if applicable.
        static void HandleEvent(RE::BSAnimationGraphEvent* a_event);

        static bool IsFnFStartEvent(std::string_view tag, MSCO::Magic::Hand& outFiringHand);
        static bool OtherHandIsConcentration(RE::Actor* actor, MSCO::Magic::Hand firingHand);
        static void InterruptOtherHand(RE::Actor* actor, MSCO::Magic::Hand firingHand);

        // store the original functions, so that we don't break other mods.
        static inline REL::Relocation<decltype(ProcessEvent_NPC)> _originalNPC;
        static inline REL::Relocation<decltype(ProcessEvent_NPC)> _originalPC;
    };
}