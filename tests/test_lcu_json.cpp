#include "doctest.h"
#include "lcu_json.h"
#include "riot_api_json.h"

using sintence::LobbySide;
using sintence::ParseActiveGame;
using sintence::ParseChampSelectSession;
using sintence::ParseCurrentSummoner;
using sintence::ParseGameflowPhase;
using sintence::ParseLockfile;

TEST_CASE("ParseLockfile: пять полей через двоеточие") {
    const auto credentials = ParseLockfile("LeagueClient:12345:54321:AbCd-Ef_12:https");
    REQUIRE(credentials.has_value());
    CHECK(credentials->port == 54321);
    CHECK(credentials->password == "AbCd-Ef_12");
    CHECK(credentials->protocol == "https");

    // Перевод строки в конце — если файл пересохранили руками.
    CHECK(ParseLockfile("LeagueClient:1:2999:pw:https\r\n").has_value());
}

TEST_CASE("ParseLockfile: всё, что не пять полей с числовым портом, отвергается") {
    CHECK_FALSE(ParseLockfile("").has_value());
    CHECK_FALSE(ParseLockfile("LeagueClient:1:2:pw").has_value());
    CHECK_FALSE(ParseLockfile("LeagueClient:1:порт:pw:https").has_value());
    CHECK_FALSE(ParseLockfile("LeagueClient:1:70000:pw:https").has_value());
    CHECK_FALSE(ParseLockfile("LeagueClient:1:2999::https").has_value());
    CHECK_FALSE(ParseLockfile("a:b:c:d:e:f").has_value());
}

TEST_CASE("ParseGameflowPhase: этап приходит JSON-строкой") {
    CHECK(ParseGameflowPhase(R"("ChampSelect")") == "ChampSelect");
    CHECK(ParseGameflowPhase(R"("None")") == "None");
    CHECK_FALSE(ParseGameflowPhase("ChampSelect").has_value());  // без кавычек — не JSON
    CHECK_FALSE(ParseGameflowPhase(R"({"phase":"x"})").has_value());
}

TEST_CASE("ParseCurrentSummoner: puuid и Riot ID") {
    const auto summoner = ParseCurrentSummoner(
        R"({"puuid": "p-self", "gameName": "Лисья тропа", "tagLine": "RU1", "summonerLevel": 300})");
    REQUIRE(summoner.has_value());
    CHECK(summoner->puuid == "p-self");
    CHECK(summoner->riot_id == "Лисья тропа#RU1");

    CHECK_FALSE(ParseCurrentSummoner(R"({"gameName": "x", "tagLine": "y"})").has_value());
}

namespace {

// Сессия выбора чемпиона, урезанная до полей, которые читает разбор.
// Второй союзник — со скрытым именем (анонимность в ранговых):
// клиент прислал puuid, но показывать его нельзя.
constexpr const char* kSession = R"({
  "localPlayerCellId": 2,
  "myTeam": [
    {"cellId": 0, "puuid": "p-ally", "gameName": "Ветер", "tagLine": "EUW",
     "nameVisibilityType": "VISIBLE", "championId": 64, "championPickIntent": 0,
     "assignedPosition": "jungle", "spell1Id": 4, "spell2Id": 11},
    {"cellId": 1, "puuid": "p-hidden", "gameName": "Скрытый", "tagLine": "RU1",
     "nameVisibilityType": "HIDDEN", "championId": 0, "championPickIntent": 222,
     "assignedPosition": "bottom", "spell1Id": 18446744073709551615, "spell2Id": 7},
    {"cellId": 2, "puuid": "p-self", "gameName": "Лисья тропа", "tagLine": "RU1",
     "nameVisibilityType": "HIDDEN", "championId": 103, "championPickIntent": 0,
     "assignedPosition": "middle", "spell1Id": 4, "spell2Id": 14}
  ],
  "theirTeam": [
    {"cellId": 5, "puuid": "p-enemy", "gameName": "Тень", "tagLine": "ZED",
     "championId": 238, "assignedPosition": ""}
  ],
  "bans": {"myTeamBans": [157, -1], "theirTeamBans": [266]},
  "timer": {"phase": "BAN_PICK", "adjustedTimeLeftInPhase": 23456.0}
})";

}  // namespace

