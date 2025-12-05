#include "PCH.h"

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
        return RE::BSEventNotifyControl::kContinue;
    }
    //abort if no actor
    auto* actor = a_event->holder->As<RE::Actor>();
    if (!actor) {
        return RE::BSEventNotifyControl::kContinue;
    }
    const auto& tag = a_event->tag;  // BSFixedString
    constexpr auto kLeftTag = "lh_msco_cast"sv;
    constexpr auto kRightTag = "rh_msco_cast"sv;
    constexpr auto kDualTag = "dc_msco_cast"sv;

    if (tag == kLeftTag) {
        log::info("{} has Left Casted.", actor->GetName());
        //logger::info("LH event for {}", actor->GetName());
        //CastEquippedHand(actor, /*left*/ true, /*dual*/ false);
    } else if (tag == kRightTag) {
        log::info("{} has Right Casted.", actor->GetName());
        //logger::info("RH event for {}", actor->GetName());
        //CastEquippedHand(actor, /*left*/ false, /*dual*/ false);
    } else if (tag == kDualTag) {
        log::info("{} has Dual Casted.", actor->GetName());
        //logger::info("DC event for {}", actor->GetName());
        //CastEquippedHand(actor, /*left*/ true, /*dual*/ true);
    }

    return RE::BSEventNotifyControl::kContinue;
}