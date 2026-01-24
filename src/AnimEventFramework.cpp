#include "PCH.h"

#include "AnimEventFramework.h"
#include "magichandler.h"
#include "PayloadHandler.h"
#include "settings.h"

using namespace SKSE;
using namespace SKSE::log;
using namespace SKSE::stl;

namespace MSCO {
    struct MSCOChargeSpeedCFG {
        float shortest;   // MSCO_shortest
        float longest;    // MSCO_longest
        float baseTime;   // MSCO_basetime
        float minSpeed;   // MSCO_minspeed
        float maxSpeed;   // MSCO_maxspeed
        float expFactor;  // MSCO_ExpFactor
    };

    static MSCOChargeSpeedCFG getCFG() {
        const auto& s = settings::Get();
        MSCOChargeSpeedCFG cfg{};
        cfg.shortest = s.shortest;
        cfg.longest = s.longest;
        cfg.baseTime = s.basetime;
        cfg.minSpeed = s.minspeed;
        cfg.maxSpeed = s.maxspeed;
        cfg.expFactor = s.expFactor;

        // sanitize
        if (cfg.longest < cfg.shortest) {
            std::swap(cfg.longest, cfg.shortest);
        }

        cfg.shortest = std::max(0.0f, cfg.shortest);
        cfg.longest = std::max(cfg.shortest, cfg.longest);

        cfg.baseTime = std::clamp(cfg.baseTime, cfg.shortest, cfg.longest);

        cfg.minSpeed = std::max(0.01f, cfg.minSpeed);
        cfg.maxSpeed = std::max(cfg.minSpeed, cfg.maxSpeed);

        // your exp mode clamps expFactor internally too, but keep it sane here
        cfg.expFactor = std::clamp(cfg.expFactor, 0.01f, 10.0f);

        return cfg;
    }
    static bool IsConvertChargeTime() { return settings::Get().chargeMechanicOn; }
    static bool IsExpMode() { return settings::Get().expMode; }
    static bool NPCAllowed() { return settings::Get().NPCAllowed; }
    static bool PlayerAllowed() { return settings::Get().PlayerAllowed; }
    static float GetNPCFactor() { return settings::Get().NPCFactor; }
    static bool LogEnabled() { return settings::Get().log; }

    static const char* SafeName(const RE::NiAVObject* obj) {
        if (!obj) return "<null>";
        // NiAVObject usually has BSFixedString name
        // In CommonLib: obj->name.c_str()
        const auto& nm = obj->name;
        const char* s = nm.c_str();
        return (s && s[0]) ? s : "<noname>";
    }

    //log node function
    static void LogNodeBasic(const RE::NiAVObject* obj, std::string_view label) {
        if (!obj) {
            log::info("[AnimEventFramework] {}: <null>", label);
            return;
        }

        // AsNode() returns NiNode* if it is a node (or nullptr)
        auto* node = const_cast<RE::NiAVObject*>(obj)->AsNode();

        log::info("[AnimEventFramework] {}: ptr={} name='{}' isNode={} rtti={}", label, fmt::ptr(obj), SafeName(obj), node != nullptr,
                  fmt::ptr(obj->GetRTTI()));  // optional: RTTI pointer
    }

