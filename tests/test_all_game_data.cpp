#include "doctest.h"
#include "live_client_json.h"

using sintence::ParseAllGameData;

namespace {

// Форма ответа GET /liveclientdata/allgamedata, урезанная до тех полей,
// которые читает разбор. Имена и вложенность — как отдаёт игра.
constexpr const char* kAllGameData = R"({
  "activePlayer": {
    "abilities": {
      "E": {"abilityLevel": 3, "displayName": "Shadow Slash", "id": "ZedE"},
      "Passive": {"displayName": "Contempt for the Weak", "id": "ZedP"},
      "Q": {"abilityLevel": 5, "displayName": "Razor Shuriken", "id": "ZedQ"},
      "R": {"abilityLevel": 1, "displayName": "Death Mark", "id": "ZedR"},
      "W": {"abilityLevel": 2, "displayName": "Living Shadow", "id": "ZedW"}
    },
    "championStats": {"abilityPower": 0.0, "armor": 62.0},
    "currentGold": 1234.5,
    "fullRunes": {
      "generalRunes": [
        {"displayName": "Electrocute", "id": 8112},
        {"displayName": "Sudden Impact", "id": 8143},
        {"displayName": "Eyeball Collection", "id": 8138},
        {"displayName": "Relentless Hunter", "id": 8105},
        {"displayName": "Triumph", "id": 9111},
        {"displayName": "Coup de Grace", "id": 8014}
      ],
      "keystone": {"displayName": "Electrocute", "id": 8112},
      "primaryRuneTree": {"displayName": "Domination", "id": 8100},
      "secondaryRuneTree": {"displayName": "Precision", "id": 8000},
      "statRunes": [{"id": 5008, "rawDescription": "perk_tooltip_StatModAdaptive"}]
    },
    "level": 11,
    "riotId": "Riot Tuxedo#TXC1",
    "summonerName": "Riot Tuxedo"
  },
  "allPlayers": [
    {
      "championName": "Zed",
      "isBot": false,
      "isDead": false,
      "items": [
        {"canUse": false, "consumable": false, "count": 1, "displayName": "Youmuu's Ghostblade",
         "itemID": 3142, "price": 2900, "slot": 0},
        {"canUse": true, "consumable": true, "count": 2, "displayName": "Health Potion",
         "itemID": 2003, "price": 50, "slot": 1}
      ],
      "level": 11,
      "position": "MIDDLE",
      "riotId": "Riot Tuxedo#TXC1",
      "scores": {"assists": 3, "creepScore": 142, "deaths": 1, "kills": 7, "wardScore": 8.5},
      "team": "ORDER"
    },
    {
      "championName": "Vladimir",
      "isBot": false,
      "isDead": true,
      "items": [],
      "level": 10,
      "position": "TOP",
      "riotId": "Fremius#EUW",
      "scores": {"assists": 1, "creepScore": 88, "deaths": 3, "kills": 2, "wardScore": 0.0},
      "team": "CHAOS"
    }
  ],
  "events": {"Events": [{"EventID": 0, "EventName": "GameStart", "EventTime": 0.0}]},
  "gameData": {
    "gameMode": "CLASSIC",
    "gameTime": 1234.56789,
    "mapName": "Map11",
    "mapNumber": 11,
    "mapTerrain": "Infernal"
  }
})";

}  // namespace

TEST_CASE("ParseAllGameData собирает табло и статистику одним разбором") {
    const auto game = ParseAllGameData(kAllGameData);

    REQUIRE(game.has_value());
    CHECK(game->stats.game_mode == "CLASSIC");
    CHECK(game->stats.map_name == "Map11");
    CHECK(game->stats.game_time_seconds == doctest::Approx(1234.56789));
    REQUIRE(game->players.size() == 2);
    CHECK(game->players[0].champion_name == "Zed");
    CHECK(game->players[0].kills == 7);
    CHECK(game->players[1].is_dead);
}

TEST_CASE("ParseAllGameData читает инвентарь со стаком и ценой") {
    const auto game = ParseAllGameData(kAllGameData);

    REQUIRE(game.has_value());
    REQUIRE(game->players.size() == 2);
    REQUIRE(game->players[0].items.size() == 2);

    const auto& blade = game->players[0].items[0];
    CHECK(blade.item_id == 3142);
    CHECK(blade.name == "Youmuu's Ghostblade");
    CHECK(blade.slot == 0);
    CHECK(blade.count == 1);
    CHECK(blade.price == 2900);

    const auto& potion = game->players[0].items[1];
    CHECK(potion.count == 2);
    CHECK(potion.price == 50);

    // Пустой инвентарь — это пустой вектор, а не пропуск игрока.
    CHECK(game->players[1].items.empty());
}

