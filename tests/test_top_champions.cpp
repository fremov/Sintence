#include "doctest.h"

#include "top_champions.h"

#include "games_by_champion.h"  // callback

#include <map>
#include <string>
#include <vector>

using sintence::ChampionGames;
using sintence::TopChampionsByGames;

namespace {

std::map<std::string, int> Games(std::initializer_list<std::pair<const std::string, int>> items) {
    return std::map<std::string, int>(items);
}

}  // namespace

TEST_CASE("сортировка по убыванию числа игр") {
    const auto top = TopChampionsByGames(Games({{"Ahri", 3}, {"LeeSin", 9}, {"Zed", 5}}), 10);

    REQUIRE(top.size() == 3);
    CHECK(top[0].champion_name == "LeeSin");
    CHECK(top[0].games == 9);
    CHECK(top[1].champion_name == "Zed");
    CHECK(top[2].champion_name == "Ahri");
}

TEST_CASE("при равном числе игр — по возрастанию имени") {
    // Контракт, а не случайность: тай-брейк задаётся компаратором явно.
    const auto top = TopChampionsByGames(Games({{"Zed", 7}, {"Ahri", 7}, {"LeeSin", 7}}), 10);

    REQUIRE(top.size() == 3);
    CHECK(top[0].champion_name == "Ahri");
    CHECK(top[1].champion_name == "LeeSin");
    CHECK(top[2].champion_name == "Zed");
}

TEST_CASE("ничья и разные числа вперемешку") {
    const auto top = TopChampionsByGames(
        Games({{"Yasuo", 2}, {"Zed", 9}, {"Ahri", 9}, {"Thresh", 4}}), 10);

    REQUIRE(top.size() == 4);
    CHECK(top[0].champion_name == "Ahri");   // 9, раньше Zed по алфавиту
    CHECK(top[1].champion_name == "Zed");    // 9
    CHECK(top[2].champion_name == "Thresh"); // 4
    CHECK(top[3].champion_name == "Yasuo");  // 2
}

TEST_CASE("limit обрезает результат") {
    const auto top = TopChampionsByGames(
        Games({{"Ahri", 3}, {"LeeSin", 9}, {"Zed", 5}, {"Thresh", 1}}), 2);

    REQUIRE(top.size() == 2);
    CHECK(top[0].champion_name == "LeeSin");
    CHECK(top[1].champion_name == "Zed");
}

TEST_CASE("граница: limit == 0") {
    const auto top = TopChampionsByGames(Games({{"Ahri", 3}, {"Zed", 5}}), 0);

    CHECK(top.empty());
}

TEST_CASE("граница: limit больше размера таблицы") {
    // Классическое место для выхода за границы: resize на большее число
    // молча добавил бы пустые строки.
    const auto top = TopChampionsByGames(Games({{"Ahri", 3}, {"Zed", 5}}), 100);

    REQUIRE(top.size() == 2);
    CHECK(top[0].champion_name == "Zed");
    CHECK(top[1].champion_name == "Ahri");
}

TEST_CASE("пустая таблица") {
    const auto top = TopChampionsByGames({}, 5);

    CHECK(top.empty());
}

TEST_CASE("один чемпион") {
    const auto top = TopChampionsByGames(Games({{"Ahri", 1}}), 5);

    REQUIRE(top.size() == 1);
    CHECK(top[0].champion_name == "Ahri");
    CHECK(top[0].games == 1);
}

TEST_CASE("callback к задаче 4.1: вход берётся прямо из CountGamesByChampion") {
    // Две функции из разных тем работают в связке — так они и будут
    // стоять в анализаторе: подсчёт, потом сортировка.
    const std::vector<sintence::MatchEntry> entries = {
        {"Ahri", {}},
        {"Zed", {}},
        {"Ahri", {}},
        {"", {}},        // битая запись: CountGamesByChampion её отбросит
        {"Zed", {}},
        {"Ahri", {}},
    };

    const auto top = TopChampionsByGames(sintence::CountGamesByChampion(entries), 5);

    REQUIRE(top.size() == 2);
    CHECK(top[0].champion_name == "Ahri");
    CHECK(top[0].games == 3);
    CHECK(top[1].champion_name == "Zed");
    CHECK(top[1].games == 2);
}
