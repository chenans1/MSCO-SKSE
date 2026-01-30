#include "PCH.h"

#include "AnimEventFramework.h"
#include "magichandler.h"   
#include "PayloadHandler.h"
#include "settings.h"
#include "utils.h"
using namespace SKSE;
using namespace SKSE::log;
using namespace SKSE::stl;

namespace MSCO {
    // logging the charge time
    static void log_chargetime(RE::BSFixedString tag, RE::Actor* actor, float chargetime, float speed) {
        const char* actorName = actor->GetName();
        if (!actorName || actorName[0] == '\0') {
            actorName = "<unnamed actor>";
        }
        log::info("[AnimEventFramework] {} | actor='{}' | chargeTime={:.3f} | speed={:.3f}", 
            tag.data(), actorName, chargetime, speed);
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
            if (!actorCaster) {
                log::warn("[AnimEventFramework] replaceNode: null ActorMagicCaster slot={}", slot); return;
            }

            auto* obj = root->GetObjectByName(nodeName);
            auto* replacement_node = obj ? const_cast<RE::NiAVObject*>(obj)->AsNode() : nullptr;

            if (!replacement_node) {
                log::warn("[AnimEventFramework] replaceNode: failed to resolve replacement_node '{}' (resolved='{}')", 
                    nodeName, utils::SafeNodeName(obj));
                return;
            }
            actorCaster->magicNode = replacement_node;
            if (settings::IsLogEnabled()) {
                log::info("[AnimEventFramework] replaceNode: actor='{}' original source ={} output_node='{}' ptr={}",
                          utils::SafeActorName(actor), utils::ToString(ogSource), utils::SafeNodeName(replacement_node), fmt::ptr(replacement_node));
            }
            return;
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
        expFactor = std::clamp(expFactor, 0.1f, 1.0f);

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
            log::warn("[AnimEventFramework] {}: ConvertSpeed: null actor", tag.data());
            return 1.0f;
        }
        //auto cfg = getCFG();
        const auto cfg = settings::GetChargeSpeedCFG();
        float speed = 1.0f;
        if (expMode) {
            speed = getSpeedExp(chargeTime, cfg.shortest, cfg.longest, cfg.baseTime, cfg.minSpeed, cfg.maxSpeed, cfg.expFactor);
        } else {
            speed = getSpeed(chargeTime, cfg.shortest, cfg.longest, cfg.baseTime, cfg.minSpeed, cfg.maxSpeed);
        }

