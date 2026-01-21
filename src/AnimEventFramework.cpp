#include "AnimEventFramework.h"

using namespace SKSE;
using namespace SKSE::log;
using namespace std::literals;

namespace MSCO {
    static const char* SafeName(const RE::NiAVObject* obj) {
        if (!obj) return "<null>";
        // NiAVObject usually has BSFixedString name
        // In CommonLib: obj->name.c_str()
        const auto& nm = obj->name;
        const char* s = nm.c_str();
        return (s && s[0]) ? s : "<noname>";
    }

    static void LogNodeBasic(const RE::NiAVObject* obj, std::string_view label) {
        if (!obj) {
            log::info("[Node] {}: <null>", label);
            return;
        }

        // AsNode() returns NiNode* if it is a node (or nullptr)
        auto* node = const_cast<RE::NiAVObject*>(obj)->AsNode();

        log::info("[Node] {}: ptr={} name='{}' isNode={} rtti={}", label, fmt::ptr(obj), SafeName(obj), node != nullptr,
                  fmt::ptr(obj->GetRTTI()));  // optional: RTTI pointer
    }

    std::string_view logState(RE::Actor* actor, MSCO::Magic::Hand hand) {
        if (!actor) return "Invalid actor";

        const auto source = MSCO::Magic::HandToSource(hand);
        auto* caster = actor->GetMagicCaster(source);
        if (!caster) return "No MagicCaster";

        using S = RE::MagicCaster::State;
        const S state = caster->state.get();

        switch (state) {
            case S::kNone:
                return "None";
            case S::kUnk01:
                return "Unk01";
            case S::kUnk02:
                return "Unk02";
            case S::kReady:
                return "Ready";
            case S::kUnk04:
                return "Unk04";
            case S::kCharging:
                return "Charging";
            case S::kCasting:
                return "Casting";
            case S::kUnk07:
                return "Unk07";
            case S::kUnk08:
                return "Interrupt";
            case S::kUnk09:
                return "Interrupt/Deselect";
            default:
                return "Unknown";
        }
    }

    std::string_view logState2(RE::Actor* actor, RE::MagicSystem::CastingSource source) {
        if (!actor) return "Invalid actor";
        auto* caster = actor->GetMagicCaster(source);
        if (!caster) return "No MagicCaster";

        using S = RE::MagicCaster::State;
        const S state = caster->state.get();

        switch (state) {
            case S::kNone:
                return "None";
            case S::kUnk01:
                return "Unk01";
            case S::kUnk02:
                return "Unk02";
            case S::kReady:
                return "Ready";
            case S::kUnk04:
                return "Unk04";
            case S::kCharging:
                return "Charging";
            case S::kCasting:
                return "Casting";
            case S::kUnk07:
                return "Unk07";
            case S::kUnk08:
                return "Interrupt";
            case S::kUnk09:
                return "Interrupt/Deselect";
            default:
                return "Unknown";
        }
    }

    std::string_view convert_state(RE::MagicCaster::State state) {
        using S = RE::MagicCaster::State;
        switch (state) {
            case S::kNone:
                return "None";
            case S::kUnk01:
                return "Unk01";
            case S::kUnk02:
                return "Unk02";
            case S::kReady:
                return "Ready";
            case S::kUnk04:
                return "Unk04";
            case S::kCharging:
                return "Charging";
            case S::kCasting:
                return "Casting";
            case S::kUnk07:
                return "Unk07";
            case S::kUnk08:
                return "Interrupt";
            case S::kUnk09:
                return "Interrupt/Deselect";
            default:
                return "Unknown";
        }
    }
    //node replace to change the location of where the spell fires from
    static void replaceNode(RE::Actor* actor, RE::MagicSystem::CastingSource ogSource, RE::MagicSystem::CastingSource outputSource) {
        if (auto root = actor->Get3D()) {
            static constexpr std::string_view NodeNames[2] = {"NPC L MagicNode [LMag]"sv, "NPC R MagicNode [RMag]"sv};
            const auto& nodeName = (outputSource == RE::MagicSystem::CastingSource::kLeftHand) ? NodeNames[0]   // LMag
                                                                                               : NodeNames[1];  // RMag
            auto& rd = actor->GetActorRuntimeData();
            const int slot = (ogSource == RE::MagicSystem::CastingSource::kLeftHand) ? RE::Actor::SlotTypes::kLeftHand
                                                                                   : RE::Actor::SlotTypes::kRightHand;
            auto* actorCaster = rd.magicCasters[slot];
            auto bone = root->GetObjectByName(nodeName);
            if (auto output_node = bone->AsNode()) {
                actorCaster->magicNode = output_node;
                //auto* node = actorCaster->GetMagicNode();
                //LogNodeBasic(node, "ActorCaster magicNode");
            }
        }
    }

