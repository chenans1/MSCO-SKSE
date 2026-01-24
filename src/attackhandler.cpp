#include "attackhandler.h"
#include "PCH.h"
#include <cstring>

using namespace SKSE;
using namespace SKSE::log;
using namespace SKSE::stl;
using namespace std::literals;

namespace MSCO {
    //using ProcessButton_t = void (RE::AttackBlockHandler::*)(RE::ButtonEvent*, RE::PlayerControlsData*);
    //store the original pointer as we're going to return if we don't use it so another mod that hooks into same thing doesn't blow up
    //REL::Relocation<ProcessButton_t> g_originalProcessButton;
    //static REL::Relocation<ProcessButton_t> _ProcessButton;
    using ProcessButton_t = void (*)(RE::AttackBlockHandler*, RE::ButtonEvent*, RE::PlayerControlsData*);
    static inline ProcessButton_t _ProcessButton = nullptr;
    
    static void Hook_ProcessButton(RE::AttackBlockHandler* self, RE::ButtonEvent* ev, RE::PlayerControlsData* data) {
        bool swallow = false;
        // basically, early return the left/right input if MSCO_right_lock == 1/MSCO_left_lock == 1
        if (ev && ev->IsDown() && ev->HeldDuration() <= 0.0f) {
            const char* s = ev->QUserEvent().c_str();

            if (s) {
                auto* player = RE::PlayerCharacter::GetSingleton();
                if (player) {
                    int lock = 0;
                    bool ok = false;
                    bool castingActive = false;
                    if (std::strcmp(s, "Left Attack/Block") == 0) {
                        ok = player->GetGraphVariableInt("MSCO_left_lock", lock);
                        //log::info("[ABHook] {} ok={} lock={}", s, ok, lock);
                        /*if (auto* playerCaster = player->GetMagicCaster(RE::MagicSystem::CastingSource::kLeftHand)) {
                            const auto lstate = playerCaster->state.get();
                            if (lstate >= RE::MagicCaster::State::kUnk01) {
                                castingActive = true;
                            }
                        }*/
                        //swallow = (ok && lock != 0 && castingActive);
                    } else if (std::strcmp(s, "Right Attack/Block") == 0) {
                        ok = player->GetGraphVariableInt("MSCO_right_lock", lock);
                        //log::info("[ABHook] {} ok={} lock={}", s, ok, lock);
                        /*if (auto* playerCaster = player->GetMagicCaster(RE::MagicSystem::CastingSource::kRightHand)) {
                            const auto rstate = playerCaster->state.get();
                            if (rstate >= RE::MagicCaster::State::kUnk01) {
                                castingActive = true;
                            }
                        }*/
                        //swallow = (ok && lock != 0 && castingActive);
                    }
                    swallow = (ok && lock != 0);
                    if (swallow) {  
                        //log::info("[ABHook] Swallowed {}", s);
                        return;
                    }
                }
            }
        }

        // forward if we don't do anything so other mods/thigns works just fine
        if (_ProcessButton) {
            _ProcessButton(self, ev, data);
        }
    }

    void AttackBlockHook::Install() {
        REL::Relocation<std::uintptr_t> vtbl{RE::VTABLE_AttackBlockHandler[0]};
        //return original vfunc address? 
        const std::uintptr_t orig = vtbl.write_vfunc(0x4, &Hook_ProcessButton);
        // convert raw address -> function pointer
        _ProcessButton = reinterpret_cast<ProcessButton_t>(orig);
    }
}