TEST_CASE("ParseChampSelectSession: союзники, пики, роли, таймер и баны") {
    const auto lobby = ParseChampSelectSession(kSession);
    REQUIRE(lobby.has_value());
    CHECK(lobby->source == "lcu");
    CHECK(lobby->timer_phase == "BAN_PICK");
    CHECK(lobby->time_left_ms == 23456);
    CHECK(lobby->bans == std::vector<int>{157, 266});
    REQUIRE(lobby->members.size() == 4);

    const auto& lee = lobby->members[0];
    CHECK(lee.side == LobbySide::Ally);
    CHECK(lee.riot_id == "Ветер#EUW");
    CHECK(lee.puuid == "p-ally");
    CHECK(lee.champion_id == 64);
    CHECK(lee.position == "JUNGLE");
    CHECK(lee.spell1_id == 4);
    CHECK_FALSE(lee.is_self);

    // Себя клиент тоже может пометить скрытым — своё имя знать можно.
    const auto& self = lobby->members[2];
    CHECK(self.is_self);
    CHECK(self.riot_id == "Лисья тропа#RU1");
    CHECK(self.position == "MIDDLE");
}

TEST_CASE("ParseChampSelectSession: скрытое имя и противник остаются безымянными") {
    const auto lobby = ParseChampSelectSession(kSession);
    REQUIRE(lobby.has_value());

    // Клиент прислал puuid, но имя скрыто — не раскрываем (POLICY.md).
    const auto& hidden = lobby->members[1];
    CHECK(hidden.hidden);
    CHECK(hidden.puuid.empty());
    CHECK(hidden.riot_id.empty());
    CHECK(hidden.pick_intent_id == 222);
    // 2^64-1 — «заклинание не выбрано», а не id.
    CHECK(hidden.spell1_id == 0);
    CHECK(hidden.spell2_id == 7);

    // Противника в выборе чемпиона игрок не видит — даже если клиент прислал.
    const auto& enemy = lobby->members[3];
    CHECK(enemy.side == LobbySide::Enemy);
    CHECK(enemy.puuid.empty());
    CHECK(enemy.riot_id.empty());
    CHECK(enemy.champion_id == 238);
    CHECK(enemy.position.empty());
}

TEST_CASE("ParseChampSelectSession: битый ответ — nullopt") {
    CHECK_FALSE(ParseChampSelectSession("{").has_value());
    CHECK_FALSE(ParseChampSelectSession("[]").has_value());
    // Пустая сессия — не ошибка: состава просто нет.
    const auto empty = ParseChampSelectSession("{}");
    REQUIRE(empty.has_value());
    CHECK(empty->members.empty());
}

TEST_CASE("ParseActiveGame: стороны относительно себя, perks не читаются") {
    constexpr const char* kGame = R"({
      "gameId": 1, "participants": [
        {"puuid": "p-self", "riotId": "Лисья тропа#RU1", "championId": 103, "teamId": 200,
         "spell1Id": 4, "spell2Id": 14, "perks": {"perkIds": [8112, 8139]}},
        {"puuid": "p-ally", "riotId": "Ветер#EUW", "championId": 64, "teamId": 200,
         "spell1Id": 4, "spell2Id": 11},
        {"puuid": "p-enemy", "riotId": "Тень#ZED", "championId": 238, "teamId": 100,
         "spell1Id": 4, "spell2Id": 14},
        {"championId": 1, "teamId": 100}
      ]
    })";

    const auto members = ParseActiveGame(kGame, "p-self");
    REQUIRE(members.has_value());
    REQUIRE(members->size() == 3);  // без puuid и riotId — пропущен
    CHECK((*members)[0].is_self);
    CHECK((*members)[0].side == LobbySide::Ally);
    CHECK((*members)[1].side == LobbySide::Ally);
    CHECK((*members)[2].side == LobbySide::Enemy);
    CHECK((*members)[2].riot_id == "Тень#ZED");
    CHECK((*members)[2].champion_id == 238);

    // Себя среди участников нет — это не наша игра.
    CHECK_FALSE(ParseActiveGame(kGame, "p-stranger").has_value());
    CHECK_FALSE(ParseActiveGame("{}", "p-self").has_value());
}