    void AnimEventHook::Install() {
        log::info("[MSCO] Installing AnimEventFramework hook...");

        REL::Relocation<std::uintptr_t> vtblNPC{RE::VTABLE_Character[2]};
        REL::Relocation<std::uintptr_t> vtblPC{RE::VTABLE_PlayerCharacter[2]};

        _originalNPC = vtblNPC.write_vfunc(0x1, ProcessEvent_NPC);
        _originalPC = vtblPC.write_vfunc(0x1, ProcessEvent_PC);

        log::info("[MSCO] ...AnimEventFramework hook installed");
    }

    //always returning the original process function to not mess with other functionality.
    AnimEventHook::EventResult AnimEventHook::ProcessEvent_NPC(
        RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_sink,
        RE::BSAnimationGraphEvent* a_event,
        RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource) {
        if (HandleEvent(a_event)) return RE::BSEventNotifyControl::kContinue;
        return _originalNPC(a_sink, a_event, a_eventSource);
    }

    AnimEventHook::EventResult AnimEventHook::ProcessEvent_PC(
        RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_sink, 
        RE::BSAnimationGraphEvent* a_event,
        RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource) {
        if (HandleEvent(a_event)) return RE::BSEventNotifyControl::kContinue;
        return _originalPC(a_sink, a_event, a_eventSource);
    }    

