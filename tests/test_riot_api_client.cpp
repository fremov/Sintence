#include "doctest.h"
#include "riot_api_client.h"

using sintence::UrlEncodePathSegment;

TEST_CASE("UrlEncodePathSegment оставляет незарезервированные символы") {
    // RFC 3986: A-Z a-z 0-9 - . _ ~ кодировать не нужно.
    CHECK(UrlEncodePathSegment("Faker") == "Faker");
    CHECK(UrlEncodePathSegment("a-b_c.d~e") == "a-b_c.d~e");
    CHECK(UrlEncodePathSegment("TXC1") == "TXC1");
    CHECK(UrlEncodePathSegment("") == "");
}

TEST_CASE("UrlEncodePathSegment кодирует пробел в имени") {
    // Riot ID с пробелом — самый частый случай: "Riot Tuxedo#TXC1".
    // Незакодированный пробел ломает строку запроса целиком.
    CHECK(UrlEncodePathSegment("Riot Tuxedo") == "Riot%20Tuxedo");
}

TEST_CASE("UrlEncodePathSegment кодирует служебные символы пути") {
    CHECK(UrlEncodePathSegment("a/b") == "a%2Fb");
    CHECK(UrlEncodePathSegment("a?b") == "a%3Fb");
    CHECK(UrlEncodePathSegment("a#b") == "a%23b");
    CHECK(UrlEncodePathSegment("a%b") == "a%25b");
}

TEST_CASE("UrlEncodePathSegment: кириллица кодируется по байтам UTF-8") {
    // Главная ловушка: char в MSVC знаковый, байт 0xD0 отрицателен.
    // Без static_cast<unsigned char> получится "%FFFFFFD0" вместо "%D0",
    // и Riot ответит 400 на запрос, который выглядит правильным.
    // Байты записаны явно, чтобы тест не зависел от кодировки исходника.
    CHECK(UrlEncodePathSegment("\xD0\x81\xD0\xB6") == "%D0%81%D0%B6");
}

TEST_CASE("UrlEncodePathSegment: шестнадцатеричные цифры в верхнем регистре") {
    // RFC допускает оба регистра, но Riot сверяет подпись имени побайтово,
    // и разнобой в регистре — источник неповторяемых ошибок.
    CHECK(UrlEncodePathSegment(" ") == "%20");
    CHECK(UrlEncodePathSegment("\xEF\xBB\xBF") == "%EF%BB%BF");
}
