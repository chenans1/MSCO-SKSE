#include "PCH.h"

#include "magichandler.h"
#include "settings.h"
#include "utils.h"

using namespace SKSE;
using namespace SKSE::log;
using namespace SKSE::stl;

namespace MSCO::Magic {
    bool ConsumeResource(RE::MagicSystem::CastingSource source, RE::Actor* actor, const RE::MagicItem* spell, bool dualCast, float costmult) {
        if (!actor || !spell) {
            log::warn("[consumeResource]: no actor or no spell"); return false;
        }
        //check here for spelltype being scroll
        const auto spellType = spell->GetSpellType();
        if (spellType == RE::MagicSystem::SpellType::kScroll) {
            if (settings::IsLogEnabled()) log::info("[consumeResource]: '{}' is kScroll, ignore", utils::SafeSpellName(spell));
            return true;
        }
        if (spellType != RE::MagicSystem::SpellType::kSpell && spellType != RE::MagicSystem::SpellType::kStaffEnchantment) {
            log::warn("[consumeResource]: '{}' invalid spell type: {}", utils::SafeSpellName(spell), utils::ToString(spellType));
            return false;
        }

        auto* actorAV = actor->AsActorValueOwner();
        if (!actorAV) {
            log::warn("[consumeResource] {}: no ActorValueOwner", utils::SafeActorName(actor)); return false;
        }

        //check staff/enchantment first
        const auto* enchItem = spell->As<RE::EnchantmentItem>();
        const bool checkStaff = (spellType == RE::MagicSystem::SpellType::kStaffEnchantment) || (enchItem != nullptr);
        if (checkStaff) {
            if (!enchItem) {
                 log::warn("[consumeResource] {}: kStaffEnchantment type but item is not EnchantmentItem: '{}'. Falling back to magicka.",
                      utils::SafeActorName(actor), utils::SafeSpellName(spell));
            } else {
                const bool isLeftHand = (source == RE::MagicSystem::CastingSource::kLeftHand);
                /*auto* equippedObj = actor->GetEquippedObject(isLeftHand);
                if (!equippedObj) {
                    log::warn("[consumeResource] {}: enchantment cast but no equipped object ({})",
                              utils::SafeActorName(actor), isLeftHand ? "L" : "R");
                    return false;
                }

                auto* weapon = equippedObj->As<RE::TESObjectWEAP>();
                if (!weapon) {
                    log::warn("[consumeResource] {}: enchantment cast but equipped object not a weapon ({})",
                              utils::SafeActorName(actor), isLeftHand ? "L" : "R");
                    return false;
                }

                if (!weapon->IsStaff()) {
                    log::warn("[consumeResource] {}: enchantment item but weapon is not staff ({})",
                              utils::SafeActorName(actor), weapon->GetFullName());
                    return false;
                }*/
                float cost = enchItem->CalculateMagickaCost(actor);
                if (dualCast) cost = RE::MagicFormulas::CalcDualCastCost(cost);
                cost *= costmult;
                if (cost < 0.0f) cost = 0.0f;
                const auto avToDamage = isLeftHand ? RE::ActorValue::kLeftItemCharge : RE::ActorValue::kRightItemCharge;
                if (cost > 0.0f) actorAV->DamageActorValue(avToDamage, cost);
                /*if (settings::IsLogEnabled()) {
                    log::info("[consumeResource] {}: Staff '{}' cost={:.3f} (mult={:.3f})", 
                        utils::SafeActorName(actor),weapon->GetFullName(), cost, costmult);
                }*/
                if (settings::IsLogEnabled()) {
                    log::info("[consumeResource] {}: kStaffEnchantment: '{}' cost={:.3f} (mult={:.3f})",
                              utils::SafeActorName(actor), utils::SafeSpellName(enchItem), cost, costmult);
                }
                return true;
            }
            
        }
        //assume not staff/scroll here -> magicka spell, also the fallback
        float cost = spell->CalculateMagickaCost(actor);
        if (dualCast) cost = RE::MagicFormulas::CalcDualCastCost(cost);
        if (cost > 0.0f) actorAV->DamageActorValue(RE::ActorValue::kMagicka, cost);
        if (settings::IsLogEnabled()) {
            log::info("[consumeResource] {}: Spell '{}' cost={:.3f} (mult={:.3f})", 
                utils::SafeActorName(actor), utils::SafeSpellName(spell), cost, costmult);
        }
        return true;
    }
    
