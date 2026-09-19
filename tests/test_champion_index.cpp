#include "doctest.h"

#include "champion_index.h"

#include <string>
#include <type_traits>
#include <utility>

using course::ChampionIndex;
using course::ChampionReport;
using course::MatchLine;

namespace {

MatchLine Line(int kills, int deaths, int assists, bool win, int minions, int duration_seconds) {
    MatchLine line;
    line.kills = kills;
    line.deaths = deaths;
    line.assists = assists;
    line.win = win;
    line.minions = minions;
    line.duration_seconds = duration_seconds;
    return line;
}

// Отчёт по Ahri: два матча, 10/2/8 победа и 4/2/6 поражение.
ChampionReport AhriReport() {
    ChampionReport report("Ahri");
    report.Add(Line(10, 2, 8, true, 200, 1800));
    report.Add(Line(4, 2, 6, false, 160, 1200));
    return report;
}

}  // namespace

TEST_CASE("новый индекс пуст") {
    // Граничный случай: ноль отчётов, поиск ничего не находит и не падает.
    const ChampionIndex index;
    CHECK(index.Empty());
    CHECK(index.Size() == 0);
    CHECK(index.Find("Ahri") == nullptr);
    CHECK(index.ChampionNames().empty());
}

TEST_CASE("добавленный отчёт находится по имени") {
    ChampionIndex index;
    index.AddReport(AhriReport());

    CHECK_FALSE(index.Empty());
    CHECK(index.Size() == 1);

    const ChampionReport* found = index.Find("Ahri");
    REQUIRE(found != nullptr);
    CHECK(found->ChampionName() == "Ahri");
    CHECK(found->Games() == 2);
}

TEST_CASE("метрики темы 1 доступны через индекс") {
    // Callback: ChampionReport продолжает работать внутри нового типа.
    // (10 + 4 килла + 8 + 6 ассистов) / 4 смерти = 7.0
    ChampionIndex index;
    index.AddReport(AhriReport());

    const ChampionReport* report = index.Find("Ahri");
    REQUIRE(report != nullptr);
    CHECK(report->Kda() == doctest::Approx(7.0));
    CHECK(report->Winrate() == doctest::Approx(0.5));
    CHECK_FALSE(report->HasEnoughData());
}

TEST_CASE("неизвестный чемпион — nullptr") {
    // Некорректный вход: запрос того, чего в индексе нет.
    ChampionIndex index;
    index.AddReport(AhriReport());

    CHECK(index.Find("Yasuo") == nullptr);
    CHECK(index.Find("") == nullptr);
    CHECK(index.Find("ahri") == nullptr);  // регистр важен: нормализация — не забота индекса
}

TEST_CASE("повторное добавление заменяет отчёт") {
    ChampionIndex index;
    index.AddReport(AhriReport());

    ChampionReport updated("Ahri");
    updated.Add(Line(1, 1, 1, true, 100, 900));
    index.AddReport(std::move(updated));

    CHECK(index.Size() == 1);
    const ChampionReport* found = index.Find("Ahri");
    REQUIRE(found != nullptr);
    CHECK(found->Games() == 1);
}

TEST_CASE("несколько чемпионов, имена в порядке добавления") {
    ChampionIndex index;
    index.AddReport(AhriReport());
    index.AddReport(ChampionReport("LeeSin"));
    index.AddReport(ChampionReport("Thresh"));

    CHECK(index.Size() == 3);

    const std::vector<std::string> names = index.ChampionNames();
    REQUIRE(names.size() == 3);
    CHECK(names[0] == "Ahri");
    CHECK(names[1] == "LeeSin");
    CHECK(names[2] == "Thresh");
}

TEST_CASE("копия индекса независима") {
    // Правило нуля в действии: ты не написал копирующий конструктор,
    // а копия всё равно глубокая — потому что std::vector умеет копировать себя.
    ChampionIndex original;
    original.AddReport(AhriReport());

    ChampionIndex copy(original);
    copy.AddReport(ChampionReport("Yasuo"));

    CHECK(original.Size() == 1);
    CHECK(copy.Size() == 2);
    CHECK(original.Find("Yasuo") == nullptr);
    CHECK(copy.Find("Yasuo") != nullptr);

    // Указатели на отчёты у оригинала и копии разные — это разные объекты.
    CHECK(original.Find("Ahri") != copy.Find("Ahri"));
}

TEST_CASE("перемещение индекса сохраняет данные") {
    ChampionIndex source;
    source.AddReport(AhriReport());
    source.AddReport(ChampionReport("LeeSin"));

    const ChampionIndex moved(std::move(source));

    CHECK(moved.Size() == 2);
    REQUIRE(moved.Find("Ahri") != nullptr);
    CHECK(moved.Find("Ahri")->Games() == 2);
}

TEST_CASE("перемещение бесплатное и не бросает") {
    // Тоже подарок от правила нуля: раз все поля перемещаются без исключений,
    // весь класс тоже. Ни одной строки для этого писать не нужно.
    CHECK(std::is_nothrow_move_constructible<ChampionIndex>::value);
    CHECK(std::is_nothrow_move_assignable<ChampionIndex>::value);
    CHECK(std::is_copy_constructible<ChampionIndex>::value);
}

TEST_CASE("const-объект отдаёт данные, но не даёт их менять") {
    // Возврат к теме 1. Если этот тест не компилируется — где-то потерян const.
    ChampionIndex mutable_index;
    mutable_index.AddReport(AhriReport());

    const ChampionIndex& index = mutable_index;
    CHECK(index.Size() == 1);
    CHECK_FALSE(index.Empty());
    CHECK(index.Find("Ahri") != nullptr);
    CHECK(index.ChampionNames().size() == 1);
}
