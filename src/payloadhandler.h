#pragma once

namespace MSCO {
    struct SpellFirePayload {
        std::optional<RE::MagicSystem::CastingSource> src;
        std::optional<float> costMult;
        std::optional<float> magMult;
        std::optional<bool> dualCast;
    };

    SpellFirePayload ParseSpellFire(std::string_view payload);
}

