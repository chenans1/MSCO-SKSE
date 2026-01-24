#include "payloadhandler.h"
#include "PCH.h"
#include <charconv>
#include <cctype>
#include <algorithm>
#include <cmath>
#include <optional>
#include <string_view>

using namespace SKSE;
using namespace SKSE::log;
using namespace SKSE::stl;
using namespace std::literals;

namespace MSCO {

    //helpers
    bool IsSep(char c) { return c == ' ' || c == '\t' || c == ',' || c == ';'; }
    std::string_view Trim(std::string_view s) {
        while (!s.empty() && std::isspace((unsigned char)s.front())) s.remove_prefix(1);
        while (!s.empty() && std::isspace((unsigned char)s.back())) s.remove_suffix(1);
        return s;
    }

    std::optional<float> ParseFloat(std::string_view s) {
        float out{};
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
        if (ec != std::errc{} || ptr == s.data()) return std::nullopt;
        return out;
    }

    std::optional<RE::MagicSystem::CastingSource> ParseSource(std::string_view s) {
        if (s == "left")    return RE::MagicSystem::CastingSource::kLeftHand;
        if (s == "right")   return RE::MagicSystem::CastingSource::kRightHand;
        if (s == "other")   return RE::MagicSystem::CastingSource::kOther;
        if (s == "instant") return RE::MagicSystem::CastingSource::kInstant;
        return std::nullopt;
    }

    SpellFirePayload ParseSpellFire(std::string_view payload) { 
        SpellFirePayload out{};
        payload = Trim(payload); //trim leading/trailing
        while (!payload.empty()) {
            while (!payload.empty() && IsSep(payload.front())) payload.remove_prefix(1);

            size_t end = 0;
            while (end < payload.size() && !IsSep(payload[end])) ++end;

            auto token = payload.substr(0, end);
            payload.remove_prefix(end);

            auto eq = token.find('=');
            if (eq == std::string_view::npos) continue;

            auto key = Trim(token.substr(0, eq));
            auto val = Trim(token.substr(eq + 1));

            if (key == "src")
                out.src = ParseSource(val);
            else if (key == "cost")
                out.costMult = ParseFloat(val);
            else if (key == "mag")
                out.magMult = ParseFloat(val);
        }

        return out;
    }
}