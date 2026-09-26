#include <chrono>

#include "doctest.h"
#include "rate_limiter.h"

using sintence::RateLimiter;
using std::chrono::milliseconds;
using std::chrono::seconds;

namespace {

// Точка отсчёта для всех тестов. Часы нигде не спрашиваются по-настоящему:
// «сейчас» — это просто число, которое двигает тест. Тест на окно
// в две минуты, который честно ждёт две минуты, никто не запустит.
const RateLimiter::TimePoint kStart{};

RateLimiter::TimePoint At(milliseconds offset) {
    return kStart + offset;
}

// Маленькое окно, чтобы границы были видны глазами: 3 запроса в секунду.
RateLimiter Small() {
    return RateLimiter({{3, milliseconds{1000}}});
}

}  // namespace

TEST_CASE("Пустой ограничитель пропускает сразу") {
    const RateLimiter limiter = Small();

    CHECK(limiter.DelayUntilAllowed(kStart) == milliseconds{0});
    CHECK(limiter.Used(0, kStart) == 0);
}

TEST_CASE("Пока окно не заполнено, ждать не нужно") {
    RateLimiter limiter = Small();

    limiter.Record(At(milliseconds{0}));
    limiter.Record(At(milliseconds{100}));

    CHECK(limiter.Used(0, At(milliseconds{200})) == 2);
    CHECK(limiter.DelayUntilAllowed(At(milliseconds{200})) == milliseconds{0});
}

TEST_CASE("Заполненное окно заставляет ждать выхода самой старой записи") {
    RateLimiter limiter = Small();

    limiter.Record(At(milliseconds{0}));
    limiter.Record(At(milliseconds{100}));
    limiter.Record(At(milliseconds{200}));

    // Три записи при лимите три: следующий запрос возможен не раньше,
    // чем первая запись покинет секундное окно, то есть в 1000 мс.
    CHECK(limiter.Used(0, At(milliseconds{300})) == 3);
    CHECK(limiter.DelayUntilAllowed(At(milliseconds{300})) == milliseconds{700});
}

TEST_CASE("Запись ровно на границе окна уже не считается") {
    RateLimiter limiter = Small();

    limiter.Record(At(milliseconds{0}));
    limiter.Record(At(milliseconds{100}));
    limiter.Record(At(milliseconds{200}));

    // Прошла ровно секунда с первой записи — она вышла из окна.
    CHECK(limiter.Used(0, At(milliseconds{1000})) == 2);
    CHECK(limiter.DelayUntilAllowed(At(milliseconds{1000})) == milliseconds{0});
}

TEST_CASE("Окно скользит, а не сбрасывается целиком") {
    RateLimiter limiter = Small();

    limiter.Record(At(milliseconds{0}));
    limiter.Record(At(milliseconds{100}));
    limiter.Record(At(milliseconds{200}));

    // В 1050 мс первая запись ушла, две остались — место ровно на один
    // запрос. Сделали его — снова ждём, теперь до выхода записи из 100 мс.
    limiter.Record(At(milliseconds{1050}));

    CHECK(limiter.Used(0, At(milliseconds{1050})) == 3);
    CHECK(limiter.DelayUntilAllowed(At(milliseconds{1050})) == milliseconds{50});
}

TEST_CASE("Длинное окно душит даже когда короткое свободно") {
    // 5 запросов в секунду и 6 за десять секунд.
    RateLimiter limiter({{5, milliseconds{1000}}, {6, milliseconds{10000}}});

    for (int i = 0; i < 5; ++i) {
        limiter.Record(At(milliseconds{i * 10}));
    }
    limiter.Record(At(milliseconds{1500}));

    // На 2000 мс секундное окно почти пустое (в нём одна запись),
    // но длинное заполнено: 6 из 6. Ждём выхода самой старой из него.
    CHECK(limiter.Used(0, At(milliseconds{2000})) == 1);
    CHECK(limiter.Used(1, At(milliseconds{2000})) == 6);
    CHECK(limiter.DelayUntilAllowed(At(milliseconds{2000})) == milliseconds{8000});
}

TEST_CASE("BlockUntil после 429 держит паузу при пустых окнах") {
    RateLimiter limiter = Small();

    limiter.BlockUntil(At(seconds{30}));

    CHECK(limiter.Used(0, At(milliseconds{0})) == 0);
    CHECK(limiter.DelayUntilAllowed(At(milliseconds{0})) == milliseconds{30000});
    CHECK(limiter.DelayUntilAllowed(At(seconds{30})) == milliseconds{0});
}

TEST_CASE("Второй 429 не сокращает уже назначенную паузу") {
    RateLimiter limiter = Small();

    limiter.BlockUntil(At(seconds{30}));
    limiter.BlockUntil(At(seconds{5}));

    CHECK(limiter.DelayUntilAllowed(At(milliseconds{0})) == milliseconds{30000});
}

TEST_CASE("RiotPersonalKey: 20 в секунду") {
    RateLimiter limiter = RateLimiter::RiotPersonalKey();

    for (int i = 0; i < 20; ++i) {
        limiter.Record(At(milliseconds{i}));
    }

    CHECK(limiter.Used(0, At(milliseconds{20})) == 20);
    CHECK(limiter.DelayUntilAllowed(At(milliseconds{20})) == milliseconds{980});
}

TEST_CASE("RiotPersonalKey: 100 за две минуты") {
    RateLimiter limiter = RateLimiter::RiotPersonalKey();

    // Сто запросов по пять в секунду — секундное окно не нарушено ни разу.
    for (int i = 0; i < 100; ++i) {
        limiter.Record(At(milliseconds{(i / 5) * 1000 + (i % 5) * 10}));
    }

    // Уложились в двадцать секунд, но двухминутный лимит выбран целиком.
    const auto now = At(seconds{25});
    CHECK(limiter.Used(0, now) == 0);
    CHECK(limiter.Used(1, now) == 100);
    // Первая запись была в нуле, окно 120 с — ждать до 120-й секунды.
    CHECK(limiter.DelayUntilAllowed(now) == milliseconds{95000});
}

TEST_CASE("Used на несуществующем окне возвращает ноль, а не падает") {
    const RateLimiter limiter = Small();

    CHECK(limiter.Used(7, kStart) == 0);
}
