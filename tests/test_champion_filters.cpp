#include "doctest.h"

#include "champion_filters.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

using course::ChampionIndex;
using course::ChampionReport;
using course::ChampionsWithMinGames;
using course::CountChampionsWithEnoughData;
using course::HasChampionAboveWinrate;
using course::MatchLine;

namespace {

// Отчёт с заданным числом побед и поражений.
// Длительность и миньоны здесь не важны: проверяются games и winrate.
ChampionReport MakeReport(std::string name, int wins, int losses) {
    ChampionReport report(std::move(name));
    for (int i = 0; i < wins; ++i) {
        report.Add(MatchLine{5, 2, 7, true, 150, 1800});
    }
    for (int i = 0; i < losses; ++i) {
        report.Add(MatchLine{2, 8, 3, false, 120, 1800});
    }
    return report;
}

// Индекс: Ahri — 40 игр (30 побед), LeeSin — 10 игр (9 побед),
// Zed — 30 игр ровно (12 побед).
ChampionIndex MakeIndex() {
    ChampionIndex index;
    index.AddReport(MakeReport("Ahri", 30, 10));
    index.AddReport(MakeReport("LeeSin", 9, 1));
    index.AddReport(MakeReport("Zed", 12, 18));
    return index;
}

}  // namespace

TEST_CASE("CountChampionsWithEnoughData считает только тех, у кого хватает игр") {
    // Порог kMinGamesForConclusion == 30: Ahri (40) и Zed (30) проходят,
    // LeeSin (10) — нет.
    CHECK(CountChampionsWithEnoughData(MakeIndex()) == 2);
}

TEST_CASE("CountChampionsWithEnoughData на пустом индексе") {
    const ChampionIndex index;

    CHECK(CountChampionsWithEnoughData(index) == 0);
}

TEST_CASE("CountChampionsWithEnoughData: никто не набрал") {
    ChampionIndex index;
    index.AddReport(MakeReport("Yasuo", 1, 1));

    CHECK(CountChampionsWithEnoughData(index) == 0);
}

TEST_CASE("ChampionsWithMinGames отбирает по порогу") {
    const auto names = ChampionsWithMinGames(MakeIndex(), 30);

    REQUIRE(names.size() == 2);
    CHECK(names[0] == "Ahri");
    CHECK(names[1] == "Zed");
}

TEST_CASE("ChampionsWithMinGames: сравнение нестрогое") {
    // У Zed ровно 30 игр — он обязан попасть.
    const auto names = ChampionsWithMinGames(MakeIndex(), 30);

    const bool has_zed = std::find(names.begin(), names.end(), "Zed") != names.end();
    CHECK(has_zed);
}

TEST_CASE("ChampionsWithMinGames сохраняет порядок ChampionNames") {
    // Порядок добавления, а не алфавит: сортировка — работа задачи 5.1.
    ChampionIndex index;
    index.AddReport(MakeReport("Zed", 20, 0));
    index.AddReport(MakeReport("Ahri", 20, 0));

    const auto names = ChampionsWithMinGames(index, 1);

    REQUIRE(names.size() == 2);
    CHECK(names[0] == "Zed");
    CHECK(names[1] == "Ahri");
}

TEST_CASE("ChampionsWithMinGames: порог 0 и отрицательный — все") {
    CHECK(ChampionsWithMinGames(MakeIndex(), 0).size() == 3);
    CHECK(ChampionsWithMinGames(MakeIndex(), -5).size() == 3);
}

TEST_CASE("ChampionsWithMinGames: порог выше всех") {
    CHECK(ChampionsWithMinGames(MakeIndex(), 1000).empty());
}

TEST_CASE("ChampionsWithMinGames на пустом индексе") {
    const ChampionIndex index;

    CHECK(ChampionsWithMinGames(index, 1).empty());
}

TEST_CASE("HasChampionAboveWinrate находит подходящего") {
    // Ahri: 30 побед из 40 = 0.75, данных достаточно.
    CHECK(HasChampionAboveWinrate(MakeIndex(), 0.7));
}

TEST_CASE("HasChampionAboveWinrate: сравнение строгое") {
    // Ровно 0.75 — это не «выше 0.75».
    CHECK_FALSE(HasChampionAboveWinrate(MakeIndex(), 0.75));
}

TEST_CASE("HasChampionAboveWinrate игнорирует тех, у кого мало данных") {
    // LeeSin: 9 побед из 10 = 0.9, но игр всего 10 — вывод делать не на чем.
    // Главная проверка этой функции: высокий винрейт на малой выборке
    // не должен считаться находкой.
    ChampionIndex index;
    index.AddReport(MakeReport("LeeSin", 9, 1));

    CHECK_FALSE(HasChampionAboveWinrate(index, 0.6));
}

TEST_CASE("HasChampionAboveWinrate на пустом индексе") {
    const ChampionIndex index;

    CHECK_FALSE(HasChampionAboveWinrate(index, 0.1));
}

TEST_CASE("HasChampionAboveWinrate: никто не проходит по винрейту") {
    CHECK_FALSE(HasChampionAboveWinrate(MakeIndex(), 0.99));
}
