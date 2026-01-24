#include "PCH.h"
#include "utils.h"

using namespace SKSE;
using namespace SKSE::log;
using namespace SKSE::stl;

namespace utils {
    std::string_view ToString(RE::MagicCaster::State state) {
        using S = RE::MagicCaster::State;
        switch (state) {
            case S::kNone:
                return "None";
            case S::kUnk01:
                return "Unk01";
            case S::kUnk02:
                return "Unk02";
            case S::kReady:
                return "kReady";
            case S::kUnk04:
                return "Unk04/kRelease";
            case S::kCharging:
                return "kCharging/kUnk05";
            case S::kCasting:
                return "kCasting/kConcentrating";
            case S::kUnk07:
                return "Unk07";
            case S::kUnk08:
                return "kUnk07/Interrupt";
            case S::kUnk09:
                return "kUnk09/Interrupt/Deselect";
            default:
                return "Unknown";
        }
    }

    std::string_view ToString(RE::MagicSystem::CastingType type) {
        using T = RE::MagicSystem::CastingType;
        switch (type) {
            case T::kConstantEffect:
                return "kConstantEffect";
            case T::kFireAndForget:
                return "kFireAndForget";
            case T::kConcentration:
                return "kConcentration";
            case T::kScroll:
                return "kScroll";
            default:
                return "Unknown";
        }
    }

    std::string_view ToString(RE::MagicSystem::SpellType type) {
        using T = RE::MagicSystem::SpellType;
        switch (type) {
            case T::kSpell:
                return "Spell";
            case T::kDisease:
                return "Disease";
            case T::kPower:
                return "Power";
            case T::kLesserPower:
                return "LesserPower";
            case T::kAbility:
                return "Ability";
            case T::kPoison:
                return "Poison";
            case T::kEnchantment:
                return "Enchantment";
            case T::kPotion:
                return "Potion";
            case T::kWortCraft:
                return "WortCraft";
            case T::kLeveledSpell:
                return "LeveledSpell";
            case T::kAddiction:
                return "Addiction";
            case T::kVoicePower:
                return "VoicePower";
            case T::kStaffEnchantment:
                return "StaffEnchantment";
            case T::kScroll:
                return "Scroll";
            default:
                return "Unknown";
        }
    }

}