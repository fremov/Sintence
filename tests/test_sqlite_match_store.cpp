#include <string>

#include "doctest.h"
#include "sqlite_match_store.h"

using sintence::MatchDetail;
using sintence::MatchParticipant;
using sintence::SqliteMatchStore;

namespace {

MatchParticipant Player(const std::string& puuid, int team, bool win, const std::string& champion) {
    MatchParticipant p;
    p.puuid = puuid;
    p.riot_id = puuid + "#RU1";
    p.team_id = team;
    p.win = win;
    p.champion_id = static_cast<int>(champion.size());
    p.champion = champion;
    p.role = "MIDDLE";
    p.kills = 5;
    p.deaths = 3;
    p.assists = 8;
    p.items = {3152, 3020, 0, 0, 0, 0, 3340};
    p.keystone = 8214;
    return p;
}

MatchDetail Match(const std::string& id, long long started_at, int queue) {
    MatchDetail match;
    match.match_id = id;
    match.platform = "RU";
    match.queue_id = queue;
    match.game_mode = "CLASSIC";
    match.game_version = "15.12.688.6522";
    match.patch = "15.12";
    match.started_at_ms = started_at;
    match.duration_seconds = 1800;
    match.participants = {Player("me", 100, true, "Vladimir"), Player("them", 200, false, "Zed")};
    return match;
}

}  // namespace

TEST_CASE("SqliteMatchStore: матч сохраняется и читается целиком") {
    auto store = SqliteMatchStore::Open(":memory:");
    REQUIRE(store != nullptr);
    CHECK_FALSE(store->HasMatch("RU_1"));

    REQUIRE(store->SaveMatch(Match("RU_1", 1000, 420), R"({"raw":1})"));
    CHECK(store->HasMatch("RU_1"));

    const auto loaded = store->LoadMatch("RU_1");
    REQUIRE(loaded.has_value());
    CHECK(loaded->queue_id == 420);
    CHECK(loaded->patch == "15.12");
    CHECK(loaded->started_at_ms == 1000);
    REQUIRE(loaded->participants.size() == 2);
    const auto* me = loaded->Find("me");
    REQUIRE(me != nullptr);
    CHECK(me->win);
    CHECK(me->champion == "Vladimir");
    CHECK(me->items[0] == 3152);
    CHECK(me->items[6] == 3340);
    CHECK(me->keystone == 8214);
    CHECK(store->LoadRawMatch("RU_1") == std::string(R"({"raw":1})"));
}

TEST_CASE("SqliteMatchStore: повторное сохранение заменяет, а не дублирует") {
    auto store = SqliteMatchStore::Open(":memory:");
    REQUIRE(store != nullptr);
    REQUIRE(store->SaveMatch(Match("RU_1", 1000, 420), "{}"));
    REQUIRE(store->SaveMatch(Match("RU_1", 1000, 420), "{}"));
    CHECK(store->CountMatches("me") == 1);
    CHECK(store->LoadMatch("RU_1")->participants.size() == 2);
}

TEST_CASE("SqliteMatchStore: последние матчи — новые первыми, страницами и по очереди") {
    auto store = SqliteMatchStore::Open(":memory:");
    REQUIRE(store != nullptr);
    store->SaveMatch(Match("RU_1", 1000, 420), "{}");
    store->SaveMatch(Match("RU_3", 3000, 450), "{}");
    store->SaveMatch(Match("RU_2", 2000, 420), "{}");

    const auto all = store->RecentMatches("me", 0, 10);
    REQUIRE(all.size() == 3);
    CHECK(all[0].match_id == "RU_3");
    CHECK(all[1].match_id == "RU_2");
    CHECK(all[2].match_id == "RU_1");

    const auto page = store->RecentMatches("me", 1, 1);
    REQUIRE(page.size() == 1);
    CHECK(page[0].match_id == "RU_2");

    const auto solo = store->RecentMatches("me", 0, 10, {.queue_id = 420});
    REQUIRE(solo.size() == 2);
    CHECK(solo[0].match_id == "RU_2");

    CHECK(store->CountMatches("me") == 3);
    CHECK(store->CountMatches("nobody") == 0);
    CHECK(store->RecentMatches("nobody", 0, 10).empty());
}

TEST_CASE("SqliteMatchStore: timeline отдельно от матча") {
    auto store = SqliteMatchStore::Open(":memory:");
    REQUIRE(store != nullptr);
    CHECK_FALSE(store->LoadTimeline("RU_1").has_value());
    REQUIRE(store->SaveTimeline("RU_1", R"({"frames":[]})"));
    CHECK(store->LoadTimeline("RU_1") == std::string(R"({"frames":[]})"));
}