    //bool Spellfire(RE::MagicSystem::CastingSource source, RE::Actor* actor, RE::MagicItem* spell, bool dualCast, float magmult, RE::MagicSystem::CastingSource outputSource) {
    //    if (!actor) {
    //        log::warn("No Actor for spellfire()");
    //        return false;
    //    }
    //    if (!spell) {
    //        log::warn("No Spell for spellfire()");
    //        return false;
    //    }
    //    auto caster = actor->GetMagicCaster(source);
    //    if (!caster) {
    //        log::warn("Spellfire() called on {} has no MagicCaster for source {}", actor->GetName(),
    //                  std::to_underlying(source));
    //        return false;
    //    }
    //    auto& rd = actor->GetActorRuntimeData();
    //    const int slot = (source == RE::MagicSystem::CastingSource::kLeftHand) ? RE::Actor::SlotTypes::kLeftHand
    //                                                                           : RE::Actor::SlotTypes::kRightHand;
    //    auto* actorCaster = rd.magicCasters[slot];
    //    actorCaster->SetDualCasting(dualCast);  // i dont think this is necessary for normal dualcasting
    //    /*bool validCast = consumeResource(source, actor, spell, dualCast, costmult);
    //    if (!validCast) {
    //        log::info("[spellfire] invalid cast - dont fire");
    //        return false;
    //    }*/
    //    RE::Actor* target = nullptr;
    //    if (spell->GetDelivery() == RE::MagicSystem::Delivery::kSelf) {
    //        target = actor;  // self-targeted spells: target is the actor
    //    } else {
    //        // try combat target; if none, fall back to self
    //        target = rd.currentCombatTarget.get().get();
    //        if (!target) target = actor;
    //    }
    //    // send spell casting event
    //    if (auto ScriptEventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton()) {
    //        if (auto RefHandle = actor->CreateRefHandle()) {
    //            ScriptEventSourceHolder->SendSpellCastEvent(RefHandle.get(), spell->formID);
    //        }
    //    }
    //    // log::info("spellfire state: {}", logState2(actor, source));
    //    actorCaster->PrepareSound(
    //        RE::MagicSystem::SoundID::kRelease,
    //        spell);  // Doesn't seem to actually do anything, probably something to do with detection events?
    //    actorCaster->PlayReleaseSound(spell);  // idk what this does but maybe it sends a detection event? no clue
    //    RE::BGSSoundDescriptorForm* releaseSound = MSCO::Sound::GetMGEFSound(spell);
    //    if (releaseSound) MSCO::Sound::play_sound(actor, releaseSound);
    //    // apply damage buff i think
    //    float effectiveness = 1.0f;
    //    float magOverride = 0.0f;
    //    /*if (auto* effect = spell->GetCostliestEffectItem()) {
    //        magOverride = effect->GetMagnitude() * magmult;
    //    }*/
    //    // log::info("spellfire event: source={}, magOverride = {}", (source ==
    //    // RE::MagicSystem::CastingSource::kLeftHand) ? "leftHand" : "rightHand", magOverride);
    //    // test node
    //    /*valid node names:
    //    NPC L MagicNode [LMag]
    //    NPC R MagicNode [RMag]*/
    //    //if (auto root = actor->Get3D()) {
    //    //    static constexpr std::string_view NodeNames[2] = {"NPC L MagicNode [LMag]"sv, "NPC R MagicNode [RMag]"sv};
    //    //    const auto& nodeName = (outputSource == RE::MagicSystem::CastingSource::kLeftHand) ? NodeNames[0]   // LMag
    //    //                                                                                       : NodeNames[1];  // RMag
    //    //    auto bone = root->GetObjectByName(nodeName);
    //    //    if (auto output_node = bone->AsNode()) {
    //    //        actorCaster->magicNode = output_node;
    //    //        /*log::info("spellfire: replaced node");
    //    //        auto* node = actorCaster->GetMagicNode();
    //    //        LogNodeBasic(node, "ActorCaster magicNode");*/
    //    //    }
    //    //}
    //    // actorCaster->magicNode = RE::MagicSystem::CastingSource::kLeftHand;
    //    //actorCaster->state.set(RE::MagicCaster::State::k);
    //    actorCaster->CastSpellImmediate(spell,   // spell
    //                                    false,   // noHitEffectArt
    //                                    target,  // target
    //                                    1.0f,    // effectiveness
    //                                    false,   // hostileEffectivenessOnly
    //                                    0.0f,    // magnitudeOverride dunno what this does not sure
    //                                    actor    // cause (blame the caster so XP/aggro work)
    //    );
    //    log::info("spellfire event: source={}", (source == RE::MagicSystem::CastingSource::kLeftHand) ? "leftHand" : "rightHand");
    //    actorCaster->FinishCast();
    //    // caster->SetDualCasting(false);
    //    return true;
    //}

