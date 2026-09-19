#include "doctest.h"

#include "champion.h"

using course::Champion;

TEST_CASE("имя сохраняется как есть") {
    const Champion champion("Ahri", "MIDDLE");
    CHECK(champion.Name() == "Ahri");
}

TEST_CASE("роль приводится к верхнему регистру") {
    CHECK(Champion("Ahri", "middle").Role() == "MIDDLE");
    CHECK(Champion("Ahri", "Middle").Role() == "MIDDLE");
    CHECK(Champion("Ahri", "MIDDLE").Role() == "MIDDLE");
}

TEST_CASE("пустые данные не ломают объект") {
    // Граничный случай: teamPosition приходит пустым, если игрок ушёл
    // с линии на первых минутах. Это валидный матч, а не повод падать.
    const Champion no_role("Ahri", "");
    CHECK(no_role.Name() == "Ahri");
    CHECK(no_role.Role() == "UNKNOWN");

    const Champion nothing("", "");
    CHECK(nothing.Name() == "Unknown");
    CHECK(nothing.Role() == "UNKNOWN");
}

TEST_CASE("DisplayName собирает строку для отчёта") {
    CHECK(Champion("LeeSin", "jungle").DisplayName() == "LeeSin (JUNGLE)");
    CHECK(Champion("", "").DisplayName() == "Unknown (UNKNOWN)");
}

TEST_CASE("HasRole сравнивает без учёта регистра") {
    const Champion champion("Thresh", "UTILITY");
    CHECK(champion.HasRole("UTILITY"));
    CHECK(champion.HasRole("utility"));
    CHECK(champion.HasRole("Utility"));
    CHECK_FALSE(champion.HasRole("MIDDLE"));
}

TEST_CASE("HasRole на пустой строке — это не совпадение") {
    // Некорректный вход: фильтр «по пустой роли» не должен молча
    // притворяться, что нашёл совпадение.
    const Champion champion("Ahri", "MIDDLE");
    REQUIRE(champion.HasRole("MIDDLE"));  // предпосылка: сравнение вообще работает
    CHECK_FALSE(champion.HasRole(""));
    CHECK_FALSE(Champion("Ahri", "").HasRole(""));
}

TEST_CASE("const-объект отдаёт все свои данные") {
    // Если этот тест не компилируется — значит какой-то метод забыл const.
    // В анализаторе Champion почти всюду ходит как const Champion&.
    const Champion champion("Jinx", "bottom");
    const Champion& ref = champion;
    CHECK(ref.Name() == "Jinx");
    CHECK(ref.Role() == "BOTTOM");
    CHECK(ref.DisplayName() == "Jinx (BOTTOM)");
    CHECK(ref.HasRole("bottom"));
}