    //interrupts vanilla conc casting on transition
    bool AnimEventHook::HandleEvent(RE::BSAnimationGraphEvent* a_event) {
        if (!a_event || !a_event->holder || !a_event->tag.data()) return false;
        auto* holder = const_cast<RE::TESObjectREFR*>(a_event->holder);
        if (!holder) return false;
        auto* actor = holder ? holder->As<RE::Actor>() : nullptr;
        /*const RE::Actor* actor = nullptr;
        actor = a_event.holder->As<RE::Actor>();*/
        if (!actor) return false;
        
        //log::info("HandleEvent Called");
        const auto& tag = a_event->tag;
        const auto& payload = a_event->payload;

        //if We see the "CastingStateExit" Event we interrupt/clear both hands -> reset nodes
        if (tag == "CastingStateExit"sv) {
            RE::MagicItem* leftSpell = GetEquippedMagicItemForHand(actor, MSCO::Magic::Hand::Left);
            RE::MagicItem* rightSpell = GetEquippedMagicItemForHand(actor, MSCO::Magic::Hand::Right);
            //auto& rd = actor->GetActorRuntimeData();

            if (rightSpell) {
                actor->NotifyAnimationGraph("MRh_Equipped_Event"sv);
                InterruptHand(actor, MSCO::Magic::Hand::Right);
            }

            if (leftSpell) {
                //log::info("CastingStateExit: left spell exists");
                if (leftSpell->IsTwoHanded()) {
                    // log::info("CastingStateExit: Ritual Spell, ignore");
                    return false;
                }
                actor->NotifyAnimationGraph("MLh_Equipped_Event"sv);
                InterruptHand(actor, MSCO::Magic::Hand::Left);
            }

            
            if (!actor->SetGraphVariableBool("IsCastingRight", false)) {
                log::warn("CastingStateExit: Failed to set IsCastingRight to false");
            }
            if (!actor->SetGraphVariableBool("IsCastingDual", false)) {
                log::warn("CastingStateExit: Failed to set IsCastingDual to false");
            }
            if (!actor->SetGraphVariableBool("IsCastingLeft", false)) {
                log::warn("CastingStateExit: Failed to set IsCastingLeft to false");
            }

            return false;
        }

        //maybe if we send the equipped event the npc ai processes stuff better?
        if (tag == "MSCO_WinOpen"sv) {
            if (GetGraphBool(actor, "bMSCO_RightCasting")) {
                actor->NotifyAnimationGraph("MRh_WinStart"sv);
                if (!actor->SetGraphVariableBool("IsCastingRight"sv, false)) {
                    log::warn("MSCO_WinOpen: Failed to set IsCastingRight to false");
                }
                if (!actor->SetGraphVariableBool("bMSCO_RightCasting"sv, false)) {
                    log::warn("MSCO_WinOpen: Failed to set bMSCO_RightCasting to false");
                }
            }
            if (GetGraphBool(actor, "bMSCO_LeftCasting")) {
                actor->NotifyAnimationGraph("MLh_WinStart"sv);
                if (!actor->SetGraphVariableBool("IsCastingLeft"sv, false)) {
                    log::warn("MSCO_WinOpen: Failed to set IsCastingLeft to false");
                }
                if (!actor->SetGraphVariableBool("bMSCO_LeftCasting"sv, false)) {
                    log::warn("MSCO_WinOpen: Failed to set bMSCO_LeftCasting to false");
                }
            }
            if (GetGraphBool(actor, "bMSCODualCasting")) {
                if (!actor->SetGraphVariableBool("IsCastingDual"sv, false)) {
                    log::warn("MSCO_WinOpen: Failed to set IsCastingDual to false");
                }
                if (!actor->SetGraphVariableBool("bMSCODualCasting"sv, false)) {
                    log::warn("MSCO_WinOpen: Failed to set bMSCODualCasting to false");
                }
            }
            return false;
        }

        if (tag == "MSCO_WinClose"sv) {
            if (GetGraphBool(actor, "bMSCO_RightCasting")) actor->NotifyAnimationGraph("MRh_WinEnd"sv);
            if (GetGraphBool(actor, "bMSCO_LeftCasting") || GetGraphBool(actor, "bMSCODualCasting")) {
                actor->NotifyAnimationGraph("MLh_WinEnd"sv);
            }
            if (GetGraphBool(actor, "bMSCO_LRCasting")) {
                actor->NotifyAnimationGraph("MRh_WinEnd"sv);
                actor->NotifyAnimationGraph("MLh_WinEnd"sv);
            }
            return false;
        }

        if (tag == "MSCO_LR_ready"sv) {
            actor->SetGraphVariableInt("MSCO_right_lock"sv, 0);
            actor->SetGraphVariableInt("MSCO_left_lock"sv, 0);
            actor->NotifyAnimationGraph("MRh_WinStart"sv);
            actor->NotifyAnimationGraph("MLh_WinStart"sv);
            if (!actor->SetGraphVariableBool("IsCastingRight"sv, false)) {
                log::warn("MSCO_LR_ready: Failed to set IsCastingRight to false");
            }
            if (!actor->SetGraphVariableBool("IsCastingLeft"sv, false)) {
                log::warn("MSCO_LR_ready: Failed to set IsCastingLeft to false");
            }
            return false;
        }

        //on MCO_WinOpen/BFCO_WinOpen Allow for the casting?
        if (tag == "MCO_WinOpen"sv || tag == "MCO_PowerWinOpen"sv || tag=="BFCO_NextWinStart"sv || tag=="BFCO_NextPowerWinStart"sv) {
            actor->NotifyAnimationGraph("CastOkStart"sv);
            return false;
        }

        //spellfire event handlers->just handles source replacement
        if (tag == "MRh_SpellFire_Event"sv) {
            //if (auto* caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kRightHand)) {
            //    log::info("caster casting timer: {}", caster->castingTimer);
            //}
            if (!GetGraphBool(actor, "bIsMSCO")) return false;
            if (!GetGraphBool(actor, "bMSCO_LRCasting")) actor->SetGraphVariableInt("MSCO_right_lock"sv, 0);
            replaceNode(actor, RE::MagicSystem::CastingSource::kRightHand, RE::MagicSystem::CastingSource::kRightHand); //need to force reset the node because the game doesnt
            RE::MagicItem* CurrentSpell = GetEquippedMagicItemForHand(actor, MSCO::Magic::Hand::Right);
            if (!CurrentSpell) {
                log::warn("No CurrentSpell");
                return false;
            }
            log::info("MRh_SpellFire_Event state = {}", logState2(actor, RE::MagicSystem::CastingSource::kRightHand));
            const auto parsed = MSCO::ParseSpellFire(payload);
            auto output_source = parsed.src.value_or(RE::MagicSystem::CastingSource::kRightHand);
            if (MSCO::Magic::ConsumeResource(RE::MagicSystem::CastingSource::kRightHand, actor, CurrentSpell, GetGraphBool(actor, "bMSCODualCasting"), 1.0f)) {
                if (RE::MagicSystem::CastingSource::kRightHand != output_source) {  // only replace if it's different
                    replaceNode(actor, RE::MagicSystem::CastingSource::kRightHand, output_source);
                    /*MSCO::Magic::Spellfire(RE::MagicSystem::CastingSource::kRightHand, actor, CurrentSpell,
                                           GetGraphBool(actor, "bMSCODualCasting"), 1.0f, output_source);*/
                }
            }
            return false;
        }

        if (tag == "MLh_SpellFire_Event"sv) {
            if (!GetGraphBool(actor, "bIsMSCO")) return false;
            if (!GetGraphBool(actor, "bMSCO_LRCasting")) actor->SetGraphVariableInt("MSCO_left_lock"sv, 0);
            replaceNode(actor, RE::MagicSystem::CastingSource::kLeftHand, RE::MagicSystem::CastingSource::kLeftHand);
            RE::MagicItem* CurrentSpell = GetEquippedMagicItemForHand(actor, MSCO::Magic::Hand::Left);
            if (!CurrentSpell) {
                log::warn("No CurrentSpell");
                return false;
            }
            log::info("MLh_SpellFire_Event state = {}", logState2(actor, RE::MagicSystem::CastingSource::kLeftHand));
            const auto parsed = ParseSpellFire(payload);
            auto output_source = parsed.src.value_or(RE::MagicSystem::CastingSource::kLeftHand);
            if (MSCO::Magic::ConsumeResource(RE::MagicSystem::CastingSource::kLeftHand, actor, CurrentSpell,
                                             GetGraphBool(actor, "bMSCODualCasting"), 1.0f)) {
                if (RE::MagicSystem::CastingSource::kLeftHand != output_source) {  // only replace if it's different
                    replaceNode(actor, RE::MagicSystem::CastingSource::kLeftHand, output_source);
                    /*MSCO::Magic::Spellfire(RE::MagicSystem::CastingSource::kRightHand, actor, CurrentSpell,
                                           GetGraphBool(actor, "bMSCODualCasting"), 1.0f, output_source);*/
                }
            }
            return false;
        }        
        //BeginCastLeft/Right handler. Handles button suppresion and MSCO inputs
        MSCO::Magic::Hand beginHand{};
        if (IsBeginCastEvent(tag, beginHand)) {
            const char* lockName = (beginHand == MSCO::Magic::Hand::Right) ? "MSCO_right_lock" : "MSCO_left_lock";
            std::int32_t lock = 0;
            const bool ok = actor->GetGraphVariableInt(lockName, lock);

            if (ok && lock != 0) return true;  // swallow the BeginCastLeft/Right Event
            if (actor->IsSneaking()) return false; //check if we are sneaking nor not - don't do anything if we are
            //if npc not in combat also default to vanilla behaviors
            if (!actor->IsInCombat() && !actor->IsPlayerRef()) {
                log::info("non-combat NPC spell casting - normal behaviors");
                return false;
            }
            if (GetGraphBool(actor, "IsFirstPerson")) return false; //dont affect first person 
            RE::MagicItem* CurrentSpell = GetEquippedMagicItemForHand(actor, beginHand);
            if (!CurrentSpell) {log::warn("No CurrentSpell"); return false;}
            //GOING TO IGNORE RITUAL SPELLS FOR NOW
            if (CurrentSpell->IsTwoHanded()) {
                //log::info("Equipped Spell is Ritual, Ignore");
                return false;
            }

            //bool isfnf = (CurrentSpell->GetCastingType() == RE::MagicSystem::CastingType::kFireAndForget);
            
            if (CurrentSpell->GetCastingType() != RE::MagicSystem::CastingType::kFireAndForget &&
                CurrentSpell->GetCastingType() != RE::MagicSystem::CastingType::kScroll) {
                return false;
            }
            //if (actor->SetGraphVariableInt("iMSCO_ON"sv, 1)) log::info("iMSCO_ON == 1");
            auto& rd = actor->GetActorRuntimeData();
            const int slot = (beginHand == MSCO::Magic::Hand::Left) ? RE::Actor::SlotTypes::kLeftHand : RE::Actor::SlotTypes::kRightHand;
            auto* actorCaster = rd.magicCasters[slot];

            //additional state check to block wonky behaviors maybe?
            if (actorCaster->state.get() >= RE::MagicCaster::State::kUnk02) {
                return true;
            }

            if (!actor->SetGraphVariableInt("iMSCO_ON"sv, 1)) log::warn("set iMSCO_ON == 1 failed");
            //turns out the game actually already sets the dual casting value correctly by the time we intercept begincastleft
            bool wantDualCasting = actorCaster->GetIsDualCasting();
            if (wantDualCasting) { 
                if (!actor->SetGraphVariableBool("IsCastingDual"sv, true)) {
                    log::warn("MSCO_start_dual: Failed to set IsCastingDual to true");
                }
                actor->NotifyAnimationGraph("MSCO_start_dual"sv);
                actorCaster->castingTimer = 0.00f;
                //log::info("DualCast BeginCast state = {}", logState2(actor, RE::MagicSystem::CastingSource::kLeftHand));
                if (!actor->IsPlayerRef()) actorCaster->state.set(RE::MagicCaster::State::kUnk04);
                return false;
            }

            //need to grab the other hand, check if it's fnf in order to handle sending the LR event properly
            RE::MagicItem* otherSpell = GetEquippedMagicItemForHand(actor, (beginHand == MSCO::Magic::Hand::Right) ? MSCO::Magic::Hand::Left : MSCO::Magic::Hand::Right);
            const bool otherisFNF = otherSpell && otherSpell->GetCastingType() == RE::MagicSystem::CastingType::kFireAndForget;
            //const RE::MagicSystem::CastingSource currentSource = (beginHand == MSCO::Magic::Hand::Right)
            //                                                         ? RE::MagicSystem::CastingSource::kRightHand
            //                                                         : RE::MagicSystem::CastingSource::kLeftHand;
            //gonna check if we want to cast the other hand if that's the case we send the MSCO_start_lr event instead
            if (!otherisFNF) {
                if (actor->SetGraphVariableBool(
                        (beginHand == MSCO::Magic::Hand::Right) ? "IsCastingRight"sv : "IsCastingLeft"sv, true)) {
                } else {
                    log::warn("{}: Failed to set {} to true",
                              (beginHand == MSCO::Magic::Hand::Right) ? "MSCO_start_right"sv : "MSCO_start_left"sv,
                              (beginHand == MSCO::Magic::Hand::Right) ? "IsCastingRight" : "IsCastingLeft");
                }
                //log::info("other spell is not fnf, sending default MSCO animevent");
                actor->NotifyAnimationGraph((beginHand == MSCO::Magic::Hand::Right) ? "MSCO_start_right"sv : "MSCO_start_left"sv);
                //actor->SetGraphVariableInt((beginHand == MSCO::Magic::Hand::Right) ? "MSCO_right_lock"sv : "MSCO_left_lock"sv, 1);
                actorCaster->castingTimer = 0.00f;
                //log::info("BeginCast state = {}", logState2(actor, currentSource));
                if (!actor->IsPlayerRef()) actorCaster->state.set(RE::MagicCaster::State::kUnk04);
                return false;
            } else {
                //think I check for the bwantcast boolean var here? not sure if that guarantees the otherhand spell firing. maybe I should check for the caster state?
                auto* otherCaster = actor->GetMagicCaster((beginHand == MSCO::Magic::Hand::Right)
                                                              ? RE::MagicSystem::CastingSource::kLeftHand
                                                              : RE::MagicSystem::CastingSource::kRightHand);
                //i think this works not sure
                //need to check that current spell is two handed, also needs both MRH and mlh spell fire. It's possible it also needs order, but I'm not sure
                if (otherCaster->state.get() >= RE::MagicCaster::State::kReady) {
                    if (!actor->SetGraphVariableBool("IsCastingRight"sv, true)) {
                        log::warn("MSCO_start_lr: Failed to set IsCastingRight to true");
                    }
                    if (!actor->SetGraphVariableBool("IsCastingLeft"sv, true)) {
                        log::warn("MSCO_start_lr: Failed to set IsCastingLeft to true");
                    }
                    actor->NotifyAnimationGraph("MSCO_start_lr"sv);
                } else {
                    if (!actor->SetGraphVariableBool((beginHand == MSCO::Magic::Hand::Right) ? "IsCastingRight"sv : "IsCastingLeft"sv, true)) {
                        log::warn("{}: Failed to set {} to true",
                                  (beginHand == MSCO::Magic::Hand::Right) ? "MSCO_start_right"sv : "MSCO_start_left"sv,
                                  (beginHand == MSCO::Magic::Hand::Right) ? "IsCastingRight" : "IsCastingLeft");
                    }
                    actor->NotifyAnimationGraph((beginHand == MSCO::Magic::Hand::Right) ? "MSCO_start_right"sv : "MSCO_start_left"sv); 
                }
                actorCaster->castingTimer = 0.00f;
                //log::info("BeginCast state = {}", logState2(actor, currentSource));
                //log::info("casting timer at begincast: {}", actorCaster->castingTimer);
                if (!actor->IsPlayerRef()) actorCaster->state.set(RE::MagicCaster::State::kUnk04);
                return false;
            }
        }

        //interrupt conc spell in other hand if applicable. Also: Fallback state set. Will compute charge time here
        MSCO::Magic::Hand firingHand{};
        if (IsMSCOEvent(tag, firingHand)) {
            //log::info("msco casting event detected");
            //log::info("{} state = {}", tag.data(),logState2(actor, currentSource));
            const RE::MagicSystem::CastingSource currentSource = (firingHand == MSCO::Magic::Hand::Right)
                                                                     ? RE::MagicSystem::CastingSource::kRightHand
                                                                     : RE::MagicSystem::CastingSource::kLeftHand;
           //start locking 
           const auto lockvar = (currentSource == RE::MagicSystem::CastingSource::kRightHand) ? "MSCO_right_lock"sv : "MSCO_left_lock"sv;
           if (!actor->SetGraphVariableInt(lockvar, 1)) {
               log::warn("{}: failed to set {} to 1", tag.data(), lockvar);
           }
            const auto otherHand =
                (firingHand == MSCO::Magic::Hand::Right) ? MSCO::Magic::Hand::Left : MSCO::Magic::Hand::Right;

            if (auto* otherItem = GetCurrentlyCastingMagicItem(actor, otherHand)) {
                if (otherItem->GetCastingType() == RE::MagicSystem::CastingType::kConcentration) {
                    InterruptHand(actor, otherHand);
                }
            }

            return false;
        }

        if (tag == "Dual_MSCOStart") {
            if (!actor->SetGraphVariableInt("MSCO_right_lock"sv, 1)) {
                log::warn("{}: failed to set MSCO_right_lock to 1", tag.data());
            }
            if (!actor->SetGraphVariableInt("MSCO_left_lock"sv, 1)) {
                log::warn("{}: failed to set MSCO_right_lock to 1", tag.data());
            }
            return false;
        }
        
        if (tag == "LR_MSCOStart") {
            if (!actor->SetGraphVariableInt("MSCO_right_lock"sv, 1)) {
                log::warn("{}: failed to set MSCO_right_lock to 1", tag.data());
            }
            if (!actor->SetGraphVariableInt("MSCO_left_lock"sv, 1)) {
                log::warn("{}: failed to set MSCO_right_lock to 1", tag.data());
            }
            return false;
        }

        return false;
    }

