#include "doctest.h"

#include "champion_report.h"

using sintence::ChampionReport;
using sintence::MatchLine;

namespace {

MatchLine MakeLine(int kills, int deaths, int assists, bool win, int minions,
                   int duration_seconds) {
    MatchLine line;
    line.kills = kills;
    line.deaths = deaths;
    line.assists = assists;
    line.win = win;
    line.minions = minions;
    line.duration_seconds = duration_seconds;
    return line;
}

}  // namespace

TEST_CASE("пустой отчёт отвечает нулями") {
    // Граничный случай: ни одного матча. Здесь три потенциальных деления
    // на ноль подряд — и ни одно не должно случиться.
    const ChampionReport report("Ahri");
    CHECK(report.ChampionName() == "Ahri");
    CHECK(report.Games() == 0);
    CHECK(report.Kda() == doctest::Approx(0.0));
    CHECK(report.Winrate() == doctest::Approx(0.0));
    CHECK(report.CsPerMinute() == doctest::Approx(0.0));
    CHECK_FALSE(report.HasEnoughData());
}

TEST_CASE("метрики одного матча") {
    ChampionReport report("Ahri");
    report.Add(MakeLine(10, 5, 5, true, 200, 1800));  // 30 минут

    CHECK(report.Games() == 1);
    CHECK(report.Kda() == doctest::Approx(3.0));            // (10 + 5) / 5
    CHECK(report.Winrate() == doctest::Approx(1.0));
    CHECK(report.CsPerMinute() == doctest::Approx(6.6667).epsilon(0.001));
}

TEST_CASE("KDA считается по сумме, а не как среднее средних") {
    // Матч 1: (10 + 5) / 5 = 3.0
    // Матч 2: (2 + 6) / 8 = 1.0
    // Среднее средних дало бы 2.0 — и это неверный ответ.
    // Правильно: (10 + 2 + 5 + 6) / (5 + 8) = 23 / 13 = 1.769...
    ChampionReport report("Ahri");
    report.Add(MakeLine(10, 5, 5, true, 200, 1800));
    report.Add(MakeLine(2, 8, 6, false, 120, 1200));

    CHECK(report.Games() == 2);
    CHECK(report.Kda() == doctest::Approx(23.0 / 13.0));
    CHECK(report.Winrate() == doctest::Approx(0.5));
}

TEST_CASE("CS/min тоже считается по сумме") {
    // (200 + 120) миньонов за (1800 + 1200) секунд = 320 / 50 минут = 6.4.
    // Среднее по матчам дало бы 6.33 — близко, но неправильно,
    // и разойдётся сильнее, когда длительности будут отличаться в разы.
    ChampionReport report("Ahri");
    report.Add(MakeLine(10, 5, 5, true, 200, 1800));
    report.Add(MakeLine(2, 8, 6, false, 120, 1200));

    CHECK(report.CsPerMinute() == doctest::Approx(6.4));
}

TEST_CASE("ноль смертей не делит на ноль") {
    ChampionReport report("Jinx");
    report.Add(MakeLine(12, 0, 3, true, 300, 1500));
    CHECK(report.Kda() == doctest::Approx(15.0));  // (12 + 3) / 1
}

TEST_CASE("испорченные строки отбрасываются целиком") {
    // Некорректный вход. gameDuration == 0 бывает у ремейков,
    // отрицательные значения — след от null в JSON.
    ChampionReport report("Thresh");
    report.Add(MakeLine(4, 2, 20, true, 30, 1500));

    report.Add(MakeLine(5, 1, 10, true, 100, 0));     // нулевая длительность
    report.Add(MakeLine(5, 1, 10, true, 100, -600));  // отрицательная
    report.Add(MakeLine(-1, 1, 10, true, 100, 900));  // отрицательные убийства
    report.Add(MakeLine(5, 1, 10, true, -50, 900));   // отрицательные миньоны

    CHECK(report.Games() == 1);
    CHECK(report.Winrate() == doctest::Approx(1.0));
    CHECK(report.CsPerMinute() == doctest::Approx(1.2));  // 30 / 25 минут
}

TEST_CASE("HasEnoughData не даёт делать выводы по маленькой выборке") {
    ChampionReport report("Yasuo");
    for (int i = 0; i < ChampionReport::kMinGamesForConclusion - 1; ++i) {
        report.Add(MakeLine(5, 5, 5, i % 2 == 0, 150, 1500));
    }
    CHECK(report.Games() == ChampionReport::kMinGamesForConclusion - 1);
    CHECK_FALSE(report.HasEnoughData());

    report.Add(MakeLine(5, 5, 5, true, 150, 1500));
    CHECK(report.HasEnoughData());
}

TEST_CASE("все метрики доступны у const-объекта") {
    // Если этот тест не компилируется — где-то потерян const.
    ChampionReport mutable_report("Ahri");
    mutable_report.Add(MakeLine(10, 5, 5, true, 200, 1800));

    const ChampionReport& report = mutable_report;
    CHECK(report.ChampionName() == "Ahri");
    CHECK(report.Games() == 1);
    CHECK(report.Kda() == doctest::Approx(3.0));
    CHECK(report.Winrate() == doctest::Approx(1.0));
    CHECK(report.CsPerMinute() == doctest::Approx(6.6667).epsilon(0.001));
    CHECK(report.HasEnoughData() == false);
}
