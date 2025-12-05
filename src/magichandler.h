#pragma once

#include "PCH.h"

namespace MSCO::Magic {

	/*enum class Hand {
		Left,
		Right
	};*/
    //enum class Hand : std::uint8_t { Right = 0, Left = 1 };

    enum class Hand : std::uint8_t { Right = 1, Left = 0 };
	RE::MagicItem* GetEquippedSpellHand(RE::Actor* actor, Hand hand);

	bool CastEquippedHand(RE::Actor* actor, Hand hand, bool dualCast);
}