    //the mrh_spellaimedstart type specific events do not appear in the event sinks - they are notifys, do workaround instead:
    bool AnimEventHook::IsBeginCastEvent(const RE::BSFixedString& tag, MSCO::Magic::Hand& outHand) {
        if (tag == "BeginCastRight"sv) {
            outHand = MSCO::Magic::Hand::Right;
            return true;
        }
        if (tag == "BeginCastLeft"sv) {
            outHand = MSCO::Magic::Hand::Left;
            return true;
        }
        return false;
    }

    bool AnimEventHook::IsMSCOEvent(const RE::BSFixedString& tag, MSCO::Magic::Hand& outHand) {
        if (tag == "RightMSCOStart"sv) {
            outHand = MSCO::Magic::Hand::Right;
            return true;
        }
        if (tag == "LeftMSCOStart"sv) {
            outHand = MSCO::Magic::Hand::Left;
            return true;
        }
        return false;
    }

    //gets the equipped magic item for hand. should handle staves (no idea for scrolls?)
    RE::MagicItem* AnimEventHook::GetEquippedMagicItemForHand(RE::Actor* actor, MSCO::Magic::Hand hand) { 
        if (!actor) {
            return nullptr;
        }
        //first, check for spell. 
        if (auto* form = MSCO::Magic::GetEquippedSpellHand(actor, hand)) {
            if (auto* spell = form->As<RE::SpellItem>()) {
                return spell;  // SpellItem is a MagicItem
            }   
        }

        //then check for staff:
        const bool left = (hand == MSCO::Magic::Hand::Left);
        auto* equippedForm = actor->GetEquippedObject(left);
        auto* weap = equippedForm ? equippedForm->As<RE::TESObjectWEAP>() : nullptr;
        if (!weap) {
            return nullptr;
        }
        RE::EnchantmentItem* ench = nullptr;
        ench = weap->formEnchanting;

        if (!ench) {
            return nullptr;
        }

        return ench;
    }

    //can check for the source, im assumign some of the edits come from other hand?
    RE::MagicItem* AnimEventHook::GetCurrentlyCastingMagicItem(RE::Actor* actor, MSCO::Magic::Hand hand) {
        if (!actor) {
            return nullptr;
        }
        const auto source = MSCO::Magic::HandToSource(hand);
        auto* caster = actor->GetMagicCaster(source);
        if (!caster) {
            return nullptr;
        }

        return caster->currentSpell;
    }

    //interrupts the spell at specified hand source. 
    void AnimEventHook::InterruptHand(RE::Actor* actor, MSCO::Magic::Hand hand) { 
        const auto source = MSCO::Magic::HandToSource(hand);
        if (auto* caster = actor->GetMagicCaster(source); caster) {
            caster->InterruptCast(true);
            //log::info("InterruptedCast");
        }
        //do nothing if else.
    }

    //grab the input bool
    bool AnimEventHook::GetGraphBool(RE::Actor* actor, const char* name, bool defaultValue) {
        bool v = defaultValue;
        if (actor) {
            actor->GetGraphVariableBool(name, v);
        }
        return v;
    }
}