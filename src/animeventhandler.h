//animation event sink handler
#pragma once
#include "PCH.h"

namespace MSCO {
    /*we listen on the animation event sink 
    and trigger stuff when the events we about fire*/
    class AnimationEventSink : public RE::BSTEventSink<RE::BSAnimationGraphEvent> {
    public:
        static AnimEventSink* GetSingleton() { 
            static AnimEventSink instance;
            return std::addressof(instance);
        }

        /*process function*/
        RE::BSEventNotifyControl ProcessEvent(
            const RE::BSAnimationGraphEvent* a_event,
            RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override;
        )
    private:
        AnimationEventSink() = default;
    };
}