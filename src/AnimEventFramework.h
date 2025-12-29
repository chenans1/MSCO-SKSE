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
        static bool HandleEvent(RE::BSAnimationGraphEvent* a_event);
        
        static RE::MagicItem* GetEquippedMagicItemForHand(RE::Actor* actor, MSCO::Magic::Hand hand);
        static RE::MagicItem* GetCurrentlyCastingMagicItem(RE::Actor* actor, MSCO::Magic::Hand hand);
        static bool isMagicItemConcentration(RE::MagicItem* item);
       
        static bool IsMSCOEvent(const RE::BSFixedString& tag, MSCO::Magic::Hand& outHand);
        static bool IsBeginCastEvent(const RE::BSFixedString& tag, MSCO::Magic::Hand& outHand);
        static bool IsHandFireAndForget(RE::Actor* actor, MSCO::Magic::Hand hand);
        static void InterruptHand(RE::Actor* actor, MSCO::Magic::Hand hand);
        static bool GetGraphBool(RE::Actor* actor, const char* name, bool defaultValue = false);

        // store the original functions, so that we don't break other mods.
        static inline REL::Relocation<decltype(ProcessEvent_NPC)> _originalNPC;
        static inline REL::Relocation<decltype(ProcessEvent_NPC)> _originalPC;
    };
}