#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "match_payload.h"

#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using course::MatchPayload;

namespace {

// Кусок настоящего ответа Riot — в теме 8 через этот тип пойдёт он целиком.
const std::string kJson = R"({"championName":"Ahri","kills":10,"deaths":3,"assists":7})";

}  // namespace

TEST_CASE("payload по умолчанию пуст") {
    // Граничный случай: ничего не выделено, и освобождать в деструкторе нечего.
    const MatchPayload payload;
    CHECK(payload.Empty());
    CHECK(payload.Size() == 0);
    CHECK(payload.Data() == nullptr);
    CHECK(payload.ToString().empty());
}

TEST_CASE("payload из текста хранит его целиком") {
    const MatchPayload payload(kJson);
    CHECK_FALSE(payload.Empty());
    CHECK(payload.Size() == kJson.size());
    CHECK(payload.Data() != nullptr);
    CHECK(payload.ToString() == kJson);
}

TEST_CASE("пустой текст — валидный вход") {
    // Граничный случай: выделять под ноль байт не нужно, но и падать нельзя.
    const MatchPayload payload(std::string{});
    CHECK(payload.Empty());
    CHECK(payload.Size() == 0);
    CHECK(payload.ToString().empty());
}

TEST_CASE("копия глубокая: два объекта — два буфера") {
    const MatchPayload original(kJson);
    const MatchPayload copy(original);

    CHECK(copy.ToString() == kJson);
    CHECK(original.ToString() == kJson);

    // Если указатели совпали — копирование поверхностное, и один и тот же
    // буфер освободится дважды на выходе из TEST_CASE.
    CHECK(copy.Data() != original.Data());
}

TEST_CASE("копия переживает смерть оригинала") {
    MatchPayload copy;
    {
        const MatchPayload original(kJson);
        copy = original;
    }  // original разрушен: при поверхностной копии буфер уже освобождён

    CHECK(copy.ToString() == kJson);
}

TEST_CASE("копирующее присваивание поверх непустого payload") {
    const MatchPayload source(kJson);
    MatchPayload target(std::string("старое содержимое"));

    target = source;

    CHECK(target.ToString() == kJson);
    CHECK(target.Size() == kJson.size());
    CHECK(target.Data() != source.Data());
}

TEST_CASE("самоприсваивание не разрушает объект") {
    // Классическая ловушка: «освободить своё, потом скопировать чужое»
    // на a = a освобождает буфер и тут же читает из него.
    MatchPayload payload(kJson);

    MatchPayload& alias = payload;  // через ссылку, чтобы компилятор не ругался
    payload = alias;

    CHECK(payload.ToString() == kJson);
    CHECK(payload.Size() == kJson.size());
}

TEST_CASE("перемещающий конструктор забирает буфер") {
    MatchPayload source(kJson);
    const char* const original_data = source.Data();

    const MatchPayload moved(std::move(source));

    CHECK(moved.ToString() == kJson);
    // Буфер именно передан, а не скопирован заново.
    CHECK(moved.Data() == original_data);

    // Контракт из match_payload.h: источник остаётся пустым и валидным.
    CHECK(source.Empty());
    CHECK(source.Size() == 0);
    CHECK(source.Data() == nullptr);
}

TEST_CASE("перемещающее присваивание забирает буфер") {
    MatchPayload source(kJson);
    MatchPayload target(std::string("старое содержимое"));

    target = std::move(source);

    CHECK(target.ToString() == kJson);
    CHECK(source.Empty());
    CHECK(source.Data() == nullptr);
}

TEST_CASE("перемещение помечено noexcept") {
    // Без noexcept std::vector при расширении не станет рисковать
    // и будет копировать — молча, без единого сообщения.
    CHECK(std::is_nothrow_move_constructible<MatchPayload>::value);
    CHECK(std::is_nothrow_move_assignable<MatchPayload>::value);
}

TEST_CASE("вектор payload'ов переживает расширение") {
    // Здесь всё сходится вместе: vector при росте перемещает или копирует
    // элементы, и любая ошибка во владении вылезает как двойное освобождение.
    std::vector<MatchPayload> payloads;
    for (int i = 0; i < 100; ++i) {
        payloads.push_back(MatchPayload(kJson + std::to_string(i)));
    }

    REQUIRE(payloads.size() == 100);
    CHECK(payloads.front().ToString() == kJson + "0");
    CHECK(payloads.back().ToString() == kJson + "99");
}

TEST_CASE("все геттеры доступны у const-объекта") {
    // Возврат к теме 1: если этот тест не компилируется — где-то потерян const.
    MatchPayload mutable_payload(kJson);
    const MatchPayload& payload = mutable_payload;

    CHECK_FALSE(payload.Empty());
    CHECK(payload.Size() == kJson.size());
    CHECK(payload.Data() != nullptr);
    CHECK(payload.ToString() == kJson);
}
