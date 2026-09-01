// Вторая единица трансляции. Она видит только заголовок kda.h,
// то есть только объявления. Тела ComputeKda здесь нет и быть не должно —
// его подставит линкер, когда будет собирать demo_build_model.exe
// из kda.obj и demo_tests.obj.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "kda.h"

TEST_CASE("ComputeKda считает (kills + assists) / deaths") {
    const course::Kda kda{10, 5, 5};
    CHECK(course::ComputeKda(kda) == doctest::Approx(3.0));
}

TEST_CASE("ноль смертей не делит на ноль") {
    const course::Kda kda{7, 0, 3};
    CHECK(course::ComputeKda(kda) == doctest::Approx(10.0));
    CHECK(course::IsPerfectGame(kda));
}
