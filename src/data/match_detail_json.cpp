#include "match_detail_json.h"

#include <string>

#include "json.hpp"
#include "json_helpers.h"

namespace sintence {

namespace {

int Int(const nlohmann::json& obj, std::string_view key) {
    return GetInt(obj, key).value_or(0);
}

std::string PatchOf(const std::string& version) {
    const std::size_t first = version.find('.');
    if (first == std::string::npos) {
        return version;
    }
    const std::size_t second = version.find('.', first + 1);
    return second == std::string::npos ? version : version.substr(0, second);
}

// Ключевая руна и деревья: perks.styles[0] — основное, [1] — дополнительное.
void ParsePerks(const nlohmann::json& participant, MatchParticipant& out) {
    if (!participant.contains("perks") || !participant["perks"].is_object()) {
        return;
    }
    const auto& perks = participant["perks"];
    if (!perks.contains("styles") || !perks["styles"].is_array()) {
        return;
    }
    const auto& styles = perks["styles"];
    if (!styles.empty() && styles[0].is_object()) {
        out.primary_style = Int(styles[0], "style");
        if (styles[0].contains("selections") && styles[0]["selections"].is_array() &&
            !styles[0]["selections"].empty() && styles[0]["selections"][0].is_object()) {
            out.keystone = Int(styles[0]["selections"][0], "perk");
        }
    }
    if (styles.size() > 1 && styles[1].is_object()) {
        out.sub_style = Int(styles[1], "style");
    }
}

}  // namespace

std::optional<MatchDetail> ParseMatchDetail(std::string_view json_text) {
    const auto doc = nlohmann::json::parse(json_text, nullptr, false);
    if (doc.is_discarded() || !doc.is_object() || !doc.contains("metadata") ||
        !doc.contains("info") || !doc["metadata"].is_object() || !doc["info"].is_object()) {
        return std::nullopt;
    }
    const auto& info = doc["info"];
    if (!info.contains("participants") || !info["participants"].is_array()) {
        return std::nullopt;
    }

    MatchDetail match;
    match.match_id = GetString(doc["metadata"], "matchId").value_or("");
    if (match.match_id.empty()) {
        return std::nullopt;
    }
    match.platform = GetString(info, "platformId").value_or("");
    match.queue_id = Int(info, "queueId");
    match.game_mode = GetString(info, "gameMode").value_or("");
    match.game_version = GetString(info, "gameVersion").value_or("");
    match.patch = PatchOf(match.game_version);
    match.started_at_ms = GetInt64(info, "gameStartTimestamp")
                              .value_or(GetInt64(info, "gameCreation").value_or(0));
    match.duration_seconds = Int(info, "gameDuration");

    for (const auto& raw : info["participants"]) {
        if (!raw.is_object()) {
            continue;
        }
        MatchParticipant p;
        p.puuid = GetString(raw, "puuid").value_or("");
        if (p.puuid.empty() || p.puuid == "BOT") {
            continue;
        }
        const auto name = GetString(raw, "riotIdGameName").value_or("");
        const auto tag = GetString(raw, "riotIdTagline").value_or("");
        p.riot_id = name.empty() ? GetString(raw, "summonerName").value_or("")
                                 : (tag.empty() ? name : name + "#" + tag);
        p.team_id = Int(raw, "teamId");
        p.win = GetBoolean(raw, "win").value_or(false);
        p.champion_id = Int(raw, "championId");
        p.champion = GetString(raw, "championName").value_or("");
        p.role = GetString(raw, "teamPosition").value_or("");
        p.champ_level = Int(raw, "champLevel");
        p.kills = Int(raw, "kills");
        p.deaths = Int(raw, "deaths");
        p.assists = Int(raw, "assists");
        p.cs = Int(raw, "totalMinionsKilled") + Int(raw, "neutralMinionsKilled");
        p.gold = Int(raw, "goldEarned");
        p.damage_dealt = Int(raw, "totalDamageDealtToChampions");
        p.damage_taken = Int(raw, "totalDamageTaken");
        p.vision_score = Int(raw, "visionScore");
        p.wards_placed = Int(raw, "wardsPlaced");
        p.wards_killed = Int(raw, "wardsKilled");
        for (int slot = 0; slot < 7; ++slot) {
            p.items[static_cast<std::size_t>(slot)] = Int(raw, "item" + std::to_string(slot));
        }
        p.spell1 = Int(raw, "summoner1Id");
        p.spell2 = Int(raw, "summoner2Id");
        ParsePerks(raw, p);
        match.participants.push_back(std::move(p));
    }
    return match;
}

}  // namespace sintence