    //returns equipped spell item. nullptr if none
    const RE::MagicItem* GetEquippedSpell(RE::Actor* actor, bool leftHand) {
        if (!actor) {
            log::warn("[GetEquippedSpell] No Actor");
            return nullptr;
        }
        auto& rd = actor->GetActorRuntimeData();
        const auto slot = leftHand ? RE::Actor::SlotTypes::kLeftHand : RE::Actor::SlotTypes::kRightHand;
        const RE::MagicItem* spell = rd.selectedSpells[slot];
        if (!spell) {
            if (settings::IsLogEnabled())
                log::info("[GetEquippedSpell] {}: No spells", utils::SafeActorName(actor));
            return nullptr;
        }
        /*if (settings::IsLogEnabled()) {
            const auto castingType = spell->GetCastingType();
            const auto spellType = spell->GetSpellType();
            log::info("[GetEquippedSpellHand] {} equipped spell = '{}', casting type = {}, spell type = {}",
                      leftHand ? "Left Hand" : "Right Hand", utils::SafeSpellName(spell), utils::ToString(castingType), utils::ToString(spellType));
        }*/
        /*const auto castingType = spell->GetCastingType();
        const auto spellType = spell->GetSpellType();
        log::info("[GetEquippedSpell] {} equipped spell = '{}', casting type = {}, spell type = {}",
                  leftHand ? "Left Hand" : "Right Hand", utils::SafeSpellName(spell), utils::ToString(castingType),
                  utils::ToString(spellType));*/
        return spell;
    }

    //returns the casting spell
    const RE::MagicItem* GetCastingSpell(RE::Actor* actor, RE::MagicSystem::CastingSource source) {
        if (!actor) {
            log::warn("[GetCastingSpell] No Actor");
            return nullptr;
        }
        const RE::MagicCaster* magicCaster = actor->GetMagicCaster(source);
        if (!magicCaster) {
            log::warn("[GetCastingSpell] No magicCaster");
            return nullptr;
        }
        const auto spell = magicCaster->currentSpell;
        if (!spell) {
            if (settings::IsLogEnabled()) log::info("[GetCastingSpell] No magicCaster->spell");
            return nullptr;
        }
        /*const auto castingType = spell->GetCastingType();
        const auto spellType = spell->GetSpellType();
        log::info("[GetCastingSpell] {} casting spell = '{}', casting type = {}, spell type = {}", 
            utils::ToString(source), utils::SafeSpellName(spell), utils::ToString(castingType), utils::ToString(spellType));*/
        return spell;
    }

