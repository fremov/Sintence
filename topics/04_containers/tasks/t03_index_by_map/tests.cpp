#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "index_by_map.h"

#include "index_builder.h"  // задача 3.3 — с ней сравниваем результат

#include <string>
#include <utility>
#include <vector>

using course::BuildChampionIndex;
using course::BuildChampionIndexFast;
using course::ChampionIndex;
using course::ChampionReport;
using course::FixtureMatchSource;
using course::MatchEntry;
using course::MatchSource;

namespace {

MatchEntry Entry(std::string champion, int kills, int deaths, int assists, bool win, int minions,
                 int duration_seconds) {
    MatchEntry entry;
    entry.champion_name = std::move(champion);
    entry.line.kills = kills;
    entry.line.deaths = deaths;
    entry.line.assists = assists;
    entry.line.win = win;
    entry.line.minions = minions;
    entry.line.duration_seconds = duration_seconds;
    return entry;
}

// Zed идёт первым в записях, но последним по алфавиту — на этом
// и видно разницу в порядке между 3.3 и 4.3.
std::vector<MatchEntry> MixedEntries() {
    return {Entry("Zed", 8, 3, 4, true, 190, 1700), Entry("Ahri", 10, 2, 8, true, 200, 1800),
            Entry("LeeSin", 4, 6, 9, false, 90, 1200), Entry("Ahri", 4, 2, 6, false, 160, 1200)};
}

class CountingSource : public MatchSource {
public:
    CountingSource(std::vector<MatchEntry> entries, bool available)
        : entries_(std::move(entries)), available_(available) {}

    int LoadCount() const { return load_count_; }

    std::string Name() const override { return "counting"; }
    bool IsAvailable() const override { return available_; }

    std::vector<MatchEntry> LoadMatches() const override {
        ++load_count_;
        return available_ ? entries_ : std::vector<MatchEntry>();
    }

private:
    std::vector<MatchEntry> entries_;
    bool available_ = false;
    mutable int load_count_ = 0;
};

}  // namespace

TEST_CASE("индекс собирается из записей источника") {
    const FixtureMatchSource source("fixtures", MixedEntries(), true);
    const ChampionIndex index = BuildChampionIndexFast(source);

    CHECK(index.Size() == 3);

    const ChampionReport* ahri = index.Find("Ahri");
    REQUIRE(ahri != nullptr);
    CHECK(ahri->Games() == 2);

    const ChampionReport* zed = index.Find("Zed");
    REQUIRE(zed != nullptr);
    CHECK(zed->Games() == 1);
}

TEST_CASE("порядок чемпионов — по алфавиту, а не по появлению") {
    // Тот самый изменившийся контракт. В задаче 3.3 первым был бы Zed:
    // он идёт первым в записях. Здесь первым будет Ahri.
    const FixtureMatchSource source("fixtures", MixedEntries(), true);
    const ChampionIndex index = BuildChampionIndexFast(source);

    const std::vector<std::string> names = index.ChampionNames();
    REQUIRE(names.size() == 3);
    CHECK(names[0] == "Ahri");
    CHECK(names[1] == "LeeSin");
    CHECK(names[2] == "Zed");
}

TEST_CASE("содержимое совпадает с реализацией из задачи 3.3") {
    // Callback: две разные внутренности дают одни и те же данные.
    // Различается только порядок — и именно поэтому сравниваем через Find,
    // а не поэлементно по ChampionNames().
    const FixtureMatchSource source("fixtures", MixedEntries(), true);

    const ChampionIndex slow = BuildChampionIndex(source);
    const ChampionIndex fast = BuildChampionIndexFast(source);

    REQUIRE(slow.Size() == fast.Size());

    for (const std::string& name : slow.ChampionNames()) {
        const ChampionReport* from_slow = slow.Find(name);
        const ChampionReport* from_fast = fast.Find(name);

        REQUIRE(from_slow != nullptr);
        REQUIRE(from_fast != nullptr);
        CHECK(from_slow->Games() == from_fast->Games());
        CHECK(from_slow->Kda() == doctest::Approx(from_fast->Kda()));
        CHECK(from_slow->Winrate() == doctest::Approx(from_fast->Winrate()));
    }
}

TEST_CASE("недоступный источник — пустой индекс") {
    // Некорректный вход: клиент закрыт, ключ истёк.
    const FixtureMatchSource source("live-client", MixedEntries(), false);
    const ChampionIndex index = BuildChampionIndexFast(source);

    CHECK(index.Empty());
    CHECK(index.Find("Ahri") == nullptr);
}

TEST_CASE("источник без записей — пустой индекс") {
    // Граничный случай: доступен, но данных нет.
    const FixtureMatchSource source("fixtures", {}, true);
    const ChampionIndex index = BuildChampionIndexFast(source);

    CHECK(index.Empty());
    CHECK(index.ChampionNames().empty());
}

TEST_CASE("записи с пустым именем чемпиона отбрасываются") {
    const FixtureMatchSource source(
        "fixtures", {Entry("", 5, 1, 5, true, 100, 1500), Entry("Ahri", 10, 2, 8, true, 200, 1800)},
        true);
    const ChampionIndex index = BuildChampionIndexFast(source);

    CHECK(index.Size() == 1);
    CHECK(index.Find("") == nullptr);
    REQUIRE(index.Find("Ahri") != nullptr);
    CHECK(index.Find("Ahri")->Games() == 1);
}

TEST_CASE("негодные строки отбрасывает ChampionReport, чемпион остаётся") {
    // Callback к теме 1: duration_seconds <= 0 отбрасывает Add,
    // но сам чемпион из выгрузки не теряется.
    const FixtureMatchSource source("fixtures",
                                    {Entry("Yasuo", 5, 1, 5, true, 100, 0),
                                     Entry("Yasuo", -3, 1, 5, true, 100, 1500)},
                                    true);
    const ChampionIndex index = BuildChampionIndexFast(source);

    CHECK(index.Size() == 1);
    const ChampionReport* yasuo = index.Find("Yasuo");
    REQUIRE(yasuo != nullptr);
    CHECK(yasuo->Games() == 0);
}

TEST_CASE("источник спрашивается ровно один раз") {
    // Для Match-V5 каждый LoadMatches — сетевой запрос под лимитами Riot.
    const CountingSource source(MixedEntries(), true);
    const ChampionIndex index = BuildChampionIndexFast(source);

    CHECK(index.Size() == 3);
    CHECK(source.LoadCount() == 1);
}

TEST_CASE("недоступный источник не спрашивается вообще") {
    // IsAvailable() == false — значит LoadMatches() дёргать незачем.
    const CountingSource source(MixedEntries(), false);
    const ChampionIndex index = BuildChampionIndexFast(source);

    CHECK(index.Empty());
    CHECK(source.LoadCount() == 0);
}

TEST_CASE("работает с любой реализацией интерфейса") {
    const CountingSource counting(MixedEntries(), true);
    const FixtureMatchSource fixtures("fixtures", MixedEntries(), true);

    const ChampionIndex from_counting = BuildChampionIndexFast(counting);
    const ChampionIndex from_fixtures = BuildChampionIndexFast(fixtures);

    CHECK(from_counting.ChampionNames() == from_fixtures.ChampionNames());
}
