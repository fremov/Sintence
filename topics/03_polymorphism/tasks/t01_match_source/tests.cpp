#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "match_source.h"

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using course::FixtureMatchSource;
using course::MatchEntry;
using course::MatchLine;
using course::MatchSource;

namespace {

MatchEntry Entry(std::string champion, int kills, int deaths, int assists, bool win) {
    MatchEntry entry;
    entry.champion_name = std::move(champion);
    entry.line.kills = kills;
    entry.line.deaths = deaths;
    entry.line.assists = assists;
    entry.line.win = win;
    entry.line.minions = 180;
    entry.line.duration_seconds = 1800;
    return entry;
}

std::vector<MatchEntry> TwoEntries() {
    return {Entry("Ahri", 10, 2, 8, true), Entry("LeeSin", 4, 6, 9, false)};
}

// Функция знает только интерфейс. Ни одного упоминания FixtureMatchSource —
// в этом весь смысл темы.
std::size_t CountMatches(const MatchSource& source) {
    return source.LoadMatches().size();
}

}  // namespace

TEST_CASE("MatchSource — абстрактный интерфейс с виртуальным деструктором") {
    // Объект интерфейса создать нельзя: реализовать методы обязан наследник.
    CHECK(std::is_abstract<MatchSource>::value);

    // Без виртуального деструктора удаление через указатель на базу
    // не вызвало бы деструктор наследника — утечка, которую компилятор не покажет.
    CHECK(std::has_virtual_destructor<MatchSource>::value);

    // Интерфейс не копируется: копия через базовую ссылку взяла бы только
    // базовую часть объекта.
    CHECK_FALSE(std::is_copy_constructible<MatchSource>::value);
    CHECK_FALSE(std::is_copy_assignable<MatchSource>::value);
}

TEST_CASE("FixtureMatchSource отдаёт имя, доступность и записи") {
    const FixtureMatchSource source("fixtures", TwoEntries(), true);

    CHECK(source.Name() == "fixtures");
    CHECK(source.IsAvailable());

    const std::vector<MatchEntry> entries = source.LoadMatches();
    REQUIRE(entries.size() == 2);
    CHECK(entries[0].champion_name == "Ahri");
    CHECK(entries[0].line.kills == 10);
    CHECK(entries[0].line.win);
    CHECK(entries[1].champion_name == "LeeSin");
    CHECK(entries[1].line.deaths == 6);
    CHECK_FALSE(entries[1].line.win);
}

TEST_CASE("недоступный источник — пустой вектор, а не исключение") {
    // Некорректный вход по смыслу задачи: записи есть, но читать их сейчас нельзя.
    // Live Client Data вне матча ведёт себя именно так, и анализатор это переживает.
    const FixtureMatchSource source("live-client", TwoEntries(), false);

    CHECK(source.Name() == "live-client");
    CHECK_FALSE(source.IsAvailable());
    CHECK(source.LoadMatches().empty());
}

TEST_CASE("источник без записей") {
    // Граничный случай: доступен, но данных нет. Это не то же самое,
    // что недоступный источник, и путать их нельзя.
    const FixtureMatchSource source("fixtures", {}, true);

    CHECK(source.IsAvailable());
    CHECK(source.LoadMatches().empty());
}

TEST_CASE("вызов через ссылку на базу выбирает реализацию наследника") {
    // Динамический полиморфизм: статический тип — MatchSource,
    // фактический — FixtureMatchSource, и вызывается второй.
    const FixtureMatchSource fixtures("fixtures", TwoEntries(), true);
    const MatchSource& source = fixtures;

    CHECK(source.Name() == "fixtures");
    CHECK(source.IsAvailable());
    CHECK(CountMatches(source) == 2);
}

TEST_CASE("удаление через указатель на базу не течёт") {
    // Если деструктор базы перестанет быть виртуальным, этот тест
    // сам по себе останется зелёным, а ASan напечатает detected memory leaks.
    std::unique_ptr<MatchSource> source =
        std::make_unique<FixtureMatchSource>("fixtures", TwoEntries(), true);

    REQUIRE(source != nullptr);
    CHECK(source->Name() == "fixtures");
    CHECK(CountMatches(*source) == 2);
}

TEST_CASE("LoadMatches возвращает копию, а не доступ к внутреннему вектору") {
    const FixtureMatchSource source("fixtures", TwoEntries(), true);

    std::vector<MatchEntry> first = source.LoadMatches();
    first.clear();

    // Испортили полученный вектор — источник этого не заметил.
    CHECK(source.LoadMatches().size() == 2);
}

TEST_CASE("несколько источников независимы") {
    const FixtureMatchSource first("fixtures", TwoEntries(), true);
    const FixtureMatchSource second("match-v5", {Entry("Thresh", 1, 1, 20, true)}, true);

    CHECK(first.Name() == "fixtures");
    CHECK(second.Name() == "match-v5");
    CHECK(CountMatches(first) == 2);

    const std::vector<MatchEntry> second_entries = second.LoadMatches();
    REQUIRE(second_entries.size() == 1);
    CHECK(second_entries[0].champion_name == "Thresh");
}
