#include <string>

#include "doctest.h"
#include "match_detail_json.h"

using sintence::MatchDetail;
using sintence::ParseMatchDetail;

namespace {

// Ответ match-v5 matches/{id}, урезанный до полей, которые читает разбор.
// Третий участник — бот из пользовательской игры: его быть не должно.
constexpr const char* kMatch = R"({
  "metadata": {"matchId": "RU_528252891", "participants": ["p-self", "p-enemy"]},
  "info": {
    "platformId": "RU",
    "queueId": 420,
    "gameMode": "CLASSIC",
    "gameVersion": "15.12.688.6522",
    "gameCreation": 1750000000000,
    "gameStartTimestamp": 1750000030000,
    "gameDuration": 1834,
    "participants": [
      {
        "puuid": "p-self", "riotIdGameName": "Riot Tuxedo", "riotIdTagline": "TXC1",
        "teamId": 100, "win": true, "championId": 8, "championName": "Vladimir",
        "teamPosition": "MIDDLE", "champLevel": 17,
        "kills": 9, "deaths": 2, "assists": 7,
        "totalMinionsKilled": 210, "neutralMinionsKilled": 12,
        "goldEarned": 14250, "totalDamageDealtToChampions": 31000, "totalDamageTaken": 22000,
        "visionScore": 21, "wardsPlaced": 9, "wardsKilled": 3,
        "item0": 3152, "item1": 3020, "item2": 4645, "item3": 0, "item4": 1058, "item5": 0,
        "item6": 3340,
        "summoner1Id": 4, "summoner2Id": 12,
        "damageDealtToBuildings": 5400,
        "challenges": {"takedownsFirstXMinutes": 6, "soloKills": 3,
                       "laningPhaseGoldExpAdvantage": 1},
        "perks": {"styles": [
          {"style": 8200, "selections": [{"perk": 8214}, {"perk": 8226}]},
          {"style": 8000, "selections": [{"perk": 9111}]}
        ]}
      },
      {
        "puuid": "p-enemy", "riotIdGameName": "Enemy", "riotIdTagline": "RU1",
        "teamId": 200, "win": false, "championId": 238, "championName": "Zed",
        "teamPosition": "MIDDLE", "kills": 2, "deaths": 9, "assists": 1
      },
      {"puuid": "BOT", "teamId": 200, "championId": 1, "championName": "Annie"}
    ]
  }
})";

}  // namespace

TEST_CASE("ParseMatchDetail: заголовок матча") {
    const auto match = ParseMatchDetail(kMatch);
    REQUIRE(match.has_value());
    CHECK(match->match_id == "RU_528252891");
    CHECK(match->platform == "RU");
    CHECK(match->queue_id == 420);
    CHECK(match->game_mode == "CLASSIC");
    CHECK(match->patch == "15.12");
    CHECK(match->started_at_ms == 1750000030000LL);  // старт, а не создание лобби
    CHECK(match->duration_seconds == 1834);
}

TEST_CASE("ParseMatchDetail: участник со всеми полями") {
    const auto match = ParseMatchDetail(kMatch);
    REQUIRE(match.has_value());
    const auto* self = match->Find("p-self");
    REQUIRE(self != nullptr);
    CHECK(self->riot_id == "Riot Tuxedo#TXC1");
    CHECK(self->team_id == 100);
    CHECK(self->win);
    CHECK(self->champion == "Vladimir");
    CHECK(self->role == "MIDDLE");
    CHECK(self->cs == 222);  // миньоны + нейтральные
    CHECK(self->gold == 14250);
    CHECK(self->damage_dealt == 31000);
    CHECK(self->items[0] == 3152);
    CHECK(self->items[6] == 3340);
    CHECK(self->spell1 == 4);
    CHECK(self->keystone == 8214);
    CHECK(self->primary_style == 8200);
    CHECK(self->sub_style == 8000);
}

TEST_CASE("ParseMatchDetail: боты пропускаются, пустые поля — нули") {
    const auto match = ParseMatchDetail(kMatch);
    REQUIRE(match.has_value());
    CHECK(match->participants.size() == 2);
    const auto* enemy = match->Find("p-enemy");
    REQUIRE(enemy != nullptr);
    CHECK(enemy->gold == 0);
    CHECK(enemy->keystone == 0);
    CHECK(match->Find("BOT") == nullptr);
}

TEST_CASE("ParseMatchDetail: без старта берётся время создания") {
    const auto match = ParseMatchDetail(R"({
      "metadata": {"matchId": "RU_1"},
      "info": {"gameCreation": 1700000000000, "gameVersion": "14.1", "participants": []}
    })");
    REQUIRE(match.has_value());
    CHECK(match->started_at_ms == 1700000000000LL);
    CHECK(match->patch == "14.1");
}

TEST_CASE("ParseMatchDetail: мусор и неполный ответ — nullopt") {
    CHECK_FALSE(ParseMatchDetail("не json").has_value());
    CHECK_FALSE(ParseMatchDetail("[]").has_value());
    CHECK_FALSE(ParseMatchDetail(R"({"metadata": {}, "info": {"participants": []}})").has_value());
    CHECK_FALSE(ParseMatchDetail(R"({"metadata": {"matchId": "RU_1"}, "info": {}})").has_value());
}

TEST_CASE("ParseMatchDetail: поля для плашек стиля игры") {
    const auto match = ParseMatchDetail(kMatch);
    REQUIRE(match.has_value());
    const auto* self = match->Find("p-self");
    REQUIRE(self != nullptr);
    CHECK(self->damage_to_buildings == 5400);
    CHECK(self->early_takedowns == 6);
    CHECK(self->solo_kills == 3);
    CHECK(self->lane_lead == 1);
    // Нет challenges (старые матчи, режимы без них) — нули и «нет данных».
    const auto* enemy = match->Find("p-enemy");
    REQUIRE(enemy != nullptr);
    CHECK(enemy->early_takedowns == 0);
    CHECK(enemy->lane_lead == -1);
}
