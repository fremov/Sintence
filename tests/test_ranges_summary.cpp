#include "doctest.h"

#include "ranges_summary.h"

#include <string>
#include <utility>
#include <vector>

using sintence::BuildSummaries;
using sintence::ChampionIndex;
using sintence::ChampionReport;
using sintence::ChampionSummary;
using sintence::MatchLine;
using sintence::TopByWinrate;
using sintence::TotalGames;

namespace {

ChampionReport MakeReport(std::string name, int wins, int losses) {
    ChampionReport report(std::move(name));
    for (int i = 0; i < wins; ++i) {
        report.Add(MatchLine{10, 2, 5, true, 200, 1800});
    }
    for (int i = 0; i < losses; ++i) {
        report.Add(MatchLine{1, 9, 2, false, 100, 1800});
    }
    return report;
}

// Ahri  — 40 игр, 30 побед (0.75), данных достаточно
// LeeSin — 10 игр, 9 побед  (0.90), данных МАЛО
// Zed   — 30 игр, 12 побед  (0.40), данных достаточно
ChampionIndex MakeIndex() {
    ChampionIndex index;
    index.AddReport(MakeReport("Ahri", 30, 10));
    index.AddReport(MakeReport("LeeSin", 9, 1));
    index.AddReport(MakeReport("Zed", 12, 18));
    return index;
}

ChampionSummary Row(std::string name, int games, double winrate, bool enough) {
    return ChampionSummary{std::move(name), games, winrate, 0.0, enough};
}

}  // namespace

TEST_CASE("BuildSummaries строит по строке на чемпиона") {
    const auto rows = BuildSummaries(MakeIndex());

    REQUIRE(rows.size() == 3);
    CHECK(rows[0].champion_name == "Ahri");
    CHECK(rows[1].champion_name == "LeeSin");
    CHECK(rows[2].champion_name == "Zed");
}

TEST_CASE("BuildSummaries сохраняет порядок ChampionNames") {
    ChampionIndex index;
    index.AddReport(MakeReport("Zed", 5, 5));
    index.AddReport(MakeReport("Ahri", 5, 5));

    const auto rows = BuildSummaries(index);

    REQUIRE(rows.size() == 2);
    CHECK(rows[0].champion_name == "Zed");
    CHECK(rows[1].champion_name == "Ahri");
}

TEST_CASE("BuildSummaries заполняет метрики из отчёта") {
    const auto rows = BuildSummaries(MakeIndex());

    REQUIRE(rows.size() == 3);
    CHECK(rows[0].games == 40);
    CHECK(rows[0].winrate == doctest::Approx(0.75));
    CHECK(rows[0].enough_data);

    CHECK(rows[1].games == 10);
    CHECK(rows[1].winrate == doctest::Approx(0.9));
    CHECK_FALSE(rows[1].enough_data);  // 10 игр — мало

    CHECK(rows[2].games == 30);
    CHECK(rows[2].winrate == doctest::Approx(0.4));
    CHECK(rows[2].enough_data);  // ровно 30 — достаточно
}

TEST_CASE("BuildSummaries считает KDA") {
    // Ahri: 30 побед по 10/2/5 и 10 поражений по 1/9/2.
    // Суммарно: kills 310, deaths 150, assists 170 -> (310+170)/150 = 3.2
    const auto rows = BuildSummaries(MakeIndex());

    REQUIRE(!rows.empty());
    CHECK(rows[0].kda == doctest::Approx(3.2));
}

TEST_CASE("BuildSummaries на пустом индексе") {
    const ChampionIndex index;

    CHECK(BuildSummaries(index).empty());
}

TEST_CASE("TopByWinrate сортирует по убыванию винрейта") {
    const std::vector<ChampionSummary> rows = {
        Row("Ahri", 40, 0.75, true),
        Row("Zed", 30, 0.40, true),
        Row("Thresh", 50, 0.60, true),
    };

    const auto top = TopByWinrate(rows, 10);

    REQUIRE(top.size() == 3);
    CHECK(top[0].champion_name == "Ahri");
    CHECK(top[1].champion_name == "Thresh");
    CHECK(top[2].champion_name == "Zed");
}

TEST_CASE("TopByWinrate выбрасывает тех, у кого мало данных") {
    // Главная проверка задачи: 90% по десяти играм не попадает в топ,
    // даже будучи самым высоким винрейтом в списке.
    const std::vector<ChampionSummary> rows = {
        Row("Ahri", 40, 0.75, true),
        Row("LeeSin", 10, 0.90, false),
        Row("Zed", 30, 0.40, true),
    };

    const auto top = TopByWinrate(rows, 10);

    REQUIRE(top.size() == 2);
    CHECK(top[0].champion_name == "Ahri");
    CHECK(top[1].champion_name == "Zed");
}

TEST_CASE("TopByWinrate уважает limit") {
    const std::vector<ChampionSummary> rows = {
        Row("Ahri", 40, 0.75, true),
        Row("Thresh", 50, 0.60, true),
        Row("Zed", 30, 0.40, true),
    };

    const auto top = TopByWinrate(rows, 2);

    REQUIRE(top.size() == 2);
    CHECK(top[0].champion_name == "Ahri");
    CHECK(top[1].champion_name == "Thresh");
}

TEST_CASE("граница: limit == 0") {
    const std::vector<ChampionSummary> rows = {Row("Ahri", 40, 0.75, true)};

    CHECK(TopByWinrate(rows, 0).empty());
}

TEST_CASE("граница: limit больше числа подходящих строк") {
    const std::vector<ChampionSummary> rows = {
        Row("Ahri", 40, 0.75, true),
        Row("LeeSin", 10, 0.90, false),
    };

    const auto top = TopByWinrate(rows, 100);

    REQUIRE(top.size() == 1);
    CHECK(top[0].champion_name == "Ahri");
}

TEST_CASE("TopByWinrate: никто не прошёл фильтр") {
    const std::vector<ChampionSummary> rows = {
        Row("LeeSin", 10, 0.90, false),
        Row("Yasuo", 3, 1.00, false),
    };

    CHECK(TopByWinrate(rows, 10).empty());
}

TEST_CASE("TopByWinrate на пустом входе") {
    CHECK(TopByWinrate({}, 5).empty());
}

TEST_CASE("TotalGames складывает все игры") {
    const std::vector<ChampionSummary> rows = {
        Row("Ahri", 40, 0.75, true),
        Row("LeeSin", 10, 0.90, false),
        Row("Zed", 30, 0.40, true),
    };

    CHECK(TotalGames(rows) == 80);
}

TEST_CASE("TotalGames считает и тех, у кого мало данных") {
    const std::vector<ChampionSummary> rows = {
        Row("LeeSin", 10, 0.90, false),
        Row("Yasuo", 3, 1.00, false),
    };

    CHECK(TotalGames(rows) == 13);
}

TEST_CASE("TotalGames на пустой сводке") {
    CHECK(TotalGames({}) == 0);
}

TEST_CASE("связка: индекс -> сводка -> топ -> сумма") {
    // Все три функции в одном потоке данных — так они и будут стоять
    // в анализаторе.
    const auto rows = BuildSummaries(MakeIndex());
    const auto top = TopByWinrate(rows, 1);

    REQUIRE(top.size() == 1);
    CHECK(top[0].champion_name == "Ahri");
    CHECK(TotalGames(rows) == 80);
}
