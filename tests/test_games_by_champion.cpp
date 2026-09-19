#include "doctest.h"

#include "games_by_champion.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

using course::CountGamesByChampion;
using course::GamesOf;
using course::MatchEntry;
using course::MostPlayedChampion;

namespace {

MatchEntry Entry(std::string champion) {
    MatchEntry entry;
    entry.champion_name = std::move(champion);
    entry.line.kills = 5;
    entry.line.deaths = 3;
    entry.line.assists = 7;
    entry.line.win = true;
    entry.line.minions = 150;
    entry.line.duration_seconds = 1800;
    return entry;
}

std::vector<MatchEntry> MixedEntries() {
    return {Entry("Ahri"), Entry("LeeSin"), Entry("Ahri"), Entry("Thresh"), Entry("Ahri")};
}

}  // namespace

TEST_CASE("таблица считает записи по каждому чемпиону") {
    const std::map<std::string, int> games = CountGamesByChampion(MixedEntries());

    REQUIRE(games.size() == 3);
    CHECK(games.at("Ahri") == 3);
    CHECK(games.at("LeeSin") == 1);
    CHECK(games.at("Thresh") == 1);
}

TEST_CASE("пустой список записей — пустая таблица") {
    // Граничный случай: источник доступен, но данных нет.
    const std::map<std::string, int> games = CountGamesByChampion({});

    CHECK(games.empty());
    CHECK(MostPlayedChampion(games).empty());
}

TEST_CASE("записи с пустым именем чемпиона не попадают в таблицу") {
    // Некорректный вход: имя потерялось при разборе. Такой ключ не найти
    // через Find, и в таблице ему нечего показать.
    const std::vector<MatchEntry> entries = {Entry(""), Entry("Ahri"), Entry("")};
    const std::map<std::string, int> games = CountGamesByChampion(entries);

    REQUIRE(games.size() == 1);
    CHECK(games.count("") == 0);
    CHECK(games.at("Ahri") == 1);
}

TEST_CASE("регистр имени не нормализуется") {
    // Нормализация — работа Champion из темы 1, а не этого слоя.
    const std::vector<MatchEntry> entries = {Entry("ahri"), Entry("Ahri")};
    const std::map<std::string, int> games = CountGamesByChampion(entries);

    CHECK(games.size() == 2);
}

TEST_CASE("GamesOf возвращает 0 для неизвестного чемпиона") {
    const std::map<std::string, int> games = CountGamesByChampion(MixedEntries());

    CHECK(GamesOf(games, "Ahri") == 3);
    CHECK(GamesOf(games, "Zed") == 0);
    CHECK(GamesOf(games, "") == 0);
}

TEST_CASE("GamesOf не меняет таблицу") {
    // Главная ловушка темы: operator[] на чтение вставил бы запрошенный ключ.
    // Таблица const, поэтому такая реализация просто не соберётся — но если
    // ты обошёл это копией, размер выдаст.
    std::map<std::string, int> games = CountGamesByChampion(MixedEntries());
    const std::size_t before = games.size();

    GamesOf(games, "Zed");
    GamesOf(games, "Yasuo");

    CHECK(games.size() == before);
    CHECK(games.count("Zed") == 0);
}

TEST_CASE("MostPlayedChampion находит максимум") {
    const std::map<std::string, int> games = CountGamesByChampion(MixedEntries());

    CHECK(MostPlayedChampion(games) == "Ahri");
}

TEST_CASE("при ничьей побеждает лексикографически меньший") {
    // Граничный случай: два чемпиона с одинаковым числом игр.
    // std::map обходится по возрастанию ключа — на это и опирается контракт.
    const std::vector<MatchEntry> entries = {Entry("Zed"), Entry("Ahri")};
    const std::map<std::string, int> games = CountGamesByChampion(entries);

    REQUIRE(games.size() == 2);
    CHECK(MostPlayedChampion(games) == "Ahri");
}

TEST_CASE("таблица отсортирована по имени") {
    // Порядок обхода std::map — часть контракта, а не деталь реализации:
    // на нём держится и MostPlayedChampion, и вывод в отчёте.
    const std::map<std::string, int> games = CountGamesByChampion(MixedEntries());

    std::vector<std::string> names;
    for (const auto& pair : games) {
        names.push_back(pair.first);
    }

    REQUIRE(names.size() == 3);
    CHECK(names[0] == "Ahri");
    CHECK(names[1] == "LeeSin");
    CHECK(names[2] == "Thresh");
}
