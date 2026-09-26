#include "riot_api_json.h"

#include <cctype>
#include <string>
#include "json.hpp"
#include "json_helpers.h"

namespace sintence {

std::optional<std::string> ParsePuuid(std::string_view json_text) {
    const nlohmann::json doc = nlohmann::json::parse(json_text, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) {
        return std::nullopt;
    }

    const auto puuid = GetString(doc, "puuid");
    if (!puuid || puuid->empty()) {
        // puuid, которым нельзя сделать запрос, бесполезен ровно
        // так же, как его отсутствие.
        return std::nullopt;
    }
    return puuid;
}

std::optional<std::vector<RankedStats>> ParseRankedEntries(std::string_view json_text) {
    const nlohmann::json doc = nlohmann::json::parse(json_text, nullptr, false);
    if (doc.is_discarded() || !doc.is_array()) {
        return std::nullopt;
    }

    std::vector<RankedStats> entries;
    entries.reserve(doc.size());
    for (const auto& item : doc) {
        if (!item.is_object()) {
            continue;
        }
        // tier и rank обязательны: запись без них не описывает ранг.
        // Внимание: у Riot "rank" — это дивизион (II), а не тир.
        const auto tier = GetString(item, "tier");
        const auto division = GetString(item, "rank");
        if (!tier || !division) {
            continue;
        }

        RankedStats stats;
        stats.queue = GetString(item, "queueType").value_or(std::string{});
        stats.tier = std::move(*tier);
        stats.division = std::move(*division);
        stats.league_points = GetInt(item, "leaguePoints").value_or(0);
        stats.wins = GetInt(item, "wins").value_or(0);
        stats.losses = GetInt(item, "losses").value_or(0);
        entries.push_back(std::move(stats));
    }
    return entries;
}

const RankedStats* FindQueue(const std::vector<RankedStats>& entries,
                             std::string_view queue) {
    for (const RankedStats& entry : entries) {
        if (entry.queue == queue) {
            return &entry;
        }
    }
    return nullptr;
}

std::optional<std::vector<ChampionMastery>> ParseChampionMasteries(
    std::string_view json_text) {
    const nlohmann::json doc = nlohmann::json::parse(json_text, nullptr, false);
    if (doc.is_discarded() || !doc.is_array()) {
        return std::nullopt;
    }

    std::vector<ChampionMastery> masteries;
    masteries.reserve(doc.size());
    for (const auto& item : doc) {
        if (!item.is_object()) {
            continue;
        }
        const auto champion_id = GetInt(item, "championId");
        if (!champion_id) {
            continue;
        }

        ChampionMastery mastery;
        mastery.champion_id = *champion_id;
        mastery.level = GetInt(item, "championLevel").value_or(0);
        // Через int нельзя: очки уходят за миллион, а время — за 1.7e12.
        mastery.points = GetInt64(item, "championPoints").value_or(0);
        mastery.last_play_time_ms = GetInt64(item, "lastPlayTime").value_or(0);
        masteries.push_back(mastery);
    }
    // Порядок Riot сохраняется: массив уже отсортирован по убыванию очков.
    return masteries;
}

std::optional<std::pair<std::string, std::string>> SplitRiotId(
    std::string_view riot_id) {
    const std::size_t hash = riot_id.find('#');
    if (hash == std::string_view::npos) {
        return std::nullopt;
    }

    const std::string_view name = riot_id.substr(0, hash);
    const std::string_view tag = riot_id.substr(hash + 1);
    if (name.empty() || tag.empty()) {
        return std::nullopt;
    }
    return std::pair<std::string, std::string>{std::string(name), std::string(tag)};
}

std::optional<std::string> ParseActiveRegion(std::string_view json_text) {
    const nlohmann::json doc = nlohmann::json::parse(json_text, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) {
        return std::nullopt;
    }

    const auto region = GetString(doc, "region");
    if (!region || region->empty()) {
        return std::nullopt;
    }
    return region;
}

std::optional<std::string> PlatformHost(std::string_view platform_id) {
    if (platform_id.empty()) {
        return std::nullopt;
    }

    std::string host;
    host.reserve(platform_id.size() + 18);
    for (const char symbol : platform_id) {
        host.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(symbol))));
    }
    host += ".api.riotgames.com";
    return host;
}

std::optional<std::vector<LobbyMember>> ParseActiveGame(std::string_view json_text,
                                                        std::string_view self_puuid) {
    const auto doc = nlohmann::json::parse(json_text, nullptr, false);
    if (doc.is_discarded() || !doc.is_object() || !doc.contains("participants") ||
        !doc["participants"].is_array()) {
        return std::nullopt;
    }

    const auto& participants = doc["participants"];
    int self_team = -1;
    for (const auto& raw : participants) {
        if (raw.is_object() && GetString(raw, "puuid").value_or("") == self_puuid) {
            self_team = GetInt(raw, "teamId").value_or(-1);
        }
    }
    if (self_team < 0) {
        return std::nullopt;
    }

    std::vector<LobbyMember> members;
    int cell = 0;
    for (const auto& raw : participants) {
        if (!raw.is_object()) {
            continue;
        }
        LobbyMember member;
        member.puuid = GetString(raw, "puuid").value_or("");
        member.riot_id = GetString(raw, "riotId").value_or("");
        if (member.puuid.empty() && member.riot_id.empty()) {
            continue;
        }
        member.cell_id = cell++;
        member.side = GetInt(raw, "teamId").value_or(-1) == self_team ? LobbySide::Ally
                                                                      : LobbySide::Enemy;
        member.champion_id = GetInt(raw, "championId").value_or(0);
        member.spell1_id = GetInt(raw, "spell1Id").value_or(0);
        member.spell2_id = GetInt(raw, "spell2Id").value_or(0);
        member.is_self = member.puuid == self_puuid;
        members.push_back(std::move(member));
    }
    return members;
}

std::optional<SummonerInfo> ParseSummonerInfo(std::string_view json_text) {
    const auto doc = nlohmann::json::parse(json_text, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) {
        return std::nullopt;
    }
    const auto icon = GetInt(doc, "profileIconId");
    const auto level = GetInt(doc, "summonerLevel");
    if (!icon && !level) {
        return std::nullopt;
    }
    return SummonerInfo{icon.value_or(0), level.value_or(0)};
}

std::optional<std::vector<std::string>> ParseMatchIds(std::string_view json_text) {
    const auto doc = nlohmann::json::parse(json_text, nullptr, false);
    if (doc.is_discarded() || !doc.is_array()) {
        return std::nullopt;
    }
    std::vector<std::string> ids;
    for (const auto& id : doc) {
        if (id.is_string()) {
            ids.push_back(id.get<std::string>());
        }
    }
    return ids;
}

}  // namespace sintence
