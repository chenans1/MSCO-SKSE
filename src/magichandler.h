#pragma once

#include "PCH.h"

namespace MSCO::Magic {

    enum class Hand : std::uint8_t { Right = 1, Left = 0 };
    RE::MagicSystem::CastingSource HandToSource(Hand hand);
	RE::MagicItem* GetEquippedSpellHand(RE::Actor* actor, Hand hand);
    /*bool CanCastSpell(RE::Actor* actor, RE::MagicItem* leftSpell, RE::MagicItem* rightSpell, bool leftPressed,
                      bool rightPressed, bool& outDualCast);*/
	bool CastEquippedHand(RE::Actor* actor, Hand hand, bool dualCast);
}