#include "doctest.h"
#include "riot_api_json.h"

using sintence::ChampionMastery;
using sintence::FindQueue;
using sintence::ParseActiveRegion;
using sintence::ParseChampionMasteries;
using sintence::ParsePuuid;
using sintence::ParseRankedEntries;
using sintence::PlatformHost;
using sintence::RankedStats;
using sintence::SplitRiotId;

namespace {

// Ответ account-v1 by-riot-id. Поля ровно те, что отдаёт Riot.
constexpr const char* kAccount = R"({
  "puuid": "8yv1CdT9-Cq1fZ7sQ2m0KpLxWn3aBcDeFgHiJkLmNoPqRsTuVwXyZ01234567890abcdefghijklmn",
  "gameName": "Riot Tuxedo",
  "tagLine": "TXC1"
})";

// Ответ league-v4 entries/by-puuid: массив по очередям.
// summonerId в схеме больше нет — Riot убрал его в 2025 году.
constexpr const char* kEntries = R"([
  {
    "leagueId": "3a1e0b1c-0000-4a00-8000-000000000001",
    "puuid": "8yv1CdT9",
    "queueType": "RANKED_SOLO_5x5",
    "tier": "EMERALD",
    "rank": "II",
    "leaguePoints": 42,
    "wins": 97,
    "losses": 83,
    "veteran": false,
    "inactive": false,
    "freshBlood": true,
    "hotStreak": false
  },
  {
    "leagueId": "3a1e0b1c-0000-4a00-8000-000000000002",
    "puuid": "8yv1CdT9",
    "queueType": "RANKED_FLEX_SR",
    "tier": "GOLD",
    "rank": "IV",
    "leaguePoints": 8,
    "wins": 12,
    "losses": 14,
    "veteran": false,
    "inactive": false,
    "freshBlood": false,
    "hotStreak": false
  }
])";

// Ответ champion-mastery-v4 top?count=3, уже отсортирован Riot.
constexpr const char* kMasteries = R"([
  {
    "puuid": "8yv1CdT9",
    "championId": 157,
    "championLevel": 42,
    "championPoints": 1234567,
    "lastPlayTime": 1758830400000,
    "championPointsSinceLastLevel": 12300,
    "championPointsUntilNextLevel": 0,
    "chestGranted": true,
    "tokensEarned": 0
  },
  {
    "puuid": "8yv1CdT9",
    "championId": 64,
    "championLevel": 15,
    "championPoints": 310450,
    "lastPlayTime": 1756238400000,
    "championPointsSinceLastLevel": 4500,
    "championPointsUntilNextLevel": 1000,
    "chestGranted": false,
    "tokensEarned": 1
  },
  {
    "puuid": "8yv1CdT9",
    "championId": 245,
    "championLevel": 9,
    "championPoints": 88900,
    "lastPlayTime": 1700000000000,
    "championPointsSinceLastLevel": 900,
    "championPointsUntilNextLevel": 3100,
    "chestGranted": true,
    "tokensEarned": 0
  }
])";

}  // namespace

TEST_CASE("ParsePuuid достаёт puuid из ответа account-v1") {
    const auto puuid = ParsePuuid(kAccount);

    REQUIRE(puuid.has_value());
    CHECK(puuid->starts_with("8yv1CdT9"));
    CHECK(puuid->size() == 78);  // ровно столько символов у настоящего puuid
}

TEST_CASE("ParsePuuid: нет поля, пустая строка и мусор — nullopt") {
    CHECK_FALSE(ParsePuuid(R"({"gameName": "Riot Tuxedo"})").has_value());
    CHECK_FALSE(ParsePuuid(R"({"puuid": ""})").has_value());
    CHECK_FALSE(ParsePuuid("{").has_value());
    CHECK_FALSE(ParsePuuid("[]").has_value());
}

TEST_CASE("ParseRankedEntries разбирает обе очереди") {
    const auto entries = ParseRankedEntries(kEntries);

    REQUIRE(entries.has_value());
    REQUIRE(entries->size() == 2);
    CHECK((*entries)[0].queue == "RANKED_SOLO_5x5");
    CHECK((*entries)[1].queue == "RANKED_FLEX_SR");
}

TEST_CASE("ParseRankedEntries: rank становится division, а не наоборот") {
    // Ловушка именования: у Riot "tier" — это EMERALD, а "rank" — это II.
    // Перепутанные местами поля дадут тир "II" и дивизион "EMERALD".
    const auto entries = ParseRankedEntries(kEntries);

    REQUIRE(entries.has_value());
    REQUIRE_FALSE(entries->empty());

    const RankedStats& solo = (*entries)[0];
    CHECK(solo.tier == "EMERALD");
    CHECK(solo.division == "II");
    CHECK(solo.league_points == 42);
    CHECK(solo.wins == 97);
    CHECK(solo.losses == 83);
}

TEST_CASE("ParseRankedEntries: пустой массив — пустой вектор, а не nullopt") {
    // Аккаунт без калибровки. Это ответ, а не сбой.
    const auto entries = ParseRankedEntries("[]");

    REQUIRE(entries.has_value());
    CHECK(entries->empty());
}

TEST_CASE("ParseRankedEntries: запись без tier пропускается, остальные живут") {
    constexpr const char* kPartial = R"([
      {"queueType": "RANKED_SOLO_5x5", "leaguePoints": 10, "wins": 1, "losses": 2},
      {"queueType": "RANKED_FLEX_SR", "tier": "SILVER", "rank": "I",
       "leaguePoints": 55, "wins": 20, "losses": 18}
    ])";

    const auto entries = ParseRankedEntries(kPartial);

    REQUIRE(entries.has_value());
    REQUIRE(entries->size() == 1);
    CHECK((*entries)[0].tier == "SILVER");
}