        if (!actor->IsPlayerRef()) {
            speed = std::clamp(speed*settings::GetNPCFactor(), cfg.minSpeed, cfg.maxSpeed);
        }
        return speed;
    }

    //also sets the input bool to true/false
    static bool IsBeginCast(const RE::BSFixedString& tag, bool& leftHand) {
        if (tag == "BeginCastRight"sv) {
            leftHand = false;
            return true;
        }
        if (tag == "BeginCastLeft"sv) {
            leftHand = true;
            return true;
        }
        return false;
    }

    static bool IsMSCOStart(const RE::BSFixedString& tag, bool& leftHand) {
        if (tag == "RightMSCOStart"sv) {
            leftHand = false;
            return true;
        }
        if (tag == "LeftMSCOStart"sv) {
            leftHand = true;
            return true;
        }
        return false;
    }

    /*static bool isSpellFire(const RE::BSFixedString& tag, bool& leftHand) {
        if (tag == "MRh_SpellFire_Event"sv) {
            leftHand = false;
            return true;
        }
        if (tag == "MLh_SpellFire_Event"sv) {
            leftHand = true;
            return true;
        }
        return false;
    }*/

    /*static bool shouldUnlock() {
        const float pUnlock = 0.7f;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(rng) < std::clamp(pUnlock, 0.0f, 1.0f);
    }
    static bool shouldLockOther() {
        const float pUnlock = 0.25f;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(rng) < std::clamp(pUnlock, 0.0f, 1.0f);
    }
    static bool sameDeliveryType(const RE::MagicItem* spell, const RE::MagicItem* otherSpell) {
        if (!spell || !otherSpell) {
            return false;
        }
        const auto spellType = spell->GetDelivery();
        const auto otherType = otherSpell->GetDelivery();
        return (spellType == otherType);
    }*/

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
        if (!settings::IsNPCAllowed()) {
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
        if (!settings::IsPlayerAllowed()) {
            return _originalPC(a_sink, a_event, a_eventSource);
        }
        if (HandleEvent(a_event)) {
            return RE::BSEventNotifyControl::kContinue;
        }
        return _originalPC(a_sink, a_event, a_eventSource);
    }    

    bool AnimEventHook::HandleEvent(RE::BSAnimationGraphEvent* a_event) {
        if (!a_event || !a_event->holder || !a_event->tag.data()) return false;
        auto* holder = const_cast<RE::TESObjectREFR*>(a_event->holder);
        if (!holder) return false;
        auto* actor = holder ? holder->As<RE::Actor>() : nullptr;
        if (!actor) return false;
        if (!utils::HasActorTypeNPC(actor)) { //check if it's a humanoid player if not return
            return false;
        }
        //log::info("HandleEvent Called");
        const auto& tag = a_event->tag;
        const auto& payload = a_event->payload;

        //if (tag == "MSCO_MagicReady"sv) {
        //    const RE::MagicItem* leftSpell = MSCO::Magic::GetEquippedSpell(actor, true);
        //    const RE::MagicItem* rightSpell = MSCO::Magic::GetEquippedSpell(actor, false);
        //    //auto& rd = actor->GetActorRuntimeData();
        //    if (leftSpell) {
        //        if (auto* left_caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kLeftHand)) {
        //            //InterruptHand(actor, MSCO::Magic::Hand::Left);
        //            left_caster->ClearMagicNode();
        //            //if (settings::IsLogEnabled()) log::info("[AnimEventFramework] {}: leftCaster->clearmagicnode()", tag.data());
        //        }
        //        //actor->NotifyAnimationGraph("MLh_Equipped_Event"sv);
        //    }
        //    if (rightSpell) {
        //        if (auto* right_caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kRightHand)) {
        //            //InterruptHand(actor, MSCO::Magic::Hand::Right);
        //            right_caster->ClearMagicNode();
        //            //if (settings::IsLogEnabled()) log::info("[AnimEventFramework] {}: rightCaster->clearmagicnode()", tag.data());
        //        }
        //        //actor->NotifyAnimationGraph("MRh_Equipped_Event"sv);
        //    }
        //    return false;
        //}

        //make sure to interrupt and reset if we exit the state except if it's two-handed, assume vanilla handles that properly?
        if (tag == "CastingStateExit"sv) {
            const RE::MagicItem* leftSpell = MSCO::Magic::GetEquippedSpell(actor, true);
            const RE::MagicItem* rightSpell = MSCO::Magic::GetEquippedSpell(actor, false);
            //bool shouldInterrupt = false;
            if (leftSpell) {
                if (leftSpell->IsTwoHanded()) return false;
                if (auto* left_caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kLeftHand)) {
                    left_caster->ClearMagicNode();
                }
                MSCO::Magic::InterruptCaster(actor, false);
                //shouldInterrupt = true;
                actor->NotifyAnimationGraph("MLh_Equipped_Event"sv);
                
            }
            if (rightSpell) {
                if (auto* right_caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kRightHand)) {
                    right_caster->ClearMagicNode();
                }
                MSCO::Magic::InterruptCaster(actor, true);
                //shouldInterrupt = true;
                actor->NotifyAnimationGraph("MRh_Equipped_Event"sv);
            }
            /*actor->InterruptCast(true);*/
            /*if (shouldInterrupt) {
                actor->InterruptCast(true);
            }*/
            return false;
        }

        if (tag == "MSCO_LR_ready"sv && actor->IsPlayerRef()) {
            actor->SetGraphVariableInt("MSCO_right_lock"sv, 0);
            actor->SetGraphVariableInt("MSCO_left_lock"sv, 0);
            return false;
        }

        if (tag == "MSCO_WinOpen"sv && !actor->IsPlayerRef()) {
            actor->SetGraphVariableInt("MSCO_right_lock"sv, 0);
            actor->SetGraphVariableInt("MSCO_left_lock"sv, 0);
            return false;
        }

        //on MCO_WinOpen/BFCO_WinOpen Allow for the casting?
        if (tag == "MCO_WinOpen"sv || tag == "MCO_PowerWinOpen"sv || tag=="BFCO_NextWinStart"sv || tag=="BFCO_NextPowerWinStart"sv) {
            actor->NotifyAnimationGraph("CastOkStart"sv);
            if (settings::IsLogEnabled()) log::info("[AnimEventFramework] {}: CastOkStart Notified", tag.data());
            return false;
        }

        //spellfire event handlers->just handles source replacement
        if (tag == "MRh_SpellFire_Event"sv) {
            if (!utils::GetGraphBool(actor, "bIsMSCO")) return false;
            auto* caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kRightHand);
            if (!caster) {
                log::warn("{}: No MagicCaster", tag.data());
                return false;
            }
            //caster->ClearMagicNode();
            replaceNode(actor, RE::MagicSystem::CastingSource::kRightHand, RE::MagicSystem::CastingSource::kRightHand);
            const auto casterState = caster->state.get();
            if (casterState < RE::MagicCaster::State::kReady || casterState > RE::MagicCaster::State::kCharging) {
                log::info("[MRh_SpellFire]: {} CasterState = {}", utils::SafeActorName(actor), utils::ToString(casterState));
                return false;
            }
            if (!utils::GetGraphBool(actor, "bMSCO_LRCasting") && actor->IsPlayerRef()) {
                actor->SetGraphVariableInt("MSCO_right_lock"sv, 0);
                actor->SetGraphVariableInt("MSCO_left_lock"sv, 0);
            }
            const RE::MagicItem* CurrentSpell = MSCO::Magic::GetEquippedSpell(actor, false);
            if (!CurrentSpell) { log::warn("No CurrentSpell"); return false; }
            const auto parsed = MSCO::ParseSpellFire(payload);
            auto output_source = parsed.src.value_or(RE::MagicSystem::CastingSource::kRightHand);
            MSCO::Magic::ConsumeResource(RE::MagicSystem::CastingSource::kRightHand, actor, CurrentSpell, false, 1.0f);
            if (RE::MagicSystem::CastingSource::kRightHand != output_source) {  // only replace if it's different
                replaceNode(actor, RE::MagicSystem::CastingSource::kRightHand, output_source);
            }
            return false;
        }

        if (tag == "MLh_SpellFire_Event"sv) {
            if (!utils::GetGraphBool(actor, "bIsMSCO")) return false;
            const bool isDualCasting = utils::GetGraphBool(actor, "bMSCODualCasting");
            auto* caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kLeftHand);
            if (!caster) {
                log::warn("{}: No MagicCaster", tag.data());
                return false;
            }
            //caster->ClearMagicNode();
            replaceNode(actor, RE::MagicSystem::CastingSource::kLeftHand, RE::MagicSystem::CastingSource::kLeftHand);
            const auto casterState = caster->state.get();
            if (casterState < RE::MagicCaster::State::kReady || casterState > RE::MagicCaster::State::kCharging) {
                log::info("[MLh_SpellFire]: {} CasterState = {}", utils::SafeActorName(actor), utils::ToString(casterState));
                return false;
            }
            if (!utils::GetGraphBool(actor, "bMSCO_LRCasting") && actor->IsPlayerRef()) {
                actor->SetGraphVariableInt("MSCO_right_lock"sv, 0);
                actor->SetGraphVariableInt("MSCO_left_lock"sv, 0);
            }
            const RE::MagicItem* CurrentSpell = MSCO::Magic::GetEquippedSpell(actor, true);
            if (!CurrentSpell) { log::warn("No CurrentSpell"); return false; }
            const auto parsed = ParseSpellFire(payload);
            auto output_source = parsed.src.value_or(RE::MagicSystem::CastingSource::kLeftHand);
            MSCO::Magic::ConsumeResource(RE::MagicSystem::CastingSource::kLeftHand, actor, CurrentSpell, isDualCasting, 1.0f);
            if (RE::MagicSystem::CastingSource::kLeftHand != output_source) {  // only replace if it's different
                replaceNode(actor, RE::MagicSystem::CastingSource::kLeftHand, output_source);
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
            if (!settings::IsConvertChargeTime()) return false;
            // set charge time
            const RE::MagicItem* CurrentSpell = MSCO::Magic::GetEquippedSpell(actor, true);
            if (!CurrentSpell) {
                log::warn("[AnimEventFramework] {}: No CurrentSpell", tag.data());
                return false;
            }
            float speed = ConvertSpeed(actor, CurrentSpell->GetChargeTime(), tag, settings::IsExpMode());
            if (settings::IsLogEnabled()) log_chargetime(tag, actor, CurrentSpell->GetChargeTime(), speed);
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

            if (!settings::IsConvertChargeTime()) return false;
            const RE::MagicItem* leftSpell = MSCO::Magic::GetEquippedSpell(actor, true);
            const RE::MagicItem* rightSpell = MSCO::Magic::GetEquippedSpell(actor, false);
            if (!leftSpell || !rightSpell) {
                log::warn("[AnimEventFramework] {}: no LeftSpell or no RightSpell", tag.data());
                return false;
            }
            // average the charge times and then plug that shit in
            const float leftChargeTime = leftSpell->GetChargeTime();
            const float rightChargeTime = rightSpell->GetChargeTime();
            const float averageChargeTime = (leftChargeTime + rightChargeTime) / 2;
            const float speed = ConvertSpeed(actor, averageChargeTime, tag, settings::IsExpMode());
            if (settings::IsLogEnabled()) log_chargetime(tag, actor, averageChargeTime, speed);
            if (!actor->SetGraphVariableFloat("MSCO_attackspeed", speed)) {
                log::warn("[AnimEventFramework] {}: failed to setMSCO_attackspeed to {}", tag.data(), speed);
            }
            return false;
        }

        bool isLeft{};
        // interrupt conc spell in other hand if applicable. Also: Fallback state set. Will compute charge time here
        if (IsMSCOStart(tag, isLeft)) {
            // log::info("msco casting event detected");
            // start locking
            const auto lockvar = isLeft ? "MSCO_lock_left"sv : "MSCO_lock_right"sv;
            if (!actor->SetGraphVariableInt(lockvar, 1)) {
                log::warn("[AnimEventFramework] {}: failed to set {} to 1", tag.data(), lockvar);
            }
            //interrupt concentration spells in the other hand
            if (const auto* otherItem = MSCO::Magic::GetCastingSpell(actor, isLeft ? false : true)) {
                if (otherItem->GetCastingType() == RE::MagicSystem::CastingType::kConcentration) {
                    MSCO::Magic::InterruptCaster(actor, isLeft ? false : true);
                    actor->NotifyAnimationGraph(isLeft ? "MRh_Equipped_Event"sv : "MLh_Equipped_Event"sv);
                    if (settings::IsLogEnabled()) {
                        log::info("[AnimEventFramework] {}: Interrupted Other Hand due to concentration spell",tag.data());
                    }
                }
            }
            // set charge time
            if (!settings::IsConvertChargeTime()) return false;
            const RE::MagicItem* CurrentSpell = MSCO::Magic::GetEquippedSpell(actor, isLeft);
            if (!CurrentSpell) {
                log::warn("[AnimEventFramework] {}: No CurrentSpell", tag.data());
                return false;
            }
            float speed = ConvertSpeed(actor, CurrentSpell->GetChargeTime(), tag, settings::IsExpMode());
            if (settings::IsLogEnabled()) log_chargetime(tag, actor, CurrentSpell->GetChargeTime(), speed);
            if (!actor->SetGraphVariableFloat("MSCO_attackspeed", speed)) {
                log::warn("[AnimEventFramework] {}: failed to setMSCO_attackspeed to {}", tag.data(), speed);
            }
            return false;
        }
        //handles begin cast and sending the MSCO animevents
        if (IsBeginCast(tag, isLeft)) {
            /*const char* lockName = isLeft ? "MSCO_left_lock":"MSCO_right_lock";
            std::int32_t lock = 0;
            const bool ok = actor->GetGraphVariableInt(lockName, lock);
            if (ok && lock != 0) return false;*/  // swallow the BeginCastLeft/Right Event
            if (actor->IsSneaking()) {
                return false;  // check if we are sneaking nor not - don't do anything if we are
            }
            //if npc not in combat also default to vanilla behaviors
            if (!actor->IsInCombat() && !actor->IsPlayerRef()) {
                if (settings::IsLogEnabled()) log::info("[AnimEventFramework] non-combat NPC spell casting - normal behaviors");
                return false;
            }
            if (utils::GetGraphBool(actor, "IsFirstPerson")) return false; //dont affect first person
            const auto* CurrentSpell = MSCO::Magic::GetEquippedSpell(actor, isLeft);
            if (!CurrentSpell) {log::warn("No CurrentSpell"); return false;}
            //GOING TO IGNORE RITUAL SPELLS FOR NOW
            if (CurrentSpell->IsTwoHanded()) {
                if (settings::IsLogEnabled()) log::info("[AnimEventFramework] {}: Ritual Spell, Ignore", tag.data()); return false;
            }
            
            const auto CastingType = CurrentSpell->GetCastingType();
            if (CastingType != RE::MagicSystem::CastingType::kFireAndForget && CastingType != RE::MagicSystem::CastingType::kScroll) {
                if (settings::IsLogEnabled()) {
                    log::info("[AnimEventFramework] {}: '{}', CastingType = {} spell ignored", 
                    tag.data(), utils::SafeSpellName(CurrentSpell), utils::ToString(CastingType));
                }
                return false;
            }

            if (settings::IsLogEnabled()) {
                log::info("[AnimEventFramework] {}: '{}', CastingType = {}", tag.data(), utils::SafeSpellName(CurrentSpell), utils::ToString(CastingType));
            }

            auto magicCaster = actor->GetMagicCaster(isLeft ? RE::MagicSystem::CastingSource::kLeftHand : RE::MagicSystem::CastingSource::kRightHand);
            if (!magicCaster) {
                log::warn("[AnimEventFramework] {}: No Magic Caster", tag.data());
                return false;
            }
            if (!actor->SetGraphVariableInt("iMSCO_ON"sv, 1)) {
                log::warn("[AnimEventFramework] {}: set iMSCO_ON == 1 failed", tag.data());
            }
            
            //turns out the game actually already sets the dual casting value correctly by the time we intercept begincastleft
            const bool wantDualCasting = magicCaster->GetIsDualCasting();
            if (wantDualCasting) { 
                actor->NotifyAnimationGraph("MSCO_start_dual"sv);
            } else {
                // need to grab the other hand, check if it's fnf in order to handle sending the LR event properly
                const RE::MagicItem* otherSpell = MSCO::Magic::GetEquippedSpell(actor, isLeft ? false : true);
                const bool otherisFNF =
                    otherSpell && otherSpell->GetCastingType() == RE::MagicSystem::CastingType::kFireAndForget;
                bool shouldLR = false;
                if (otherisFNF) {
                    if (auto* otherCaster = actor->GetMagicCaster(isLeft ? RE::MagicSystem::CastingSource::kRightHand
                                                                         : RE::MagicSystem::CastingSource::kLeftHand)) {
                        shouldLR = (otherCaster->state.get() >= RE::MagicCaster::State::kReady);
                    }
                }
                actor->NotifyAnimationGraph(shouldLR ? "MSCO_start_lr"sv : (isLeft ? "MSCO_start_left"sv : "MSCO_start_right"sv));
                //make npcs spam lr slightly less frequently
                /*if (!shouldLR && !actor->IsPlayerRef() && shouldLockOther) {
                    const auto lockvar = isLeft ? "MSCO_lock_right"sv : "MSCO_lock_left"sv;
                    if (!actor->SetGraphVariableInt(lockvar, 1)) {
                        log::warn("[AnimEventFramework] {}: failed to set {} to 1", tag.data(), lockvar);
                    }
                    log::info("{}: NPC {} locked other ({}) hand", tag.data(), utils::SafeActorName(actor), isLeft ? "right":"left");
                }*/
                //lock other if it's a diff delivery type situation
            }
            magicCaster->castingTimer = 0.00f;
            if (!actor->IsPlayerRef()) magicCaster->state = RE::MagicCaster::State::kUnk04;
            return false;
        }
        return false;
    }
}