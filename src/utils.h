#pragma once

namespace utils {

    /*static std::string_view SafeActorName(const RE::Actor* a);
    static std::string_view SafeSpellName(const RE::MagicItem* m);*/

    std::string_view ToString(RE::MagicCaster::State state);
    std::string_view ToString(RE::MagicSystem::CastingType type);
    std::string_view ToString(RE::MagicSystem::SpellType type);
    std::string_view ToString(RE::MagicSystem::CastingSource type);

    //static const char* SafeNodeName(const RE::NiAVObject* obj);
    // Naming helpers
    std::string_view SafeNodeName(const RE::NiAVObject* obj);
    std::string_view SafeActorName(const RE::Actor* actor);
    std::string_view SafeSpellName(const RE::MagicItem* spell);

    bool HasActorTypeNPC(const RE::Actor* actor);
    bool GetGraphBool(const RE::Actor* actor, const char* name, bool defaultValue = false);
    int GetGraphInt(const RE::Actor* actor, const char* name, std::int32_t defaultValue = 0);
}
