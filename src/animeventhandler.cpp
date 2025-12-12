#include "PCH.h"
#include "magichandler.h"

using namespace SKSE;
using namespace SKSE::log;
using namespace std::literals;

RE::BSEventNotifyControl MSCO::AnimationEventHandler::ProcessEvent(
    const RE::BSAnimationGraphEvent* a_event,
    RE::BSTEventSource<RE::BSAnimationGraphEvent>*)
{   
    //log::info("BSEventNotifyControl Triggered");
    //abort if no event or no event holder
    if (!a_event || !a_event->holder) {
        log::info("WARNING: NO ANIM EVENT PASSED");
        return RE::BSEventNotifyControl::kContinue;
    }
    //abort if no actor
    auto* actor = a_event->holder->As<RE::Actor>();
    if (!actor) {
        log::info("WARNING: NO ACTOR FOR FOR EVENT");
        return RE::BSEventNotifyControl::kContinue;
    }

    auto* mutableActor = const_cast<RE::Actor*>(actor);

    const auto& tag = a_event->tag;  // BSFixedString
    constexpr auto kLeftTag = "lh_msco_cast"sv;
    constexpr auto kRightTag = "rh_msco_cast"sv;
    constexpr auto kDualTag = "dh_msco_cast"sv;
    constexpr auto kDualTag_right = "dh_msco_cast_right"sv;

    if (tag == kLeftTag) {
        log::info("{} has Left Casted.", mutableActor->GetName());
        //CastEquippedHand(actor, /*left*/ true, /*dual*/ false);
        MSCO::Magic::CastEquippedHand(mutableActor, MSCO::Magic::Hand::Left, false);
    } else if (tag == kRightTag) {
        log::info("{} has Right Casted.", mutableActor->GetName());
        //CastEquippedHand(actor, /*left*/ false, /*dual*/ false);
        MSCO::Magic::CastEquippedHand(mutableActor, MSCO::Magic::Hand::Right, false);
    } else if (tag == kDualTag) {
        log::info("{} has Dual Casted.", mutableActor->GetName());
        //CastEquippedHand(actor, /*left*/ true, /*dual*/ true);
        MSCO::Magic::CastEquippedHand(mutableActor, MSCO::Magic::Hand::Left, true);
    } else if (tag == kDualTag_right) {
        log::info("{} has Right Dual Casted.", mutableActor->GetName());
        MSCO::Magic::CastEquippedHand(mutableActor, MSCO::Magic::Hand::Right, true);
    }

    return RE::BSEventNotifyControl::kContinue;
}