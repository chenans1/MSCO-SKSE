#pragma once

#include "PCH.h"
#include "magichandler.h"

namespace MSCO {
    class InputHandler : public RE::BSTEventSink<RE::InputEvent*> {
        public:
            using EventResult = RE::BSEventNotifyControl;

            static InputHandler& GetSingleton() { 
                static InputHandler singleton;
                return singleton;
            }

            EventResult ProcessEvent(
                RE::InputEvent* const* a_events, 
                RE::BSTEventSource<RE::InputEvent*>* a_eventSource) 
                override;

        private:
            InputHandler() = default;
            ~InputHandler() = default;
            InputHandler(const InputHandler&) = delete;
            InputHandler(InputHandler&&) = delete;
            InputHandler& operator=(const InputHandler&) = delete;
            InputHandler& operator=(InputHandler&&) = delete;

            //helpers
            bool IsValidPlayerState(RE::PlayerCharacter* player) const;
            //bool HasEnoughMagicka(RE::PlayerCharacter* player, MSCO::Magic::Hand hand, bool dualCast) const;
            void SendAnimEvent(RE::PlayerCharacter* player, std::string_view eventName) const;
    };
}