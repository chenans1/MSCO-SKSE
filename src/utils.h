#pragma once

namespace utils {

    /*static std::string_view SafeActorName(const RE::Actor* a);
    static std::string_view SafeSpellName(const RE::MagicItem* m);*/

    std::string_view ToString(RE::MagicCaster::State state);
    std::string_view ToString(RE::MagicSystem::CastingType type);
    std::string_view ToString(RE::MagicSystem::SpellType type);

    //static const char* SafeNodeName(const RE::NiAVObject* obj);
    // Naming helpers
    std::string_view SafeNodeName(const RE::NiAVObject* obj);
    std::string_view SafeActorName(const RE::Actor* actor);
    std::string_view SafeSpellName(const RE::MagicItem* spell);
}