TEST_CASE("ParseChampSelectSession: момент окончания этапа для обратного отсчёта") {
    // Клиент обновляет сессию только по событиям: остаток «23 с» стоит
    // на месте до следующего пика. Момент окончания — нет.
    const auto lobby = ParseChampSelectSession(
        R"({"timer": {"phase": "BAN_PICK", "adjustedTimeLeftInPhase": 23000,
                      "internalNowInEpochMs": 1790000000000, "isInfinite": false}})");
    REQUIRE(lobby.has_value());
    CHECK(lobby->time_left_ms == 23000);
    CHECK(lobby->timer_ends_at_ms == 1790000023000);

    // Бесконечный таймер (пользовательская игра) и старый клиент без
    // internalNowInEpochMs — момента окончания нет.
    const auto infinite = ParseChampSelectSession(
        R"({"timer": {"adjustedTimeLeftInPhase": 0, "internalNowInEpochMs": 1790000000000,
                      "isInfinite": true}})");
    REQUIRE(infinite.has_value());
    CHECK(infinite->timer_ends_at_ms == 0);
    const auto old_client =
        ParseChampSelectSession(R"({"timer": {"adjustedTimeLeftInPhase": 5000}})");
    REQUIRE(old_client.has_value());
    CHECK(old_client->timer_ends_at_ms == 0);
}

TEST_CASE("ParseGameflowSession: состав экрана загрузки из клиента") {
    using sintence::ParseGameflowSession;
    constexpr const char* kGameSession = R"({
      "phase": "InProgress",
      "gameData": {
        "teamOne": [
          {"puuid": "p-enemy", "gameName": "Тень", "tagLine": "ZED", "championId": 238,
           "selectedPosition": "MIDDLE"},
          {"puuid": "p-old", "summonerName": "Старый#RU1", "championId": 64}
        ],
        "teamTwo": [
          {"puuid": "p-self", "gameName": "Лисья тропа", "tagLine": "RU1", "championId": 103,
           "selectedPosition": "MIDDLE"},
          {"puuid": "p-ally", "gameName": "Ветер", "tagLine": "EUW", "championId": 64}
        ],
        "playerChampionSelections": [
          {"puuid": "p-self", "championId": 103, "spell1Id": 4, "spell2Id": 14}
        ]
      }
    })";

    const auto members = ParseGameflowSession(kGameSession, "p-self");
    REQUIRE(members.has_value());
    REQUIRE(members->size() == 4);

    // teamOne — противники: себя там нет.
    CHECK((*members)[0].side == LobbySide::Enemy);
    CHECK((*members)[0].riot_id == "Тень#ZED");
    CHECK((*members)[0].position == "MIDDLE");
    // Старый формат имени — целиком в summonerName.
    CHECK((*members)[1].riot_id == "Старый#RU1");

    const auto& self = (*members)[2];
    CHECK(self.side == LobbySide::Ally);
    CHECK(self.is_self);
    CHECK(self.champion_id == 103);
    CHECK(self.spell1_id == 4);
    CHECK(self.spell2_id == 14);
    CHECK((*members)[3].side == LobbySide::Ally);

    // Вне игры gameData пустая или без нас — состава нет.
    CHECK_FALSE(ParseGameflowSession(R"({"phase": "Lobby", "gameData": {"teamOne": [], "teamTwo": []}})",
                                     "p-self")
                    .has_value());
    CHECK_FALSE(ParseGameflowSession(R"({"phase": "None"})", "p-self").has_value());
    CHECK_FALSE(ParseGameflowSession(kGameSession, "").has_value());
}
