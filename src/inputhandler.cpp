#include "PCH.h"
#include "inputhandler.h"
#include "magichandler.h"

using namespace SKSE;
using namespace SKSE::log;
using namespace std::literals;

namespace MSCO {
	using MSCO::Magic::Hand;

	bool InputHandler::IsValidPlayerState(RE::PlayerCharacter* player) const {
        if (!player) {
            log::warn("Player not found?");
            return false;
        }

		if (player->IsSneaking()) {return false;}

        if (player->IsOnMount()) {return false;}

        bool inJump = false;
        if (player->GetGraphVariableBool("bInJumpState", inJump) && inJump) {return false;}
        auto* actorState = player->AsActorState();
        //no casting if we do not have drawn weapons. Technically, the behavior patch prevents this anyways
        if (!actorState || !actorState->IsWeaponDrawn()) {
            return false;
        }
        return true;
	}

    //send the cast anim event to the behavior graph
    void InputHandler::SendAnimEvent(RE::PlayerCharacter* player, std::string_view eventName) const {
        if (!player) {
            log::warn("Player not found?");
            return;
        }
        RE::BSFixedString ev(eventName.data());
        player->NotifyAnimationGraph(ev);
        log::info("MSCO: sent anim event {}", eventName);
    }

    InputHandler::EventResult InputHandler::ProcessEvent(
        RE::InputEvent* const* a_events,
        RE::BSTEventSource<RE::InputEvent*>*) 
    {
        if (!a_events) {return RE::BSEventNotifyControl::kContinue;}
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {return RE::BSEventNotifyControl::kContinue;}
        //no sneak jump + has to have weapons out.
        if (!IsValidPlayerState(player)) {
            return RE::BSEventNotifyControl::kContinue;
        }
        bool leftPressed = false;
        bool rightPressed = false;

        for (auto* event = *a_events; event; event = event->next) {

            //if (event->eventType == RE::INPUT_EVENT_TYPE::kButton) {
            //    auto* button = event->AsButtonEvent();
            //    if (button) {
            //        //std::string mapped = button->QUserEvent();
            //        auto state = button->IsDown() ? "Down" : button->HeldDuration() > 0.f ? "Held" : "Up";
            //        auto userEvent = button->QUserEvent();   // BSFixedString
            //        const char* mapped = userEvent.c_str();  // Safe

            //        RE::ConsoleLog::GetSingleton()->Print("[MSCO-INPUT] Device=%d  Code=%d  State=%s  Mapped='%s'",
            //                                              static_cast<int>(button->device.get()),
            //                                              static_cast<int>(button->idCode), state,
            //                                              (mapped && mapped[0] != '\0') ? mapped : "<none>");
            //    }
            //}

            if (event->eventType != RE::INPUT_EVENT_TYPE::kButton) {
                continue;
            }

            auto* button = event->AsButtonEvent();
            if (!button) {
                continue;
            }

            if (!button->IsDown()) {
                continue;
            }

            //map by user-event name
            const auto& userEvent = button->QUserEvent();
            if (userEvent.empty()) {
                continue;
            }

            std::string_view evName{userEvent};
            
            //do not fucking use a mod that renames these in the action records
            if (evName == "Left Attack/Block") {
                leftPressed = true;
            } else if (evName == "Right Attack/Block") {
                rightPressed = true;
            }

            if (!leftPressed && !rightPressed) {
                //RE::ConsoleLog::GetSingleton()->Print("No Input Detected");
                return RE::BSEventNotifyControl::kContinue;
            }
            //need to check equipped hand to avoid sending invalid events
            auto* leftSpell = MSCO::Magic::GetEquippedSpellHand(player, Hand::Left);
            auto* rightSpell = MSCO::Magic::GetEquippedSpellHand(player, Hand::Right);

            const bool hasLeftSpell = leftSpell != nullptr;
            const bool hasRightSpell = rightSpell != nullptr;

            //dual cast: both hands must be same spell.
            const bool canDualCandidate =
                leftPressed && rightPressed && hasLeftSpell && hasRightSpell &&
                (leftSpell == rightSpell ||
                (leftSpell && rightSpell && leftSpell->GetFormID() == rightSpell->GetFormID()));
            
            bool dualSent = false;

            //favor dual cast checking for more reliability
            if (canDualCandidate) {
                SendAnimEvent(player, "MSCO_input_dual"sv);
                dualSent = true;
                //RE::ConsoleLog::GetSingleton()->Print("SENT MSCO_input_dual");
            }

            if (!dualSent) {
                if (leftPressed && hasLeftSpell) {
                    SendAnimEvent(player, "MSCO_input_left"sv);
                    //RE::ConsoleLog::GetSingleton()->Print("SENT MSCO_input_left");
                }

                if (rightPressed && hasRightSpell) {
                    SendAnimEvent(player, "MSCO_input_right"sv);
                    //RE::ConsoleLog::GetSingleton()->Print("SENT MSCO_input_right");
                }
            }
            break;
        }
        return RE::BSEventNotifyControl::kContinue;
    }   
}