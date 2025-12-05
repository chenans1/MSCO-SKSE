#pragma once

#include "PCH.h"

namespace MSCO::Magic {

	enum class Hand {
		Left,
		Right
	};

	RE::MagicItem* GetEquippedSpellHand(RE::Actor* actor, Hand hand);

	bool CastEquippedHand(RE::Actor* actor, Hand hand, bool dualCast);
}