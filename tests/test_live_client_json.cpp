#include "doctest.h"
#include "live_client_json.h"

using sintence::LivePlayer;
using sintence::ParseLiveGameStats;
using sintence::ParseLivePlayers;
using sintence::Team;

namespace {

// Фикстура по официальному примеру из developer.riotgames.com/docs/lol,
// раздел Game Client API -> Live Client Data API -> All Players.
// Поля и их регистр — как отдаёт игра, менять нельзя.
constexpr const char* kPlayerList = R"([
  {
    "championName": "Annie",
    "isBot": false,
    "isDead": false,
    "level": 6,
    "position": "MIDDLE",
    "rawChampionName": "game_character_displayname_Annie",
    "respawnTimer": 0.0,
    "riotId": "Riot Tuxedo#TXC1",
    "riotIdGameName": "Riot Tuxedo",
    "riotIdTagLine": "TXC1",
    "scores": {"assists": 3, "creepScore": 62, "deaths": 1, "kills": 4, "wardScore": 2.5},
    "skinID": 0,
    "summonerName": "Riot Tuxedo",
    "team": "ORDER"
  },
  {
    "championName": "Vladimir",
    "isBot": false,
    "isDead": true,
    "level": 7,
    "position": "TOP",
    "respawnTimer": 12.5,
    "riotId": "Fremius#EUW",
    "scores": {"assists": 1, "creepScore": 88, "deaths": 3, "kills": 2, "wardScore": 0.0},
    "team": "CHAOS"
  }
])";

constexpr const char* kGameStats = R"({
  "gameMode": "CLASSIC",
  "gameTime": 1234.56789,
  "mapName": "Map11",
  "mapNumber": 11,
  "mapTerrain": "Default"
})";

}  // namespace

TEST_CASE("ParseLivePlayers разбирает табло целиком") {
    const auto players = ParseLivePlayers(kPlayerList);

    REQUIRE(players.has_value());
    REQUIRE(players->size() == 2);
    CHECK((*players)[0].champion_name == "Annie");
    CHECK((*players)[1].champion_name == "Vladimir");
}

TEST_CASE("ParseLivePlayers достаёт счёт из вложенного scores") {
    const auto players = ParseLivePlayers(kPlayerList);

    REQUIRE(players.has_value());
    REQUIRE(players->size() == 2);

    const LivePlayer& annie = (*players)[0];
    CHECK(annie.kills == 4);
    CHECK(annie.deaths == 1);
    CHECK(annie.assists == 3);
    CHECK(annie.creep_score == 62);
    CHECK(annie.level == 6);
}

TEST_CASE("ParseLivePlayers различает команды") {
    const auto players = ParseLivePlayers(kPlayerList);

    REQUIRE(players.has_value());
    REQUIRE(players->size() == 2);
    CHECK((*players)[0].team == Team::Order);
    CHECK((*players)[1].team == Team::Chaos);
}

TEST_CASE("ParseLivePlayers сохраняет riotId, позицию и флаги") {
    const auto players = ParseLivePlayers(kPlayerList);

    REQUIRE(players.has_value());
    REQUIRE(players->size() == 2);
    CHECK((*players)[0].riot_id == "Riot Tuxedo#TXC1");
    CHECK((*players)[0].position == "MIDDLE");
    CHECK_FALSE((*players)[0].is_dead);
    CHECK((*players)[1].is_dead);
}

TEST_CASE("ParseLivePlayers: один битый игрок не ломает остальных") {
    // У первого нет scores — он пропускается, второй разбирается.
    constexpr const char* kBroken = R"([
      {"championName": "Annie", "team": "ORDER", "riotId": "a#1"},
      {"championName": "Zed", "team": "CHAOS", "riotId": "b#2",
       "scores": {"assists": 0, "creepScore": 10, "deaths": 0, "kills": 1, "wardScore": 0.0}}
    ])";

    const auto players = ParseLivePlayers(kBroken);

    REQUIRE(players.has_value());
    REQUIRE(players->size() == 1);
    CHECK((*players)[0].champion_name == "Zed");
}

TEST_CASE("ParseLivePlayers: неизвестная команда — игрок пропускается") {
    constexpr const char* kUnknownTeam = R"([
      {"championName": "Annie", "team": "NEUTRAL", "riotId": "a#1",
       "scores": {"assists": 0, "creepScore": 0, "deaths": 0, "kills": 0, "wardScore": 0.0}}
    ])";

    const auto players = ParseLivePlayers(kUnknownTeam);

    REQUIRE(players.has_value());
    CHECK(players->empty());
}

TEST_CASE("ParseLivePlayers: пустой массив — пустой вектор, а не nullopt") {
    const auto players = ParseLivePlayers("[]");

    REQUIRE(players.has_value());
    CHECK(players->empty());
}

TEST_CASE("ParseLivePlayers: битый JSON и не-массив — nullopt") {
    CHECK_FALSE(ParseLivePlayers("{").has_value());
    CHECK_FALSE(ParseLivePlayers("").has_value());
    CHECK_FALSE(ParseLivePlayers(R"({"championName": "Annie"})").has_value());
}

TEST_CASE("ParseLiveGameStats разбирает режим, карту и время") {
    const auto stats = ParseLiveGameStats(kGameStats);

    REQUIRE(stats.has_value());
    CHECK(stats->game_mode == "CLASSIC");
    CHECK(stats->map_name == "Map11");
    CHECK(stats->game_time_seconds == doctest::Approx(1234.56789));
}

TEST_CASE("ParseLiveGameStats: время не округляется до целого") {
    // 59.9 секунды — это ещё не минута. Разбор через int превратил бы
    // это в 59, а в тестах на границе минуты дал бы расхождение.
    const auto stats = ParseLiveGameStats(R"({"gameMode": "ARAM", "gameTime": 59.9, "mapName": "Map12"})");

    REQUIRE(stats.has_value());
    CHECK(stats->game_time_seconds == doctest::Approx(59.9));
}

TEST_CASE("ParseLiveGameStats: нет обязательного поля — nullopt") {
    CHECK_FALSE(ParseLiveGameStats(R"({"gameMode": "CLASSIC", "mapName": "Map11"})").has_value());
    CHECK_FALSE(ParseLiveGameStats("{").has_value());
    CHECK_FALSE(ParseLiveGameStats("[]").has_value());
}
