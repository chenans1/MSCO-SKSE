//animation event sink handler
#pragma once
#include "PCH.h"

namespace MSCO {
    class AnimationEventHandler : public RE::BSTEventSink<RE::BSAnimationGraphEvent> {
    public:
        static AnimationEventHandler& GetSingleton() {
            static AnimationEventHandler singleton;
            return singleton;
        }

        //graph event notify control so we can actually process things
        RE::BSEventNotifyControl ProcessEvent(
            const RE::BSAnimationGraphEvent* a_event,
            RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override;

    private:
        // constructors and destructors
        AnimationEventHandler() = default;
        ~AnimationEventHandler() = default;
        AnimationEventHandler(const AnimationEventHandler&) = delete;
        AnimationEventHandler(AnimationEventHandler&&) = delete;
        AnimationEventHandler& operator=(const AnimationEventHandler&) = delete;
        AnimationEventHandler& operator=(AnimationEventHandler&&) = delete;
    };
}
