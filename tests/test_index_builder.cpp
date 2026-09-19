#include "doctest.h"

#include "index_builder.h"

#include <string>
#include <utility>
#include <vector>

using course::BuildChampionIndex;
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

// Две игры на Ahri (победа и поражение) и одна на LeeSin.
std::vector<MatchEntry> MixedEntries() {
    return {Entry("Ahri", 10, 2, 8, true, 200, 1800), Entry("LeeSin", 4, 6, 9, false, 90, 1200),
            Entry("Ahri", 4, 2, 6, false, 160, 1200)};
}

// Источник, который считает, сколько раз его спрашивали.
// Для Match-V5 каждый вызов — сетевой запрос, поэтому их число важно.
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
    const ChampionIndex index = BuildChampionIndex(source);

    CHECK(index.Size() == 2);

    const ChampionReport* ahri = index.Find("Ahri");
    REQUIRE(ahri != nullptr);
    CHECK(ahri->Games() == 2);

    const ChampionReport* lee = index.Find("LeeSin");
    REQUIRE(lee != nullptr);
    CHECK(lee->Games() == 1);
}

TEST_CASE("метрики тем 1 и 2 считаются по собранному индексу") {
    // Callback: (10 + 4 килла + 8 + 6 ассистов) / 4 смерти = 7.0, одна победа из двух.
    const FixtureMatchSource source("fixtures", MixedEntries(), true);
    const ChampionIndex index = BuildChampionIndex(source);

    const ChampionReport* ahri = index.Find("Ahri");
    REQUIRE(ahri != nullptr);
    CHECK(ahri->Kda() == doctest::Approx(7.0));
    CHECK(ahri->Winrate() == doctest::Approx(0.5));
    CHECK_FALSE(ahri->HasEnoughData());
}

TEST_CASE("порядок чемпионов — порядок первого появления") {
    const FixtureMatchSource source("fixtures", MixedEntries(), true);
    const ChampionIndex index = BuildChampionIndex(source);

    const std::vector<std::string> names = index.ChampionNames();
    REQUIRE(names.size() == 2);
    CHECK(names[0] == "Ahri");
    CHECK(names[1] == "LeeSin");
}

TEST_CASE("недоступный источник — пустой индекс") {
    // Некорректный вход: живой игры нет, ключ истёк, клиент закрыт.
    const FixtureMatchSource source("live-client", MixedEntries(), false);
    const ChampionIndex index = BuildChampionIndex(source);

    CHECK(index.Empty());
    CHECK(index.Size() == 0);
    CHECK(index.Find("Ahri") == nullptr);
}

TEST_CASE("источник без записей — пустой индекс") {
    // Граничный случай: доступен, но данных нет.
    const FixtureMatchSource source("fixtures", {}, true);
    const ChampionIndex index = BuildChampionIndex(source);

    CHECK(index.Empty());
    CHECK(index.ChampionNames().empty());
}

TEST_CASE("записи с пустым именем чемпиона отбрасываются") {
    // Некорректный вход: отчёт без имени нельзя найти через Find,
    // и в таблице ему нечего показать.
    const FixtureMatchSource source("fixtures",
                                    {Entry("", 5, 1, 5, true, 100, 1500),
                                     Entry("Ahri", 10, 2, 8, true, 200, 1800)},
                                    true);
    const ChampionIndex index = BuildChampionIndex(source);

    CHECK(index.Size() == 1);
    CHECK(index.Find("") == nullptr);
    REQUIRE(index.Find("Ahri") != nullptr);
    CHECK(index.Find("Ahri")->Games() == 1);
}

TEST_CASE("негодные строки матча отбрасывает ChampionReport, а не builder") {
    // Callback к теме 1: duration_seconds <= 0 и отрицательные значения
    // отбрасывает Add. Чемпион при этом остаётся в индексе с нулём игр —
    // он был в выгрузке, и терять его молча нельзя.
    const FixtureMatchSource source("fixtures",
                                    {Entry("Yasuo", 5, 1, 5, true, 100, 0),
                                     Entry("Yasuo", -3, 1, 5, true, 100, 1500)},
                                    true);
    const ChampionIndex index = BuildChampionIndex(source);

    CHECK(index.Size() == 1);
    const ChampionReport* yasuo = index.Find("Yasuo");
    REQUIRE(yasuo != nullptr);
    CHECK(yasuo->Games() == 0);
    CHECK(yasuo->Kda() == doctest::Approx(0.0));
}

TEST_CASE("регистр имени сохраняется как есть") {
    // Нормализацией занимается Champion из темы 1, а не этот слой.
    const FixtureMatchSource source("fixtures",
                                    {Entry("ahri", 1, 1, 1, true, 10, 600),
                                     Entry("Ahri", 2, 1, 1, true, 10, 600)},
                                    true);
    const ChampionIndex index = BuildChampionIndex(source);

    CHECK(index.Size() == 2);
    CHECK(index.Find("ahri") != nullptr);
    CHECK(index.Find("Ahri") != nullptr);
}

TEST_CASE("источник спрашивается ровно один раз") {
    // Для Match-V5 каждый LoadMatches — сетевой запрос под лимитами Riot.
    const CountingSource source(MixedEntries(), true);
    const ChampionIndex index = BuildChampionIndex(source);

    CHECK(index.Size() == 2);
    CHECK(source.LoadCount() == 1);
}

TEST_CASE("builder работает с любой реализацией интерфейса") {
    // Ни одного упоминания конкретного источника внутри — иначе этот тест
    // пришлось бы переписывать при каждой новой интеграции.
    const CountingSource counting(MixedEntries(), true);
    const FixtureMatchSource fixtures("fixtures", MixedEntries(), true);

    const ChampionIndex from_counting = BuildChampionIndex(counting);
    const ChampionIndex from_fixtures = BuildChampionIndex(fixtures);

    CHECK(from_counting.Size() == 2);
    CHECK(from_counting.Size() == from_fixtures.Size());
    CHECK(from_counting.ChampionNames() == from_fixtures.ChampionNames());
}

TEST_CASE("индекс переживает смерть источника") {
    // Индекс владеет своими отчётами: он копирует данные, а не ссылается
    // на внутренности источника. К теме 17 источник будет жить в другом потоке.
    ChampionIndex index;
    {
        const FixtureMatchSource source("fixtures", MixedEntries(), true);
        index = BuildChampionIndex(source);
    }

    CHECK(index.Size() == 2);
    REQUIRE(index.Find("Ahri") != nullptr);
    CHECK(index.Find("Ahri")->Games() == 2);
}