    // logging the charge time
    static void log_chargetime(RE::BSFixedString tag, RE::Actor* actor, float chargetime, float speed) {
        const char* actorName = actor->GetName();
        if (!actorName || actorName[0] == '\0') {
            actorName = "<unnamed actor>";
        }
        log::info("[AnimEventFramework] {} | actor='{}' | chargeTime={:.3f} | speed={:.3f}", tag.data(), actorName,
                  chargetime, speed);
    }
    static std::string_view convert_state(RE::MagicCaster::State state) {
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
                if (LogEnabled()) {
                    auto* node = actorCaster->GetMagicNode();
                    LogNodeBasic(node, "ActorCaster magicNode");
                } 
            }
        }
    }
    
    //clamp function.
    static float Clamp(float x, float a, float b) { return std::max(a, std::min(x, b)); }

    //maps charge time to [maxspeed ... 1.0x ] if chargetime < base, else maps it to [1.0x ... minspeed]
    float getSpeed(float chargeTime, float shortest, float longest, float base, float minSpeed, float maxSpeed) {
        //sanitize values first to avoid any divide by zeroes or negatives
        shortest = std::min(shortest, longest);
        base = Clamp(base, shortest, longest);
        minSpeed = std::max(0.0f, minSpeed);
        maxSpeed = std::max(minSpeed, maxSpeed);

        const float t = Clamp(chargeTime, shortest, longest);
        // Exactly base => exactly 1.0
        if (std::abs(t - base) < 1e-6f) {
            return 1.0f;
        }

        //Faster side: [shortest .. base] maps to [maxSpeed .. 1.0]
        if (t < base) {
            const float denom = (base - shortest);
            if (denom <= 1e-6f) {
                return 1.0f;  // base==shortest, no "fast" range
            }
            const float u = (base - t) / denom;  // 0..1
            return 1.0f + u * (maxSpeed - 1.0f);
        }

        //Slower side: [base .. longest] maps to [1.0 .. minSpeed]
        const float denom = (longest - base);
        if (denom <= 1e-6f) {
            return 1.0f;  // base==longest, no "slow" range
        }

        const float v = (t - base) / denom;  // 0..1
        return 1.0f - v * (1.0f - minSpeed);
    }

    //alternative exponential mode -> better if you have wide distribution of charge times and want that reflected in the animation speed
    float getSpeedExp(float chargeTime, float shortest, float longest, float baseTime, float minSpeed, float maxSpeed, float expFactor) {
        // Prevent division by zero or nonsense configs
        if (shortest <= 0.0f) {
            shortest = 0.001f;
        }

        if (longest < shortest) {
            std::swap(longest, shortest);
        }

        if (baseTime <= 0.0f) {
            baseTime = shortest;
        }

        if (minSpeed > maxSpeed) {
            std::swap(minSpeed, maxSpeed);
        }

        // Clamp exponent so designers can’t nuke the curve
        expFactor = std::clamp(expFactor, 0.1f, 4.0f);

        // ---- Clamp charge time ----
        chargeTime = std::clamp(chargeTime, shortest, longest);

        // ---- Anchored reciprocal ----
        float speed = std::pow(baseTime / chargeTime, expFactor);

        // ---- Clamp final speed ----
        speed = std::clamp(speed, minSpeed, maxSpeed);

        return speed;
    }

    float ConvertSpeed(RE::Actor* actor, float chargeTime, const RE::BSFixedString tag, bool expMode) {
        if (!actor) {
            log::warn("ConvertSpeed: null actor", tag.data());
            return 1.0f;
        }
        auto cfg = getCFG();
        float speed = 1.0f;
        if (expMode) {
            speed = getSpeedExp(chargeTime, cfg.shortest, cfg.longest, cfg.baseTime, cfg.minSpeed, cfg.maxSpeed, cfg.expFactor);
        } else {
            speed = getSpeed(chargeTime, cfg.shortest, cfg.longest, cfg.baseTime, cfg.minSpeed, cfg.maxSpeed);
        }

        if (!actor->IsPlayerRef()) {
            speed = std::clamp(speed*GetNPCFactor(), cfg.minSpeed, cfg.maxSpeed);
        }
        return speed;
    }

    void AnimEventHook::Install() {
        log::info("[AnimEventFramework] Installing AnimEventFramework hook...");

        REL::Relocation<std::uintptr_t> vtblNPC{RE::VTABLE_Character[2]};
        REL::Relocation<std::uintptr_t> vtblPC{RE::VTABLE_PlayerCharacter[2]};

        _originalNPC = vtblNPC.write_vfunc(0x1, ProcessEvent_NPC);
        _originalPC = vtblPC.write_vfunc(0x1, ProcessEvent_PC);

        log::info("[AnimEventFramework] ...AnimEventFramework hook installed");
    }

    //always returning the original process function to not mess with other functionality.
    AnimEventHook::EventResult AnimEventHook::ProcessEvent_NPC(
        RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_sink,
        RE::BSAnimationGraphEvent* a_event,
        RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource) {
        if (!NPCAllowed()) {
            return _originalNPC(a_sink, a_event, a_eventSource);
        }
        if (HandleEvent(a_event)) {
            return RE::BSEventNotifyControl::kContinue;
        }
        return _originalNPC(a_sink, a_event, a_eventSource);
    }

    AnimEventHook::EventResult AnimEventHook::ProcessEvent_PC(
        RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_sink, 
        RE::BSAnimationGraphEvent* a_event,
        RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource) {
        if (!PlayerAllowed()) {
            return _originalPC(a_sink, a_event, a_eventSource);
        }
        if (HandleEvent(a_event)) {
            return RE::BSEventNotifyControl::kContinue;
        }
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
        if (tag == "MSCO_MagicReady"sv) {
            RE::MagicItem* leftSpell = GetEquippedMagicItemForHand(actor, MSCO::Magic::Hand::Left);
            RE::MagicItem* rightSpell = GetEquippedMagicItemForHand(actor, MSCO::Magic::Hand::Right);
            //auto& rd = actor->GetActorRuntimeData();
            if (leftSpell) {
                if (auto* left_caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kLeftHand)) {
                    left_caster->ClearMagicNode();
                    if (LogEnabled()) log::info("[AnimEventFramework] {}: leftCaster->clearmagicnode()", tag.data());
                }
                //actor->NotifyAnimationGraph("MLh_Equipped_Event"sv);
            }
            if (rightSpell) {
                if (auto* right_caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kRightHand)) {
                    right_caster->ClearMagicNode();
                    if (LogEnabled()) log::info("[AnimEventFramework] {}: rightCaster->clearmagicnode()", tag.data());
                }
                //actor->NotifyAnimationGraph("MRh_Equipped_Event"sv);
            }
            return false;
        }

        //make sure to interrupt and reset if we exit the state except if it's two-handed, assume vanilla handles that properly
        if (tag == "CastingStateExit"sv) {
            RE::MagicItem* leftSpell = GetEquippedMagicItemForHand(actor, MSCO::Magic::Hand::Left);
            RE::MagicItem* rightSpell = GetEquippedMagicItemForHand(actor, MSCO::Magic::Hand::Right);
            if (leftSpell) {
                if (leftSpell->IsTwoHanded()) return false;
                InterruptHand(actor, MSCO::Magic::Hand::Left);
            }
            if (rightSpell) {
                InterruptHand(actor, MSCO::Magic::Hand::Right);
            }
            return false;
        }

        if (tag == "MSCO_LR_ready"sv) {
            actor->SetGraphVariableInt("MSCO_right_lock"sv, 0);
            actor->SetGraphVariableInt("MSCO_left_lock"sv, 0);
            return false;
        }

        //on MCO_WinOpen/BFCO_WinOpen Allow for the casting?
        if (tag == "MCO_WinOpen"sv || tag == "MCO_PowerWinOpen"sv || tag=="BFCO_NextWinStart"sv || tag=="BFCO_NextPowerWinStart"sv) {
            actor->NotifyAnimationGraph("CastOkStart"sv);
            if (LogEnabled()) log::info("[AnimEventFramework] {}: CastOkStart Notified", tag.data());
            return false;
        }

        //spellfire event handlers->just handles source replacement
        if (tag == "MRh_SpellFire_Event"sv) {
            //if (auto* caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kRightHand)) {
            //    log::info("caster casting timer: {}", caster->castingTimer);
            //}
            if (!GetGraphBool(actor, "bIsMSCO")) return false;
            if (!GetGraphBool(actor, "bMSCO_LRCasting")) actor->SetGraphVariableInt("MSCO_right_lock"sv, 0);
            //replaceNode(actor, RE::MagicSystem::CastingSource::kRightHand, RE::MagicSystem::CastingSource::kRightHand); //need to force reset the node because the game doesnt
            if (auto* caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kRightHand)) {caster->ClearMagicNode();}
            RE::MagicItem* CurrentSpell = GetEquippedMagicItemForHand(actor, MSCO::Magic::Hand::Right);
            if (!CurrentSpell) {
                log::warn("No CurrentSpell");
                return false;
            }
            const auto parsed = MSCO::ParseSpellFire(payload);
            auto output_source = parsed.src.value_or(RE::MagicSystem::CastingSource::kRightHand);
            if (MSCO::Magic::ConsumeResource(RE::MagicSystem::CastingSource::kRightHand, actor, CurrentSpell, false, 1.0f)) {
                if (RE::MagicSystem::CastingSource::kRightHand != output_source) {  // only replace if it's different
                    replaceNode(actor, RE::MagicSystem::CastingSource::kRightHand, output_source);
                }
            }
            return false;
        }

        if (tag == "MLh_SpellFire_Event"sv) {
            if (!GetGraphBool(actor, "bIsMSCO")) return false;
            if (!GetGraphBool(actor, "bMSCO_LRCasting")) actor->SetGraphVariableInt("MSCO_left_lock"sv, 0);
            if (auto* caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kLeftHand)) {caster->ClearMagicNode();}
            //replaceNode(actor, RE::MagicSystem::CastingSource::kLeftHand, RE::MagicSystem::CastingSource::kLeftHand);
            RE::MagicItem* CurrentSpell = GetEquippedMagicItemForHand(actor, MSCO::Magic::Hand::Left);
            if (!CurrentSpell) {
                log::warn("No CurrentSpell");
                return false;
            }
            const auto parsed = ParseSpellFire(payload);
            auto output_source = parsed.src.value_or(RE::MagicSystem::CastingSource::kLeftHand);
            if (MSCO::Magic::ConsumeResource(RE::MagicSystem::CastingSource::kLeftHand, actor, CurrentSpell, GetGraphBool(actor, "bMSCODualCasting"), 1.0f)) {
                if (RE::MagicSystem::CastingSource::kLeftHand != output_source) {  // only replace if it's different
                    replaceNode(actor, RE::MagicSystem::CastingSource::kLeftHand, output_source);
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
                if (LogEnabled()) log::info("[AnimEventFramework] non-combat NPC spell casting - normal behaviors");
                return false;
            }
            if (GetGraphBool(actor, "IsFirstPerson")) return false; //dont affect first person 
            RE::MagicItem* CurrentSpell = GetEquippedMagicItemForHand(actor, beginHand);
            if (!CurrentSpell) {log::warn("No CurrentSpell"); return false;}
            //GOING TO IGNORE RITUAL SPELLS FOR NOW
            if (CurrentSpell->IsTwoHanded()) {
                if (LogEnabled()) log::info("[AnimEventFramework] {}: Ritual Spell, Ignore", tag.data()); return false;
            }
                        
            if (CurrentSpell->GetCastingType() != RE::MagicSystem::CastingType::kFireAndForget &&
                CurrentSpell->GetCastingType() != RE::MagicSystem::CastingType::kScroll) {
                return false;
            }

            auto magicCaster = actor->GetMagicCaster((beginHand == MSCO::Magic::Hand::Right)
                                                         ? RE::MagicSystem::CastingSource::kRightHand
                                                         : RE::MagicSystem::CastingSource::kLeftHand);
            if (!magicCaster) {
                log::warn("[AnimEventFramework] {}: No Magic Caster", tag.data());
                return false;
            }
            //additional state check to block wonky behaviors maybe?
            const auto currentMagicState = magicCaster->state.get();
            if (currentMagicState >= RE::MagicCaster::State::kUnk02) {
                if (LogEnabled()) log::info("");
                return true;
            }
            if (!actor->SetGraphVariableInt("iMSCO_ON"sv, 1)) log::warn("[AnimEventFramework] {}: set iMSCO_ON == 1 failed", tag.data());
            //turns out the game actually already sets the dual casting value correctly by the time we intercept begincastleft
            const bool wantDualCasting = magicCaster->GetIsDualCasting();
            if (wantDualCasting) { 
                actor->NotifyAnimationGraph("MSCO_start_dual"sv);
                if (actor->IsPlayerRef()) {
                    magicCaster->castingTimer = 0.00f;
                } else {
                    magicCaster->castingTimer = 0.02f;
                }
                if (!actor->IsPlayerRef()) {
                    magicCaster->state.set(RE::MagicCaster::State::kUnk04);
                }
                return false;
            }

            //need to grab the other hand, check if it's fnf in order to handle sending the LR event properly
            RE::MagicItem* otherSpell = GetEquippedMagicItemForHand(actor, (beginHand == MSCO::Magic::Hand::Right) ? MSCO::Magic::Hand::Left : MSCO::Magic::Hand::Right);
            const bool otherisFNF = otherSpell && otherSpell->GetCastingType() == RE::MagicSystem::CastingType::kFireAndForget;
            /*const RE::MagicSystem::CastingSource currentSource = (beginHand == MSCO::Magic::Hand::Right)
                                                                     ? RE::MagicSystem::CastingSource::kRightHand
                                                                     : RE::MagicSystem::CastingSource::kLeftHand;*/

            //gonna check if we want to cast the other hand if that's the case we send the MSCO_start_lr event instead
            if (!otherisFNF) {
                //log::info("other spell is not fnf, sending default MSCO animevent");
                actor->NotifyAnimationGraph((beginHand == MSCO::Magic::Hand::Right) ? "MSCO_start_right"sv : "MSCO_start_left"sv);
                //actor->SetGraphVariableInt((beginHand == MSCO::Magic::Hand::Right) ? "MSCO_right_lock"sv : "MSCO_left_lock"sv, 1);
                if (actor->IsPlayerRef()) {
                    magicCaster->castingTimer = 0.00f;
                } else {
                    magicCaster->castingTimer = 0.02f;
                }
                if (!actor->IsPlayerRef()) {
                    magicCaster->state.set(RE::MagicCaster::State::kUnk04);
                }
                return false;
            } else {
                //think I check for the bwantcast boolean var here? not sure if that guarantees the otherhand spell firing. maybe I should check for the caster state?
                auto* otherMagicCaster = actor->GetMagicCaster((beginHand == MSCO::Magic::Hand::Right)
                                                              ? RE::MagicSystem::CastingSource::kLeftHand
                                                              : RE::MagicSystem::CastingSource::kRightHand);

                //check state on other, most reliable method of consistently sending the anim event properly i've found
                if (otherMagicCaster && otherMagicCaster->state.get() >= RE::MagicCaster::State::kReady) {
                    actor->NotifyAnimationGraph("MSCO_start_lr"sv);
                } else {
                    actor->NotifyAnimationGraph((beginHand == MSCO::Magic::Hand::Right) ? "MSCO_start_right"sv : "MSCO_start_left"sv); 
                }
                if (actor->IsPlayerRef()) {
                    magicCaster->castingTimer = 0.00f;
                } else {
                    magicCaster->castingTimer = 0.02f;
                }
                if (!actor->IsPlayerRef()) {
                    magicCaster->state.set(RE::MagicCaster::State::kUnk04);
                }
                return false;
            }
        }

        //interrupt conc spell in other hand if applicable. Also: Fallback state set. Will compute charge time here
        MSCO::Magic::Hand firingHand{};
        if (IsMSCOEvent(tag, firingHand)) {
            //log::info("msco casting event detected");
            const RE::MagicSystem::CastingSource currentSource = (firingHand == MSCO::Magic::Hand::Right)
                                                                     ? RE::MagicSystem::CastingSource::kRightHand
                                                                     : RE::MagicSystem::CastingSource::kLeftHand;
           //start locking 
           const auto lockvar = (currentSource == RE::MagicSystem::CastingSource::kRightHand) ? "MSCO_right_lock"sv : "MSCO_left_lock"sv;
           if (!actor->SetGraphVariableInt(lockvar, 1)) {
               log::warn("[AnimEventFramework] {}: failed to set {} to 1", tag.data(), lockvar);
           }
            const auto otherHand =
                (firingHand == MSCO::Magic::Hand::Right) ? MSCO::Magic::Hand::Left : MSCO::Magic::Hand::Right;

            if (auto* otherItem = GetCurrentlyCastingMagicItem(actor, otherHand)) {
                if (otherItem->GetCastingType() == RE::MagicSystem::CastingType::kConcentration) {
                    InterruptHand(actor, otherHand);
                    if (LogEnabled()) log::info("[AnimEventFramework] {}: Interrupted Other Hand due to concentration spell", tag.data());
                }
            }
            if (!IsConvertChargeTime()) return false;
            //set charge time
            RE::MagicItem* CurrentSpell = GetEquippedMagicItemForHand(actor, firingHand);
            if (!CurrentSpell) {
                log::warn("[AnimEventFramework] {}: No CurrentSpell", tag.data());
                return false;
            }
            float speed = ConvertSpeed(actor, CurrentSpell->GetChargeTime(), tag, IsExpMode());
            if (LogEnabled()) log_chargetime(tag, actor, CurrentSpell->GetChargeTime(), speed);
            /*const char* actorName = actor->GetName();
            if (!actorName || actorName[0] == '\0') {actorName = "<unnamed actor>";}
            log::info("[AnimEventFramework] {} | actor='{}' | chargeTime={:.3f} | speed={:.3f}", tag.data(), actorName, CurrentSpell->GetChargeTime(), speed); */
            if (!actor->SetGraphVariableFloat("MSCO_attackspeed", speed)) { 
                log::warn("[AnimEventFramework] {}: failed to setMSCO_attackspeed to {}", tag.data(), speed);
            }
            return false;
        }

        if (tag == "Dual_MSCOStart"sv) {
            if (!actor->SetGraphVariableInt("MSCO_right_lock"sv, 1)) {
                log::warn("[AnimEventFramework] {}: failed to set MSCO_right_lock to 1", tag.data());
            }
            if (!actor->SetGraphVariableInt("MSCO_left_lock"sv, 1)) {
                log::warn("[AnimEventFramework] {}: failed to set MSCO_right_lock to 1", tag.data());
            }
            if (!IsConvertChargeTime()) return false;
            // set charge time
            RE::MagicItem* CurrentSpell = GetEquippedMagicItemForHand(actor, MSCO::Magic::Hand::Left);
            if (!CurrentSpell) {
                log::warn("[AnimEventFramework] {}: No CurrentSpell", tag.data());
                return false;
            }
            float speed = ConvertSpeed(actor, CurrentSpell->GetChargeTime(), tag, IsExpMode());
            /*const char* actorName = actor->GetName();
            if (!actorName || actorName[0] == '\0') {actorName = "<unnamed actor>";}
            log::info("[AnimEventFramework] {} | actor='{}' | chargeTime={:.3f} | speed={:.3f}", tag.data(), actorName, CurrentSpell->GetChargeTime(), speed);*/
            if (LogEnabled()) log_chargetime(tag, actor, CurrentSpell->GetChargeTime(), speed);
            if (!actor->SetGraphVariableFloat("MSCO_attackspeed", speed)) {
                log::warn("[AnimEventFramework] {}: failed to setMSCO_attackspeed to {}", tag.data(), speed);
            }
            return false;
        }
        
        if (tag == "LR_MSCOStart"sv) {
            if (!actor->SetGraphVariableInt("MSCO_right_lock"sv, 1)) {
                log::warn("[AnimEventFramework] {}: failed to set MSCO_right_lock to 1", tag.data());
            }
            if (!actor->SetGraphVariableInt("MSCO_left_lock"sv, 1)) {
                log::warn("[AnimEventFramework] {}: failed to set MSCO_right_lock to 1", tag.data());
            }
            if (!IsConvertChargeTime()) return false;
            // set charge time
            RE::MagicItem* leftSpell = GetEquippedMagicItemForHand(actor, MSCO::Magic::Hand::Left);
            RE::MagicItem* rightSpell = GetEquippedMagicItemForHand(actor, MSCO::Magic::Hand::Right);
            if (!leftSpell || !rightSpell) {
                log::warn("[AnimEventFramework] {}: no LeftSpell or no RightSpell", tag.data());
                return false;
            }
            //average the charge times and then plug that shit in
            const float leftChargeTime = leftSpell->GetChargeTime();
            const float rightChargeTime = rightSpell->GetChargeTime();
            const float averageChargeTime = (leftChargeTime + rightChargeTime) / 2;
            const float speed = ConvertSpeed(actor, averageChargeTime, tag, IsExpMode());
            /*const char* actorName = actor->GetName();
            if (!actorName || actorName[0] == '\0') {actorName = "<unnamed actor>";}
            log::info("[AnimEventFramework] {} | actor='{}' | chargeTime={:.3f} | speed={:.3f}", tag.data(), actorName, averageChargeTime, speed);*/
            if (LogEnabled()) log_chargetime(tag, actor, averageChargeTime, speed);
            if (!actor->SetGraphVariableFloat("MSCO_attackspeed", speed)) {
                log::warn("[AnimEventFramework] {}: failed to setMSCO_attackspeed to {}", tag.data(), speed);
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
            /*caster->ClearMagicNode();
            log::info("InterruptedCast");*/
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