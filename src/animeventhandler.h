//animation event sink handler
#pragma once
#include "PCH.h"

namespace MSCO {
    class AnimationEventSink : public RE::BSTEventSink<RE::BSAnimationGraphEvent> {
    public:
        //grab the animationeventsink
        static AnimationEventSink* GetSingleton() {
            static AnimationEventSink instance;
            return std::addressof(instance);
        }

        //graph event notify control so we can actually process things
        RE::BSEventNotifyControl ProcessEvent(
            const RE::BSAnimationGraphEvent* a_event,
            RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override;

    private:
        AnimationEventSink() = default;
    };
}