TEST_CASE("ParseAllGameData читает уровни способностей в порядке Q W E R Passive") {
    const auto game = ParseAllGameData(kAllGameData);

    REQUIRE(game.has_value());
    REQUIRE(game->active_player.has_value());

    const auto& abilities = game->active_player->abilities;
    REQUIRE(abilities.size() == 5);
    CHECK(abilities[0].slot == "Q");
    CHECK(abilities[0].name == "Razor Shuriken");
    CHECK(abilities[0].level == 5);
    CHECK(abilities[1].slot == "W");
    CHECK(abilities[1].level == 2);
    CHECK(abilities[2].slot == "E");
    CHECK(abilities[2].level == 3);
    CHECK(abilities[3].slot == "R");
    CHECK(abilities[3].level == 1);
    // У пассивки уровня нет вовсе — поле abilityLevel отсутствует.
    CHECK(abilities[4].slot == "Passive");
    CHECK(abilities[4].level == 0);
}

TEST_CASE("ParseAllGameData читает руны и не дублирует keystone") {
    const auto game = ParseAllGameData(kAllGameData);

    REQUIRE(game.has_value());
    REQUIRE(game->active_player.has_value());

    const auto& runes = game->active_player->runes;
    CHECK(runes.keystone == "Electrocute");
    CHECK(runes.primary_tree == "Domination");
    CHECK(runes.secondary_tree == "Precision");

    // generalRunes начинается с keystone, в малые руны он попасть не должен.
    REQUIRE(runes.minor_runes.size() == 5);
    CHECK(runes.minor_runes[0] == "Sudden Impact");
    CHECK(runes.minor_runes[4] == "Coup de Grace");
}

TEST_CASE("ParseAllGameData читает золото и уровень активного игрока") {
    const auto game = ParseAllGameData(kAllGameData);

    REQUIRE(game.has_value());
    REQUIRE(game->active_player.has_value());
    CHECK(game->active_player->riot_id == "Riot Tuxedo#TXC1");
    CHECK(game->active_player->level == 11);
    CHECK(game->active_player->current_gold == doctest::Approx(1234.5));
}

TEST_CASE("ParseAllGameData: наблюдатель без активного игрока — табло остаётся") {
    // В режиме наблюдателя клиент присылает вместо объекта строку с ошибкой.
    constexpr const char* kSpectator = R"({
      "activePlayer": "Error: 'activePlayer' is not available in spectator mode",
      "allPlayers": [
        {"championName": "Zed", "riotId": "a#1", "team": "ORDER", "items": [],
         "scores": {"assists": 0, "creepScore": 0, "deaths": 0, "kills": 0, "wardScore": 0.0}}
      ],
      "gameData": {"gameMode": "CLASSIC", "gameTime": 10.0, "mapName": "Map11"}
    })";

    const auto game = ParseAllGameData(kSpectator);

    REQUIRE(game.has_value());
    CHECK_FALSE(game->active_player.has_value());
    CHECK(game->players.size() == 1);
}

TEST_CASE("ParseAllGameData: нет gameData или allPlayers — nullopt") {
    CHECK_FALSE(ParseAllGameData(R"({"allPlayers": []})").has_value());
    CHECK_FALSE(ParseAllGameData(R"({"gameData": {}})").has_value());
    CHECK_FALSE(ParseAllGameData("{").has_value());
    CHECK_FALSE(ParseAllGameData("[]").has_value());
}

TEST_CASE("ParseAllGameData достаёт каноническое имя чемпиона") {
    // championName приходит на языке клиента («Владимир»), и по нему
    // нельзя ни искать в паке предпочтений, ни ходить в Data Dragon.
    constexpr const char* kLocalized = R"({
      "allPlayers": [
        {"championName": "Ли Син", "rawChampionName": "game_character_displayname_LeeSin",
         "riotId": "a#1", "team": "ORDER", "items": [],
         "scores": {"assists": 0, "creepScore": 0, "deaths": 0, "kills": 0, "wardScore": 0.0}},
        {"championName": "Nunu & Willump",
         "rawChampionName": "game_character_displayname_Nunu",
         "riotId": "b#2", "team": "CHAOS", "items": [],
         "scores": {"assists": 0, "creepScore": 0, "deaths": 0, "kills": 0, "wardScore": 0.0}}
      ],
      "gameData": {"gameMode": "CLASSIC", "gameTime": 10.0, "mapName": "Map11"}
    })";

    const auto game = ParseAllGameData(kLocalized);

    REQUIRE(game.has_value());
    REQUIRE(game->players.size() == 2);
    CHECK(game->players[0].champion_name == "Ли Син");
    CHECK(game->players[0].champion_key == "LeeSin");
    CHECK(game->players[1].champion_key == "Nunu");
}

