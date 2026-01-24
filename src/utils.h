#pragma once

namespace utils {

    /*static std::string_view convert_state(RE::MagicCaster::State state);
    static std::string_view SafeActorName(const RE::Actor* a);
    static std::string_view SafeSpellName(const RE::MagicItem* m);
    static std::string_view convertCastingType(RE::MagicSystem::CastingType type);
    static std::string_view convertSpellType(RE::MagicSystem::SpellType type);*/

    std::string_view ToString(RE::MagicCaster::State state);
    std::string_view ToString(RE::MagicSystem::CastingType type);
    std::string_view ToString(RE::MagicSystem::SpellType type);

   /* static const char* SafeNodeName(const RE::NiAVObject* obj);*/
}