TEST_CASE("ParseRankedEntries: битый JSON и объект вместо массива — nullopt") {
    CHECK_FALSE(ParseRankedEntries("{").has_value());
    CHECK_FALSE(ParseRankedEntries(R"({"tier": "GOLD"})").has_value());
    CHECK_FALSE(ParseRankedEntries("").has_value());
}

TEST_CASE("FindQueue находит соло-очередь и не путает её с флексом") {
    const auto entries = ParseRankedEntries(kEntries);
    REQUIRE(entries.has_value());

    const RankedStats* solo = FindQueue(*entries, "RANKED_SOLO_5x5");
    REQUIRE(solo != nullptr);
    CHECK(solo->tier == "EMERALD");

    const RankedStats* flex = FindQueue(*entries, "RANKED_FLEX_SR");
    REQUIRE(flex != nullptr);
    CHECK(flex->tier == "GOLD");
}

TEST_CASE("FindQueue: очереди нет — nullptr") {
    const auto entries = ParseRankedEntries(kEntries);
    REQUIRE(entries.has_value());

    CHECK(FindQueue(*entries, "RANKED_TFT") == nullptr);
    CHECK(FindQueue({}, "RANKED_SOLO_5x5") == nullptr);
}

TEST_CASE("ParseChampionMasteries сохраняет порядок Riot") {
    const auto masteries = ParseChampionMasteries(kMasteries);

    REQUIRE(masteries.has_value());
    REQUIRE(masteries->size() == 3);
    CHECK((*masteries)[0].champion_id == 157);
    CHECK((*masteries)[1].champion_id == 64);
    CHECK((*masteries)[2].champion_id == 245);
}

TEST_CASE("ParseChampionMasteries: очки и дата не теряют точность в int") {
    // lastPlayTime порядка 1.7e12 — в int не влезает. Разбор через int
    // даёт либо мусор, либо обрезанное значение, и «играл вчера»
    // превращается в «играл в 1970».
    const auto masteries = ParseChampionMasteries(kMasteries);

    REQUIRE(masteries.has_value());
    REQUIRE_FALSE(masteries->empty());

    const ChampionMastery& first = (*masteries)[0];
    CHECK(first.level == 42);
    CHECK(first.points == 1234567);
    CHECK(first.last_play_time_ms == 1758830400000LL);
}

TEST_CASE("ParseChampionMasteries: пустой массив и битый JSON") {
    const auto empty = ParseChampionMasteries("[]");
    REQUIRE(empty.has_value());
    CHECK(empty->empty());

    CHECK_FALSE(ParseChampionMasteries("{").has_value());
    CHECK_FALSE(ParseChampionMasteries(R"({"championId": 157})").has_value());
}

TEST_CASE("ParseChampionMasteries: запись без championId пропускается") {
    constexpr const char* kBroken = R"([
      {"championLevel": 7, "championPoints": 100, "lastPlayTime": 1700000000000},
      {"championId": 64, "championLevel": 5, "championPoints": 200,
       "lastPlayTime": 1700000000000}
    ])";

    const auto masteries = ParseChampionMasteries(kBroken);

    REQUIRE(masteries.has_value());
    REQUIRE(masteries->size() == 1);
    CHECK((*masteries)[0].champion_id == 64);
}

TEST_CASE("SplitRiotId режет имя и тег по первому решётке") {
    const auto split = SplitRiotId("Riot Tuxedo#TXC1");

    REQUIRE(split.has_value());
    CHECK(split->first == "Riot Tuxedo");
    CHECK(split->second == "TXC1");
}

TEST_CASE("SplitRiotId: имя с пробелами и коротким тегом") {
    const auto split = SplitRiotId("a b c#RU");

    REQUIRE(split.has_value());
    CHECK(split->first == "a b c");
    CHECK(split->second == "RU");
}

TEST_CASE("SplitRiotId: пустое имя, пустой тег и отсутствие решётки") {
    CHECK_FALSE(SplitRiotId("#TXC1").has_value());
    CHECK_FALSE(SplitRiotId("Riot Tuxedo#").has_value());
    CHECK_FALSE(SplitRiotId("Riot Tuxedo").has_value());
    CHECK_FALSE(SplitRiotId("").has_value());
}

TEST_CASE("ParseActiveRegion достаёт платформу аккаунта") {
    const auto region = ParseActiveRegion(
        R"({"puuid": "abc", "game": "lol", "region": "RU"})");

    REQUIRE(region.has_value());
    CHECK(*region == "RU");
}

TEST_CASE("ParseActiveRegion: нет поля или мусор — nullopt") {
    CHECK_FALSE(ParseActiveRegion(R"({"puuid": "abc", "game": "lol"})").has_value());
    CHECK_FALSE(ParseActiveRegion(R"({"region": ""})").has_value());
    CHECK_FALSE(ParseActiveRegion("[]").has_value());
    CHECK_FALSE(ParseActiveRegion("{").has_value());
}

TEST_CASE("PlatformHost собирает хост из платформенного идентификатора") {
    // Ошибка в этом месте выглядит не как ошибка: запрос к чужой платформе
    // отвечает пустым массивом, то есть «ранга нет» вместо «не туда пришли».
    CHECK(PlatformHost("EUW1").value() == "euw1.api.riotgames.com");
    CHECK(PlatformHost("RU").value() == "ru.api.riotgames.com");
    CHECK(PlatformHost("EUN1").value() == "eun1.api.riotgames.com");
    CHECK(PlatformHost("na1").value() == "na1.api.riotgames.com");
    CHECK_FALSE(PlatformHost("").has_value());
}
