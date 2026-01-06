#pragma once

#include <optional>
#include <string_view>
#include "RE/M/MagicSystem.h"

namespace {
    struct SpellFirePayload {
        std::optional<RE::MagicSystem::CastingSource> src;
        std::optional<float> costMult;
        std::optional<float> magMult;
        std::optional<bool> dualCast;
    };

    SpellFirePayload ParseSpellFire(std::string_view payload);
}

