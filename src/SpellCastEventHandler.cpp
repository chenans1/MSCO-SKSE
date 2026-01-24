#include "SpellCastEventHandler.h"
using namespace SKSE;
using namespace SKSE::log;
using namespace SKSE::stl;
using namespace std::literals;

namespace MSCO {
    RE::BSEventNotifyControl SpellCastEventHandler::ProcessEvent(
        const RE::TESSpellCastEvent* a_event,
        RE::BSTEventSource<RE::TESSpellCastEvent>* a_eventSource) {

        log::info("TESSpellCastEvent: handler entered (a_event={})", fmt::ptr(a_event));
        if (!a_event) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto* actor = a_event->object->As<RE::Actor>();
        if (!actor) {
            return RE::BSEventNotifyControl::kContinue;
        }

        /*auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(a_event->spell);
        const char* spellName = spell ? spell->GetName() : "Unknown";
        const char* actorName = actor->GetName();

        log::info("TESSpellCastEvent - Actor: {}, Spell: {} (FormID: 0x{:08X})", 
            actorName ? actorName : "Unnamed",
                  spellName, a_event->spell);*/
        auto* form = RE::TESForm::LookupByID(a_event->spell);
        if (!form) {
            log::info("SpellCastEvent: form not found for 0x{:08X}", a_event->spell);
            return RE::BSEventNotifyControl::kContinue;
        }

        log::info("SpellCastEvent: form=0x{:08X} type={}", a_event->spell, form->GetFormType());

        if (auto* spell = form->As<RE::SpellItem>()) {
            log::info("  SpellItem: {}", spell->GetName());
        } else if (auto* ench = form->As<RE::EnchantmentItem>()) {
            log::info("  EnchantmentItem: {}", ench->GetName());
        } else {
            log::info("  Other form: {}", form->GetName());
        }

        //check if it's an msco casting anim
        //bool bCastingMSCO = false;
        //if (!actor->GetGraphVariableBool("bCastingMSCO", bCastingMSCO) || !bCastingMSCO) {
        //    return RE::BSEventNotifyControl::kContinue;
        //}
        ////check the spell for 
        //auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(a_event->spell);
        //if (!spell || spell->GetCastingType() != RE::MagicSystem::CastingType::kFireAndForget) {
        //    return RE::BSEventNotifyControl::kContinue;
        //}
        ////check caster hands for spell casting source = concentration 
        //auto* leftCaster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kLeftHand);
        //auto* rightCaster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kRightHand);

        return RE::BSEventNotifyControl::kContinue;
    }
}