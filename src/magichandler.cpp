#include "PCH.h"

#include "magichandler.h"
#include "settings.h"

using namespace SKSE;
using namespace SKSE::log;
using namespace SKSE::stl;

namespace MSCO::Magic {
    static bool LogEnabled() { return settings::Get().log; }

    // log state
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
                return "kReady";
            case S::kUnk04:
                return "Unk04/kRelease";
            case S::kCharging:
                return "kCharging/kUnk05";
            case S::kCasting:
                return "kCasting/kConcentrating";
            case S::kUnk07:
                return "Unk07";
            case S::kUnk08:
                return "kUnk07/Interrupt";
            case S::kUnk09:
                return "kUnk09/Interrupt/Deselect";
            default:
                return "Unknown";
        }
    }

    static const char* SafeActorName(const RE::Actor* a) {
        if (!a) return "<null actor>";
        const char* n = a->GetName();
        return (n && n[0]) ? n : "<unnamed>";
    }

    static std::string_view SafeSpellName(const RE::MagicItem* m) {
        if (!m) return "<null spell>";
        // GetFullName returns const char* on TESForm-derived, but MagicItem is TESForm-derived in practice.
        const char* n = m->GetFullName();
        return (n && n[0]) ? std::string_view{n} : std::string_view{"<noname spell>"};
    }

    static std::string_view convertCastingType(RE::MagicSystem::CastingType type) {
        using T = RE::MagicSystem::CastingType;
        switch (type) {
            case T::kConstantEffect:
                return "kConstantEffect";
            case T::kFireAndForget:
                return "kFireAndForget";
            case T::kConcentration:
                return "kConcentration";
            case T::kScroll:
                return "kScroll";
            default:
                return "Unknown";
        }
    }

    static std::string_view convertSpellType(RE::MagicSystem::SpellType type) {
        using T = RE::MagicSystem::SpellType;

        switch (type) {
            case T::kSpell:
                return "Spell";
            case T::kDisease:
                return "Disease";
            case T::kPower:
                return "Power";
            case T::kLesserPower:
                return "LesserPower";
            case T::kAbility:
                return "Ability";
            case T::kPoison:
                return "Poison";
            case T::kEnchantment:
                return "Enchantment";
            case T::kPotion:
                return "Potion";
            case T::kWortCraft:
                return "WortCraft";
            case T::kLeveledSpell:
                return "LeveledSpell";
            case T::kAddiction:
                return "Addiction";
            case T::kVoicePower:
                return "VoicePower";
            case T::kStaffEnchantment:
                return "StaffEnchantment";
            case T::kScroll:
                return "Scroll";
            default:
                return "Unknown";
        }
    }

    //grab spell casting hand source by flag
    RE::MagicSystem::CastingSource HandToSource(Hand hand) {
        switch (hand) {
            case Hand::Right:
                return RE::MagicSystem::CastingSource::kRightHand;
            case Hand::Left:
                return RE::MagicSystem::CastingSource::kLeftHand;
            default:
                log::warn("[MagicHandler] HandtoSource() default case switch");
                return RE::MagicSystem::CastingSource::kRightHand;
        }
    }

    RE::MagicItem* GetEquippedSpellHand(RE::Actor* actor, Hand hand) { 
        if (!actor) {
            log::warn("[MagicHandler] null actor passed to GetEquippedSpellForHand()");
            return nullptr;
        }
        auto& rd = actor->GetActorRuntimeData();
        //std::size_t idx = static_cast<std::size_t>(hand);
        //std::size_t other = idx ^ 1;
        //RE::MagicItem* spell = nullptr;
        // try strict hand index first?
        //if (rd.selectedSpells[idx]) {
        //    spell = rd.selectedSpells[idx];
        //    //log::info("[MSCO] Using {}-hand spell: {}", hand == Hand::Right ? "right" : "left", spell->GetFullName());
        //} else if (rd.selectedSpells[other]) {
        //    //check other hand index
        //    spell = rd.selectedSpells[other];
        //    /*log::info("[MSCO] {} hand had no spell, using {} hand spell: {}", 
        //        hand == Hand::Right ? "right" : "left",
        //        hand == Hand::Right ? "left" : "right", spell->GetFullName());*/
        //} else {
        //    log::info("[MagicHandler] No spells in selectedSpells[0/1]");
        //    return nullptr;
        //}
        //return spell->As<RE::MagicItem>();

        const std::size_t idx = (hand == Hand::Left) ? 0 : 1;
        const std::size_t other = idx ^ 1;

        RE::MagicItem* spell = rd.selectedSpells[idx];
        if (!spell) {
            // Optional fallback
            spell = rd.selectedSpells[other];
            if (spell && LogEnabled()) {
                log::info("[MagicHandler] {}: {} hand empty, falling back to other hand spell '{}'",
                          SafeActorName(actor), (hand == Hand::Left) ? "Left" : "Right", SafeSpellName(spell));
            }
        }

        if (!spell) {
            if (LogEnabled()) log::info("[MagicHandler] {}: No spells in selectedSpells[0/1]", SafeActorName(actor));
            return nullptr;
        }
        if (LogEnabled()) {
            const auto castingType = spell->GetCastingType();
            const auto spellType = spell->GetSpellType();
            log::info("[MagicHandler] equipped spell = '{}', casting type = {}, spell type = {}", 
                SafeSpellName(spell), convertCastingType(castingType), convertSpellType(spellType));
        }
        return spell;
    }

    bool ConsumeResource(RE::MagicSystem::CastingSource source, RE::Actor* actor, RE::MagicItem* spell, bool dualCast, float costmult) {
        if (!actor || !spell) {
            log::warn("[consumeResource] {}: no ActorValueOwner", SafeActorName(actor)); return false;
        }

        auto* actorAV = actor->AsActorValueOwner();
        if (!actorAV) {
            log::warn("[consumeResource] {}: no ActorValueOwner", SafeActorName(actor)); return false;
        }

        //check staff/enchantment first
        if (auto* enchItem = spell->As<RE::EnchantmentItem>()) {
            const bool isLeftHand = (source == RE::MagicSystem::CastingSource::kLeftHand);

            auto* equippedObj = actor->GetEquippedObject(isLeftHand);
            if (!equippedObj) {
                log::warn("[consumeResource] {}: enchantment cast but no equipped object ({})", 
                    SafeActorName(actor), isLeftHand ? "L" : "R");
                return false;
            }

            auto* weapon = equippedObj->As<RE::TESObjectWEAP>();
            if (!weapon) {
                log::warn("[consumeResource] {}: enchantment cast but equipped object not a weapon ({})", 
                    SafeActorName(actor), isLeftHand ? "L" : "R");
                return false;
            }

            if (!weapon->IsStaff()) {
                log::warn("[consumeResource] {}: enchantment item but weapon is not staff ({})",
                    SafeActorName(actor), weapon->GetFullName());
                return false;
            }
            float cost = enchItem->CalculateMagickaCost(actor);
            if (dualCast) cost = RE::MagicFormulas::CalcDualCastCost(cost);
            cost *= costmult;
            if (cost < 0.0f) cost = 0.0f;
            const auto avToDamage = isLeftHand ? RE::ActorValue::kLeftItemCharge : RE::ActorValue::kRightItemCharge;
            if (cost > 0.0f) actorAV->DamageActorValue(avToDamage, cost);
            if (LogEnabled()) {
                log::info("[consumeResource] {}: Staff '{}' cost={:.3f} (mult={:.3f})", 
                    SafeActorName(actor), weapon->GetFullName(), cost, costmult);
            }
            return true;
        }
        //assume not staff -> magicka spell
        float cost = spell->CalculateMagickaCost(actor);
        if (dualCast) cost = RE::MagicFormulas::CalcDualCastCost(cost);
        if (cost > 0.0f) actorAV->DamageActorValue(RE::ActorValue::kMagicka, cost);
        if (LogEnabled()) {
            log::info("[consumeResource] {}: Spell '{}' cost={:.3f} (mult={:.3f})", 
                SafeActorName(actor), SafeSpellName(spell), cost, costmult);
        }
        return true;
        //auto* enchItem = spell->As<RE::EnchantmentItem>();
        //const bool isEnchantment = enchItem != nullptr;
        //if (isEnchantment) {  // assume it's a staff in this case
        //    const bool isLeftHand = source == RE::MagicSystem::CastingSource::kLeftHand;
        //    if (auto* equippedObj = actor->GetEquippedObject(isLeftHand)) {
        //        if (auto* weapon = equippedObj->As<RE::TESObjectWEAP>()) {
        //            if (!weapon->IsStaff()) {
        //                log::warn("[consumeResource] Not staff, ignore");
        //                return false;
        //            }
        //        } else {
        //            log::warn("[consumeResource] No Weapon on Enchantment, ignore");
        //            return false;
        //        }
        //    }
        //    float spellCost = enchItem->CalculateMagickaCost(actor);
        //    // float spellCost = static_cast<float>(enchItem->data.costOverride);

        //    if (dualCast) {  // staves dont dual cast but I might want this feature someday
        //        spellCost = RE::MagicFormulas::CalcDualCastCost(spellCost);
        //    }
        //    spellCost = spellCost * costmult;

        //    RE::ActorValueOwner* actorAV = actor->AsActorValueOwner();
        //    if (!actorAV) {
        //        log::warn("[consumeResource] No actor value");
        //        return false;
        //    }

        //    if (spellCost > 0.0f) {
        //        auto avToDamage = (isLeftHand) ? RE::ActorValue::kLeftItemCharge : RE::ActorValue::kRightItemCharge;
        //        actorAV->DamageActorValue(avToDamage, spellCost);
        //    }
        //    return true;

        //} else {
        //    RE::ActorValueOwner* actorAV = actor->AsActorValueOwner();
        //    if (!actorAV) {
        //        log::warn("[consumeResource] No actor value");
        //        return false;
        //    }
        //    float spellCost = spell->CalculateMagickaCost(actor);

        //    if (dualCast) {
        //        spellCost = RE::MagicFormulas::CalcDualCastCost(spellCost);
        //    }

        //    spellCost = spellCost * costmult;  // apply costmult param
        //    if (spellCost < 0.0f) spellCost = 0.0f;
        //    float curMagicka = actorAV->GetActorValue(RE::ActorValue::kMagicka);
        //    if (spellCost > 0.0f && curMagicka < spellCost) {
        //        // log::info("cannot cast has {} magicka < {} cost", curMagicka, spellCost);
        //        //RE::HUDMenu::FlashMeter(RE::ActorValue::kMagicka);
        //        return false;
        //    }
        //    if (spellCost > 0.0f) actorAV->DamageActorValue(RE::ActorValue::kMagicka, spellCost);
        //    return true;
        //}
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
        if (!caster || !RequestCastImpl) return;
        const auto* actor = caster->GetCasterAsActor();
        if (!actor) {
            RequestCastImpl(caster);
            return;
        }
        if (!actor->IsPlayerRef()) {
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
            //if (state >= RE::MagicCaster::State::kUnk02) {return;}
            if (ShouldDenyRequestCast(state)) {
                if (LogEnabled()) {
                    log::info("[MagicHandler] Deny RequestCast: actor='{}' spell='{}' state={}", 
                        SafeActorName(actor), SafeSpellName(spell), convert_state(state));
                }
                return;
            }
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