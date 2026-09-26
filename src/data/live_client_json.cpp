#include "live_client_json.h"

#include <algorithm>
#include <cctype>
#include <string>

#include "json.hpp"

namespace sintence {

namespace {

// Имя и id руны или дерева лежат во вложенном объекте:
//   "keystone": {"displayName": "Электрошок", "id": 8112, ...}
std::string RuneName(const nlohmann::json& parent, std::string_view key) {
    if (!parent.contains(key) || !parent[key].is_object()) {
        return {};
    }
    return GetString(parent[key], "displayName").value_or("");
}

int RuneId(const nlohmann::json& parent, std::string_view key) {
    if (!parent.contains(key) || !parent[key].is_object()) {
        return 0;
    }
    return GetInt(parent[key], "id").value_or(0);
}

// Ключевая руна и деревья — общая часть рун активного игрока и табло.
void ParseRuneHeader(const nlohmann::json& runes, LiveRunes& out) {
    out.keystone = RuneName(runes, "keystone");
    out.keystone_id = RuneId(runes, "keystone");
    out.primary_tree = RuneName(runes, "primaryRuneTree");
    out.primary_tree_id = RuneId(runes, "primaryRuneTree");
    out.secondary_tree = RuneName(runes, "secondaryRuneTree");
    out.secondary_tree_id = RuneId(runes, "secondaryRuneTree");
}

// "GeneratedTip_SummonerSpell_SummonerFlash_DisplayName" -> "SummonerFlash".
// Нелокализованный ключ есть только внутри rawDisplayName, а Data Dragon
// знает заклинания именно по нему.
std::string SummonerSpellKey(std::string_view raw) {
    constexpr std::string_view prefix = "GeneratedTip_SummonerSpell_";
    constexpr std::string_view suffix = "_DisplayName";
    if (!raw.starts_with(prefix) || !raw.ends_with(suffix) ||
        raw.size() <= prefix.size() + suffix.size()) {
        return {};
    }
    raw.remove_prefix(prefix.size());
    raw.remove_suffix(suffix.size());
    return std::string(raw);
}

std::vector<LiveSummonerSpell> ParseSummonerSpells(const nlohmann::json& player) {
    std::vector<LiveSummonerSpell> spells;
    if (!player.contains("summonerSpells") || !player["summonerSpells"].is_object()) {
        return spells;
    }
    const auto& raw = player["summonerSpells"];
    for (const std::string_view slot : {"summonerSpellOne", "summonerSpellTwo"}) {
        if (!raw.contains(slot) || !raw[slot].is_object()) {
            continue;
        }
        LiveSummonerSpell spell;
        spell.name = GetString(raw[slot], "displayName").value_or("");
        spell.key = SummonerSpellKey(GetString(raw[slot], "rawDisplayName").value_or(""));
        if (spell.name.empty() && spell.key.empty()) {
            continue;
        }
        spells.push_back(std::move(spell));
    }
    return spells;
}

}  // namespace

std::string ChampionKeyFromRaw(std::string_view raw) {
    // Служебные слова в любом регистре: "game", "Character", "Name"...
    const auto is_service = [](std::string_view part) {
        std::string lower;
        for (const char symbol : part) {
            lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(symbol))));
        }
        return lower == "game" || lower == "character" || lower == "displayname" ||
               lower == "skin" || lower == "name";
    };
    const auto is_number = [](std::string_view part) {
        return !part.empty() &&
               std::all_of(part.begin(), part.end(),
                           [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; });
    };

    std::size_t start = 0;
    while (start <= raw.size()) {
        const std::size_t end = std::min(raw.find('_', start), raw.size());
        const std::string_view part = raw.substr(start, end - start);
        if (!part.empty() && !is_service(part) && !is_number(part)) {
            return std::string(part);
        }
        start = end + 1;
    }
    return {};
}

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

        // Каноническое имя — только в служебных строках клиента, и формат
        // у них разный: см. ChampionKeyFromRaw. rawSkinName — запасной
        // источник, если rawChampionName не разобрался.
        player.champion_key =
            ChampionKeyFromRaw(GetString(p, "rawChampionName").value_or(""));
        if (player.champion_key.empty()) {
            player.champion_key =
                ChampionKeyFromRaw(GetString(p, "rawSkinName").value_or(""));
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

        if (p.contains("runes") && p["runes"].is_object()) {
            ParseRuneHeader(p["runes"], player.runes);
        }
        player.summoner_spells = ParseSummonerSpells(p);

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
    ability.id = GetString(raw, "id").value_or("");
    ability.name = GetString(raw, "displayName").value_or("");
    ability.level = GetInt(raw, "abilityLevel").value_or(0);
    if (ability.name.empty()) {
        return std::nullopt;
    }
    return ability;
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
        ParseRuneHeader(runes, active.runes);

        if (runes.contains("generalRunes") && runes["generalRunes"].is_array()) {
            for (const auto& rune : runes["generalRunes"]) {
                if (!rune.is_object()) {
                    continue;
                }
                auto name = GetString(rune, "displayName").value_or("");
                const int id = GetInt(rune, "id").value_or(0);
                if (name.empty() || (id != 0 && id == active.runes.keystone_id) ||
                    name == active.runes.keystone) {
                    // generalRunes начинается с keystone — второй раз
                    // показывать его незачем.
                    continue;
                }
                active.runes.minor_runes.push_back(std::move(name));
                active.runes.minor_rune_ids.push_back(id);
            }
        }

        // Осколки статов: названий у них в ответе нет, только id и
        // rawDescription. Интерфейсу id достаточно.
        if (runes.contains("statRunes") && runes["statRunes"].is_array()) {
            for (const auto& shard : runes["statRunes"]) {
                if (!shard.is_object()) {
                    continue;
                }
                if (const auto id = GetInt(shard, "id")) {
                    active.runes.shard_ids.push_back(*id);
                }
            }
        }
    }

    game.active_player = std::move(active);
    return game;
}

}  // namespace sintence
