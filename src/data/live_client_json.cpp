#include "live_client_json.h"

#include <string>

#include "json.hpp"

namespace sintence {

std::optional<std::vector<LivePlayer>> ParseLivePlayers(
    std::string_view json_text) {
    std::vector<LivePlayer> live_player_stats;
    nlohmann::json doc = nlohmann::json::parse(json_text, nullptr, false);
    if (doc.is_discarded()) {
        return std::nullopt;
    }
    if (!doc.is_array()) {
        return std::nullopt;
    }
    for (const auto& p : doc) {
        if (!p.contains("scores") || !p["scores"].is_object()) {
            continue;
        }
        const auto& scores = p["scores"];

        LivePlayer player;
        const auto champion_name = GetString(p, "championName");
        if (!champion_name) {
            continue;
        }
        player.champion_name = *champion_name;

        const auto kills = GetInt(scores, "kills");
        if (!kills) {
            continue;
        }
        player.kills = *kills;

        const auto deaths = GetInt(scores, "deaths");
        if (!deaths) {
            continue;
        }
        player.deaths = *deaths;

        const auto assists = GetInt(scores, "assists");
        if (!assists) {
            continue;
        }
        player.assists = *assists;

        const auto creep_score = GetInt(scores, "creepScore");
        if (!creep_score) {
            continue;
        }
        player.creep_score = *creep_score;

        const auto team = GetString(p, "team");
        if (!team) {
            continue;
        }
        if (team == "ORDER") {
            player.team = Team::Order;
        } else if (team == "CHAOS") {
            player.team = Team::Chaos;
        } else {
            continue;
        }

        player.level = GetInt(p, "level").value_or(0);
        player.riot_id = GetString(p, "riotId").value_or("");
        player.position = GetString(p, "position").value_or("");
        player.is_dead = GetBoolean(p, "isDead").value_or(false);
        player.is_bot = GetBoolean(p, "isBot").value_or(false);

        live_player_stats.push_back(std::move(player));
    }

    return live_player_stats;
}

std::optional<LiveGameStats> ParseLiveGameStats(std::string_view json_text) {
    LiveGameStats live_game_stats;
    nlohmann::json doc = nlohmann::json::parse(json_text, nullptr, false);
    if (doc.is_discarded()) {
        return std::nullopt;
    }
    if (!doc.is_object()) {
        return std::nullopt;
    }
    const auto game_mode = GetString(doc, "gameMode");
    if (!game_mode) {
        return std::nullopt;
    }
    live_game_stats.game_mode = *game_mode;

    const auto map_name = GetString(doc, "mapName");
    if (!map_name) {
        return std::nullopt;
    }
    live_game_stats.map_name = *map_name;

    const auto game_time = GetDouble(doc, "gameTime");
    if (!game_time) {
        return std::nullopt;
    }
    live_game_stats.game_time_seconds = *game_time;

    return live_game_stats;
}

}  // namespace sintence