TEST_CASE("SqliteMatchStore: несуществующий каталог — nullptr") {
    CHECK(SqliteMatchStore::Open("Z:/нет/такого/каталога/history.sqlite") == nullptr);
}

TEST_CASE("SqliteMatchStore: фильтр по чемпиону и по очереди вместе") {
    auto store = SqliteMatchStore::Open(":memory:");
    REQUIRE(store != nullptr);
    MatchDetail ahri = Match("RU_4", 4000, 420);
    ahri.participants[0].champion = "Ahri";
    ahri.participants[0].champion_id = 103;
    store->SaveMatch(Match("RU_1", 1000, 420), "{}");
    store->SaveMatch(Match("RU_2", 2000, 450), "{}");
    store->SaveMatch(ahri, "{}");

    const int vladimir = static_cast<int>(std::string("Vladimir").size());  // см. Player()
    const auto on_vladimir = store->RecentMatches("me", 0, 10, {.champion_id = vladimir});
    REQUIRE(on_vladimir.size() == 2);
    CHECK(on_vladimir[0].match_id == "RU_2");

    const auto solo_vladimir =
        store->RecentMatches("me", 0, 10, {.queue_id = 420, .champion_id = vladimir});
    REQUIRE(solo_vladimir.size() == 1);
    CHECK(solo_vladimir[0].match_id == "RU_1");

    CHECK(store->RecentMatches("me", 0, 10, {.champion_id = 103}).size() == 1);
    // Чемпион соперника в матче — не игры «me» на нём.
    CHECK(store->RecentMatches("me", 0, 10, {.champion_id = 3}).empty());
}

TEST_CASE("SqliteMatchStore: подсказки игроков без учёта регистра, с кириллицей") {
    auto store = SqliteMatchStore::Open(":memory:");
    REQUIRE(store != nullptr);
    MatchDetail first = Match("RU_1", 1000, 420);
    first.participants[0].riot_id = "Амплификатор#3138";
    first.participants[1].riot_id = "Ёжик в тумане#RU1";
    MatchDetail second = Match("RU_2", 2000, 420);
    second.participants[0].riot_id = "Амплификатор#3138";
    second.participants[1].riot_id = "Большой Ампер#EUW";
    store->SaveMatch(first, "{}");
    store->SaveMatch(second, "{}");

    const auto amp = store->SearchPlayers("амп", 10);
    REQUIRE(amp.size() == 2);
    CHECK(amp[0].riot_id == "Амплификатор#3138");  // имя начинается с запроса
    CHECK(amp[0].games == 2);
    CHECK(amp[1].riot_id == "Большой Ампер#EUW");   // запрос внутри имени

    CHECK(store->SearchPlayers("ЕЖИК", 10).size() == 1);  // ё ищется как е
    CHECK(store->SearchPlayers("#euw", 10).size() == 1);  // тег тоже
    CHECK(store->SearchPlayers("амп", 1).size() == 1);
    CHECK(store->SearchPlayers("", 10).empty());
    CHECK(store->SearchPlayers("нет такого", 10).empty());
}

TEST_CASE("SqliteMatchStore: смерти до 10-й минуты считаются при сохранении timeline") {
    auto store = SqliteMatchStore::Open(":memory:");
    REQUIRE(store != nullptr);
    // Игрок 1 умер на 3-й и на 12-й минуте, игрок 2 — не умирал.
    const std::string timeline = R"({
      "metadata": {"participants": ["me", "them"]},
      "info": {"frames": [
        {"timestamp": 0, "events": [
          {"type": "CHAMPION_KILL", "killerId": 2, "victimId": 1, "timestamp": 180000}]},
        {"timestamp": 720000, "events": [
          {"type": "CHAMPION_KILL", "killerId": 2, "victimId": 1, "timestamp": 720000}]}
      ]}
    })";
    CHECK_FALSE(store->HasTimeline("RU_1"));
    REQUIRE(store->SaveTimeline("RU_1", timeline));
    CHECK(store->HasTimeline("RU_1"));

    const auto mine = store->EarlyDeaths("me");
    REQUIRE(mine.size() == 1);
    CHECK(mine.at("RU_1") == 1);
    CHECK(store->EarlyDeaths("them").at("RU_1") == 0);
    CHECK(store->EarlyDeaths("nobody").empty());
}