    // returns the casting spell
    const RE::MagicItem* GetCastingSpell(RE::Actor* actor, bool leftHand) {
        if (!actor) {
            log::warn("[GetCastingSpell] No Actor");
            return nullptr;
        }
        const auto source =
            leftHand ? RE::MagicSystem::CastingSource::kLeftHand : RE::MagicSystem::CastingSource::kRightHand;
        const RE::MagicCaster* magicCaster = actor->GetMagicCaster(source);
        if (!magicCaster) {
            log::warn("[GetCastingSpell] No magicCaster");
            return nullptr;
        }
        const auto spell = magicCaster->currentSpell;
        if (!spell) {
            if (settings::IsLogEnabled()) log::info("[GetCastingSpell] No magicCaster->spell");
            return nullptr;
        }
        /*const auto castingType = spell->GetCastingType();
        const auto spellType = spell->GetSpellType();
        log::info("[GetCastingSpell] {} casting spell = '{}', casting type = {}, spell type = {}",
                  utils::ToString(source), utils::SafeSpellName(spell), utils::ToString(castingType), utils::ToString(spellType));*/
        return spell;
    }

    void InterruptCaster(RE::Actor* actor, bool isLeft) {
        if (!actor) {
            log::info("[InterruptCaster] no actor");
            return;
        }
        if (auto* caster = actor->GetMagicCaster(isLeft ? RE::MagicSystem::CastingSource::kLeftHand : RE::MagicSystem::CastingSource::kRightHand)) {
            caster->InterruptCast(true);
            /*caster->ClearMagicNode();*/
            if (settings::IsLogEnabled()) log::info("InterruptedCast");
            return;
        }
    }

    //switched from using <= to this for more fine grained tuning maybe? seems it anything above kunk02 needs to be denied
    static bool ShouldDenyRequestCast(RE::MagicCaster::State state) {
        using S = RE::MagicCaster::State;

        switch (state) {
            //case S::kUnk01:
            case S::kUnk02: //start
            case S::kReady: //kReady
            case S::kUnk04: //kRelease
            case S::kCharging: //actually the concentration state i think?
            case S::kCasting: //Concentrating in IDA? seems like fnf also goes here for some reason
            case S::kUnk07: // Unknown
            //case S::kUnk08: // Interrupt
            //case S::kUnk09: // Interrupt/Deselect
                return true;

            default:
                return false;
        }
    }

    using RequestCast = void (*)(RE::ActorMagicCaster*);
    static inline RequestCast RequestCastImpl = nullptr;

    static void RequestCast_Hook(RE::ActorMagicCaster* caster) {
        if (!caster || !RequestCastImpl) {
            log::warn("RequestCastImpl() called with no caster/RequestCastImpl func pointer");
            return;
        }
        const auto* actor = caster->GetCasterAsActor();
        if (!actor) {
            log::warn("RequestCastImpl() could not fetch actor");
            RequestCastImpl(caster);
            return;
        }
        if (!actor->IsPlayerRef() || !settings::IsPlayerAllowed()) {
            RequestCastImpl(caster);
            return;
        }

        //check for casting type being conc or not
        const auto* spell = caster->currentSpell;
        if (!spell) {
            RequestCastImpl(caster);
            return;
        }
        const auto castingType = spell->GetCastingType();
        if (castingType == RE::MagicSystem::CastingType::kFireAndForget || castingType == RE::MagicSystem::CastingType::kScroll) {
            const auto state = caster->state.get();
            if (state >= RE::MagicCaster::State::kUnk02) {return;}
            /*if (ShouldDenyRequestCast(state)) {
                if (settings::IsLogEnabled()) {
                    log::info("[MagicHandler] Deny RequestCast: actor='{}' spell='{}' state={}", 
                        utils::SafeActorName(actor), utils::SafeSpellName(spell), utils::ToString(state));
                }
                return;
            }*/
        }
        RequestCastImpl(caster);
        return;
    }

    void Install() {
        REL::Relocation<std::uintptr_t> vtbl{RE::VTABLE_ActorMagicCaster[0]};
        // return original vfunc address?
        const std::uintptr_t orig = vtbl.write_vfunc(0x3, &RequestCast_Hook);
        // convert raw address -> function pointer
        RequestCastImpl = reinterpret_cast<RequestCast>(orig);
    }
}