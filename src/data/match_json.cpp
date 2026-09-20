#include "match_json.h"

#include "json.hpp"

namespace course {
namespace {
std::optional<int> GetInt(const nlohmann::json& obj, std::string_view key) {
    if (!obj.contains(key) || !obj[key].is_number_integer()) {
        return std::nullopt;
    }
    return obj[key].get<int>();
}

std::optional<std::string> GetString(const nlohmann::json& obj,
                                     std::string_view key) {
    if (!obj.contains(key) || !obj[key].is_string()) {
        return std::nullopt;
    }
    return obj[key].get<std::string>();
}

std::optional<bool>
GetBoolean(const nlohmann::json& obj, std::string_view key) {
    if (!obj.contains(key) || !obj[key].is_boolean()) {
        return std::nullopt;
    }
    return obj[key].get<bool>();
}
}


std::optional<MatchEntry> ParseMatchEntry(std::string_view json_text,
                                          std::string_view puuid) {
    MatchEntry match_entry;
    nlohmann::json doc = nlohmann::json::parse(json_text, nullptr, false);
    if (doc.is_discarded()) {
        return std::nullopt;
    }
    if (!doc.contains("info")) {
        return std::nullopt;
    }

    const auto info = doc["info"];
    const auto duration = GetInt(info, "gameDuration");
    if (!duration) {
        return std::nullopt;
    }
    match_entry.line.duration_seconds = *duration;
    
    if (!info.contains("participants") || !info["participants"].is_array()) {
        return std::nullopt;
    }
    const auto& participants = info["participants"];
    for (const auto& p : participants) {
        if (p.value("puuid", std::string{}) != puuid) {
            continue;
        }
        const auto champion = GetString(p, "championName");
        if (!champion) {
            return std::nullopt;
        }
        match_entry.champion_name = *champion;

        const auto kills = GetInt(p, "kills");
        if (!kills) {
            return std::nullopt;
        }
        match_entry.line.kills = *kills;

        const auto deaths = GetInt(p, "deaths");
        if (!deaths) {
            return std::nullopt;
        }
        match_entry.line.deaths = *deaths;

        const auto assists = GetInt(p, "assists");
        if (!assists) {
            return std::nullopt;
        }
        match_entry.line.assists = *assists;

        const auto win = GetBoolean(p, "win");
        if (!win) {
            return std::nullopt;
        }
        match_entry.line.win = *win;

        const auto total_minions = GetInt(p, "totalMinionsKilled");
        if (!total_minions) {
            return std::nullopt;
        }
        const auto neutral_minions = GetInt(p, "neutralMinionsKilled");
        if (!neutral_minions) {
            return std::nullopt;
        }
        match_entry.line.minions =
            static_cast<int>(*total_minions) + static_cast<int>(
                *neutral_minions);
    }
    if (match_entry.champion_name.empty()) {
        return std::nullopt;
    }
    return match_entry;
}
} // namespace course