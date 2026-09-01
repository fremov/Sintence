#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "player_stats.h"

using course::PlayerStats;

TEST_CASE("новая статистика пустая") {
    // Граничный случай: ноль матчей. Всё нулевое, ничего не сломано,
    // делений на ноль здесь нет — они начнутся в задаче 1.3.
    const PlayerStats stats("Ahri");
    CHECK(stats.ChampionName() == "Ahri");
    CHECK(stats.Games() == 0);
    CHECK(stats.Wins() == 0);
    CHECK(stats.Losses() == 0);
    CHECK(stats.TotalKills() == 0);
    CHECK(stats.TotalDeaths() == 0);
    CHECK(stats.TotalAssists() == 0);
}

TEST_CASE("один матч учитывается целиком") {
    PlayerStats stats("Ahri");
    stats.AddMatch(10, 3, 7, true);

    CHECK(stats.Games() == 1);
    CHECK(stats.Wins() == 1);
    CHECK(stats.Losses() == 0);
    CHECK(stats.TotalKills() == 10);
    CHECK(stats.TotalDeaths() == 3);
    CHECK(stats.TotalAssists() == 7);
}

TEST_CASE("несколько матчей суммируются") {
    PlayerStats stats("LeeSin");
    stats.AddMatch(5, 5, 10, true);
    stats.AddMatch(2, 8, 4, false);
    stats.AddMatch(0, 0, 0, false);

    CHECK(stats.Games() == 3);
    CHECK(stats.Wins() == 1);
    CHECK(stats.Losses() == 2);
    CHECK(stats.TotalKills() == 7);
    CHECK(stats.TotalDeaths() == 13);
    CHECK(stats.TotalAssists() == 14);
}

TEST_CASE("матч 0/0/0 — валидный") {
    // Ремейк на четвёртой минуте: игра была, статистики нет.
    // Ноль — это данные, а не отсутствие данных.
    PlayerStats stats("Thresh");
    stats.AddMatch(0, 0, 0, false);
    CHECK(stats.Games() == 1);
    CHECK(stats.Losses() == 1);
}

TEST_CASE("испорченный матч отбрасывается целиком") {
    // Некорректный вход: отрицательное значение. Проверяем не только Games(),
    // но и все счётчики — частично применённый матч тихо портит статистику
    // и находится потом неделю.
    PlayerStats stats("Jinx");
    stats.AddMatch(8, 2, 5, true);

    stats.AddMatch(-1, 2, 3, true);
    stats.AddMatch(4, -7, 3, false);
    stats.AddMatch(4, 2, -3, true);

    CHECK(stats.Games() == 1);
    CHECK(stats.Wins() == 1);
    CHECK(stats.TotalKills() == 8);
    CHECK(stats.TotalDeaths() == 2);
    CHECK(stats.TotalAssists() == 5);
}

TEST_CASE("Wins и Losses всегда сходятся с Games") {
    PlayerStats stats("Yasuo");
    for (int i = 0; i < 7; ++i) {
        stats.AddMatch(3, 4, 6, i % 2 == 0);
    }
    CHECK(stats.Wins() + stats.Losses() == stats.Games());
    CHECK(stats.Wins() == 4);
}

TEST_CASE("все геттеры доступны у const-объекта") {
    // Если этот тест не компилируется — где-то потерян const.
    PlayerStats mutable_stats("Ahri");
    mutable_stats.AddMatch(1, 2, 3, true);

    const PlayerStats& stats = mutable_stats;
    CHECK(stats.ChampionName() == "Ahri");
    CHECK(stats.Games() == 1);
    CHECK(stats.TotalAssists() == 3);
}