TEST_CASE("ParseAllGameData: нет rawChampionName — ключ пустой, игрок остаётся") {
    constexpr const char* kNoRaw = R"({
      "allPlayers": [
        {"championName": "Zed", "riotId": "a#1", "team": "ORDER", "items": [],
         "scores": {"assists": 0, "creepScore": 0, "deaths": 0, "kills": 0, "wardScore": 0.0}}
      ],
      "gameData": {"gameMode": "CLASSIC", "gameTime": 10.0, "mapName": "Map11"}
    })";

    const auto game = ParseAllGameData(kNoRaw);

    REQUIRE(game.has_value());
    REQUIRE(game->players.size() == 1);
    CHECK(game->players[0].champion_key.empty());
}

TEST_CASE("ParseAllGameData: id способностей, рун и осколков активного игрока") {
    const auto game = ParseAllGameData(kAllGameData);

    REQUIRE(game.has_value());
    REQUIRE(game->active_player.has_value());
    const auto& active = *game->active_player;

    REQUIRE(active.abilities.size() == 5);
    CHECK(active.abilities[0].id == "ZedQ");
    CHECK(active.abilities[4].id == "ZedP");

    CHECK(active.runes.keystone_id == 8112);
    CHECK(active.runes.primary_tree_id == 8100);
    CHECK(active.runes.secondary_tree_id == 8000);
    // Ключевая руна в generalRunes первой, но в малые не попадает.
    REQUIRE(active.runes.minor_rune_ids.size() == 5);
    CHECK(active.runes.minor_rune_ids.front() == 8143);
    CHECK(active.runes.minor_rune_ids.size() == active.runes.minor_runes.size());
    REQUIRE(active.runes.shard_ids.size() == 1);
    CHECK(active.runes.shard_ids[0] == 5008);
}

TEST_CASE("ParseAllGameData: руны и заклинания призывателя у всех игроков") {
    // У чужих Live Client отдаёт только ключевую руну и деревья — то,
    // что видно на табло по Tab. Малых рун у них нет и быть не должно.
    constexpr const char* kWithRunes = R"({
      "allPlayers": [
        {"championName": "Кай'Са", "rawChampionName": "game_character_displayname_Kaisa",
         "riotId": "Kaisa#BOT", "team": "ORDER", "items": [],
         "runes": {
           "keystone": {"displayName": "Решительное наступление", "id": 8005},
           "primaryRuneTree": {"displayName": "Точность", "id": 8000},
           "secondaryRuneTree": {"displayName": "Колдовство", "id": 8200}
         },
         "summonerSpells": {
           "summonerSpellOne": {"displayName": "Барьер",
             "rawDisplayName": "GeneratedTip_SummonerSpell_SummonerBarrier_DisplayName"},
           "summonerSpellTwo": {"displayName": "Скачок",
             "rawDisplayName": "GeneratedTip_SummonerSpell_SummonerFlash_DisplayName"}
         },
         "scores": {"assists": 0, "creepScore": 0, "deaths": 0, "kills": 0, "wardScore": 0.0}},
        {"championName": "Zed", "riotId": "a#1", "team": "CHAOS", "items": [],
         "summonerSpells": {"summonerSpellOne": {"displayName": "???", "rawDisplayName": "broken"}},
         "scores": {"assists": 0, "creepScore": 0, "deaths": 0, "kills": 0, "wardScore": 0.0}}
      ],
      "gameData": {"gameMode": "PRACTICETOOL", "gameTime": 10.0, "mapName": "Map11"}
    })";

    const auto game = ParseAllGameData(kWithRunes);

    REQUIRE(game.has_value());
    REQUIRE(game->players.size() == 2);

    const auto& kaisa = game->players[0];
    CHECK(kaisa.runes.keystone == "Решительное наступление");
    CHECK(kaisa.runes.keystone_id == 8005);
    CHECK(kaisa.runes.primary_tree_id == 8000);
    CHECK(kaisa.runes.secondary_tree_id == 8200);
    CHECK(kaisa.runes.minor_rune_ids.empty());
    REQUIRE(kaisa.summoner_spells.size() == 2);
    CHECK(kaisa.summoner_spells[0].key == "SummonerBarrier");
    CHECK(kaisa.summoner_spells[0].name == "Барьер");
    CHECK(kaisa.summoner_spells[1].key == "SummonerFlash");

    // Рун нет — нули; ключ заклинания не разобрался — пустой, имя остаётся.
    const auto& zed = game->players[1];
    CHECK(zed.runes.keystone_id == 0);
    REQUIRE(zed.summoner_spells.size() == 1);
    CHECK(zed.summoner_spells[0].key.empty());
    CHECK(zed.summoner_spells[0].name == "???");
}
