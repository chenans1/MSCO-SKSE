#pragma once

namespace MSCO {
    class SpellCastEventHandler : public RE::BSTEventSink<RE::TESSpellCastEvent> {
    public:
        static SpellCastEventHandler* GetSingleton() {
            static SpellCastEventHandler singleton;
            return &singleton;
        }

        //process spell casting events here
        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESSpellCastEvent* a_event,
            RE::BSTEventSource<RE::TESSpellCastEvent>* a_eventSource) override;

        /*void RegisterSpellCastHandler() {
            auto* eventSource = RE::ScriptEventSourceHolder::GetSingleton();
            if (eventSource) {
                eventSource->AddEventSink(SpellCastEventHandler::GetSingleton());
                log::info("Registered SpellCastEventHandler");
            }
        }*/
    };
}