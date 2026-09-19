#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "champion_pool.h"

#include <set>
#include <string>

using course::ChampionPool;

TEST_CASE("пул хранит уникальных чемпионов") {
    ChampionPool pool;
    pool.Add("Ahri", "MIDDLE");
    pool.Add("LeeSin", "JUNGLE");
    pool.Add("Ahri", "MIDDLE");  // полный повтор — не дубликат

    CHECK(pool.Size() == 2);
    CHECK(pool.Contains("Ahri"));
    CHECK(pool.Contains("LeeSin"));
    CHECK_FALSE(pool.Contains("Zed"));
}

TEST_CASE("чемпионы отсортированы по имени") {
    // Порядок даёт std::set, руками сортировать нечего.
    ChampionPool pool;
    pool.Add("Zed", "MIDDLE");
    pool.Add("Ahri", "MIDDLE");
    pool.Add("LeeSin", "JUNGLE");

    const std::set<std::string>& champions = pool.Champions();
    REQUIRE(champions.size() == 3);

    auto it = champions.begin();
    CHECK(*it++ == "Ahri");
    CHECK(*it++ == "LeeSin");
    CHECK(*it == "Zed");
}

TEST_CASE("у одного чемпиона может быть несколько ролей") {
    ChampionPool pool;
    pool.Add("Ahri", "MIDDLE");
    pool.Add("Ahri", "BOTTOM");
    pool.Add("Ahri", "MIDDLE");  // повтор роли

    const std::set<std::string> roles = pool.RolesOf("Ahri");
    REQUIRE(roles.size() == 2);
    CHECK(roles.count("MIDDLE") == 1);
    CHECK(roles.count("BOTTOM") == 1);
    CHECK(pool.Size() == 1);  // чемпион по-прежнему один
}

TEST_CASE("роль нормализуется через Champion из темы 1") {
    // Callback: приводить регистр своими руками здесь нельзя —
    // это уже умеет Champion, и второй экземпляр той же логики разойдётся.
    ChampionPool pool;
    pool.Add("Ahri", "middle");
    pool.Add("Ahri", "Middle");
    pool.Add("Ahri", "MIDDLE");

    const std::set<std::string> roles = pool.RolesOf("Ahri");
    REQUIRE(roles.size() == 1);
    CHECK(roles.count("MIDDLE") == 1);
}

TEST_CASE("пустая роль превращается в UNKNOWN") {
    // Так бывает в настоящих данных: teamPosition пустой, если игрок
    // ушёл с линии на ранней стадии. Это не повод терять запись.
    ChampionPool pool;
    pool.Add("Thresh", "");

    const std::set<std::string> roles = pool.RolesOf("Thresh");
    REQUIRE(roles.size() == 1);
    CHECK(roles.count("UNKNOWN") == 1);
}

TEST_CASE("пустое имя чемпиона отбрасывается целиком") {
    // Некорректный вход: чемпиона без имени в пуле быть не может.
    ChampionPool pool;
    pool.Add("", "MIDDLE");
    pool.Add("", "");

    CHECK(pool.Size() == 0);
    CHECK(pool.Champions().empty());
    CHECK_FALSE(pool.Contains(""));
    CHECK(pool.RolesOf("").empty());
}

TEST_CASE("регистр имени чемпиона не нормализуется") {
    // Роль — да, имя — нет: "Ahri" это имя собственное, и в отчёте
    // оно должно выглядеть человечески.
    ChampionPool pool;
    pool.Add("ahri", "MIDDLE");
    pool.Add("Ahri", "MIDDLE");

    CHECK(pool.Size() == 2);
}

TEST_CASE("RolesOf на неизвестном чемпионе — пустое множество, а не исключение") {
    // Граничный случай и главная ловушка темы одновременно: наивное чтение
    // через operator[] завело бы в словарь пустую запись на каждый запрос.
    ChampionPool pool;
    pool.Add("Ahri", "MIDDLE");

    CHECK(pool.RolesOf("Zed").empty());
    CHECK(pool.RolesOf("Yasuo").empty());

    // Размер пула от чтения не изменился.
    CHECK(pool.Size() == 1);
    CHECK_FALSE(pool.Contains("Zed"));
}

TEST_CASE("пустой пул") {
    const ChampionPool pool;

    CHECK(pool.Size() == 0);
    CHECK(pool.Champions().empty());
    CHECK_FALSE(pool.Contains("Ahri"));
    CHECK(pool.RolesOf("Ahri").empty());
}
