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

        // rawChampionName = "game_character_displayname_Zed". Каноническое
        // имя — хвост после последнего подчёркивания; это единственное
        // место, где клиент отдаёт его нелокализованным.
        const auto raw_name = GetString(p, "rawChampionName");
        if (raw_name) {
            const std::size_t underscore = raw_name->rfind('_');
            player.champion_key = underscore == std::string::npos
                                      ? *raw_name
                                      : raw_name->substr(underscore + 1);
        }

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

        // Инвентарь: Live Client отдаёт уже с названиями и ценой,
        // справочник предметов не нужен. Битая запись предмета
        // пропускается — это не повод выбрасывать игрока целиком.
        if (p.contains("items") && p["items"].is_array()) {
            for (const auto& raw_item : p["items"]) {
                if (!raw_item.is_object()) {
                    continue;
                }
                const auto item_id = GetInt(raw_item, "itemID");
                if (!item_id) {
                    continue;
                }
                LiveItem item;
                item.item_id = *item_id;
                item.name = GetString(raw_item, "displayName").value_or("");
                item.slot = GetInt(raw_item, "slot").value_or(0);
                item.count = GetInt(raw_item, "count").value_or(1);
                item.price = GetInt(raw_item, "price").value_or(0);
                player.items.push_back(std::move(item));
            }
        }

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

namespace {

// Одна способность активного игрока. Пустое имя означает, что клиент
// прислал слот без данных — такую способность показывать нечего.
std::optional<LiveAbility> ParseAbility(const nlohmann::json& abilities,
                                        std::string_view slot) {
    if (!abilities.contains(slot) || !abilities[slot].is_object()) {
        return std::nullopt;
    }
    const auto& raw = abilities[slot];

    LiveAbility ability;
    ability.slot = std::string(slot);
    ability.name = GetString(raw, "displayName").value_or("");
    ability.level = GetInt(raw, "abilityLevel").value_or(0);
    if (ability.name.empty()) {
        return std::nullopt;
    }
    return ability;
}

// Имя дерева или руны лежит во вложенном объекте под displayName.
std::string RuneName(const nlohmann::json& parent, std::string_view key) {
    if (!parent.contains(key) || !parent[key].is_object()) {
        return {};
    }
    return GetString(parent[key], "displayName").value_or("");
}

}  // namespace

std::optional<LiveGame> ParseAllGameData(std::string_view json_text) {
    const nlohmann::json doc = nlohmann::json::parse(json_text, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) {
        return std::nullopt;
    }

    // gameData и allPlayers обязательны: без них снимок бесполезен.
    if (!doc.contains("gameData") || !doc.contains("allPlayers")) {
        return std::nullopt;
    }

    auto stats = ParseLiveGameStats(doc["gameData"].dump());
    if (!stats) {
        return std::nullopt;
    }
    auto players = ParseLivePlayers(doc["allPlayers"].dump());
    if (!players) {
        return std::nullopt;
    }

    LiveGame game;
    game.stats = std::move(*stats);
    game.players = std::move(*players);

    // Активного игрока может не быть вовсе: в режиме наблюдателя клиент
    // присылает вместо объекта строку с ошибкой. Это не повод терять табло.
    if (!doc.contains("activePlayer") || !doc["activePlayer"].is_object()) {
        return game;
    }
    const auto& raw_active = doc["activePlayer"];

    LiveActivePlayer active;
    active.riot_id = GetString(raw_active, "riotId")
                         .value_or(GetString(raw_active, "summonerName").value_or(""));
    active.level = GetInt(raw_active, "level").value_or(0);
    active.current_gold = GetDouble(raw_active, "currentGold").value_or(0.0);

    if (raw_active.contains("abilities") && raw_active["abilities"].is_object()) {
        const auto& abilities = raw_active["abilities"];
        // Порядок фиксированный: так же, как на панели способностей в игре.
        for (const std::string_view slot : {"Q", "W", "E", "R", "Passive"}) {
            if (auto ability = ParseAbility(abilities, slot)) {
                active.abilities.push_back(std::move(*ability));
            }
        }
    }

    if (raw_active.contains("fullRunes") && raw_active["fullRunes"].is_object()) {
        const auto& runes = raw_active["fullRunes"];
        active.runes.keystone = RuneName(runes, "keystone");
        active.runes.primary_tree = RuneName(runes, "primaryRuneTree");
        active.runes.secondary_tree = RuneName(runes, "secondaryRuneTree");

        if (runes.contains("generalRunes") && runes["generalRunes"].is_array()) {
            for (const auto& rune : runes["generalRunes"]) {
                if (!rune.is_object()) {
                    continue;
                }
                auto name = GetString(rune, "displayName").value_or("");
                if (name.empty() || name == active.runes.keystone) {
                    // generalRunes начинается с keystone — второй раз
                    // показывать его незачем.
                    continue;
                }
                active.runes.minor_runes.push_back(std::move(name));
            }
        }
    }

    game.active_player = std::move(active);
    return game;
}

}  // namespace sintence
