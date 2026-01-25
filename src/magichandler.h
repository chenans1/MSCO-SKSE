#pragma once

namespace MSCO::Magic {

    enum class Hand : std::uint8_t { Right = 1, Left = 0 };
    RE::MagicSystem::CastingSource HandToSource(Hand hand);
	RE::MagicItem* GetEquippedSpellHand(RE::Actor* actor, Hand hand);
    //bool CanCastSpell(RE::Actor* actor, RE::MagicItem* leftSpell, RE::MagicItem* rightSpell, bool leftPressed, bool rightPressed, bool& outDualCast);
	//bool CastEquippedHand(RE::Actor* actor, Hand hand, bool dualCast);
    //bool Spellfire(RE::MagicSystem::CastingSource source, RE::Actor* actor, RE::MagicItem* spell, bool dualCast, float magmult, RE::MagicSystem::CastingSource outputSource);
    bool ConsumeResource(RE::MagicSystem::CastingSource source, RE::Actor* actor, const RE::MagicItem* spell, bool dualCast, float costmult);
    const RE::MagicItem* GetEquippedSpell(RE::Actor* actor, bool leftHand);
    const RE::MagicItem* GetCastingSpell(RE::Actor* actor, RE::MagicSystem::CastingSource source);
    const RE::MagicItem* GetCastingSpell(RE::Actor* actor, bool leftHand);
    void InterruptCaster(RE::Actor* actor, bool isLeft);
    void Install();
}