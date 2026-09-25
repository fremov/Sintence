#include "doctest.h"

#include "match_json.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using sintence::MatchEntry;
using sintence::ParseMatchEntry;

namespace {

// PUUID «моего» участника во всех трёх фикстурах (project/data/README.md).
constexpr std::string_view kFixturePuuid =
    "753b38aaf5955163d62c2fc323d8fca3572932060f70d47a05d1b8949156f88101e215348fb08e";

std::string ReadFixture(const std::string& name) {
    const std::filesystem::path path = std::filesystem::path(SINTENCE_FIXTURES_DIR) / name;
    std::ifstream file(path);
    REQUIRE_MESSAGE(file.good(), "не найдена фикстура: " << path.string());

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

}  // namespace

TEST_CASE("нормальный матч: победа на миде") {
    const auto entry = ParseMatchEntry(ReadFixture("fixture_win_mid.json"), kFixturePuuid);

    REQUIRE(entry.has_value());
    CHECK(entry->champion_name == "Ahri");
    CHECK(entry->line.kills == 14);
    CHECK(entry->line.deaths == 2);
    CHECK(entry->line.assists == 8);
    CHECK(entry->line.win);
    CHECK(entry->line.duration_seconds == 2214);
}

TEST_CASE("CS считается вместе с лесными монстрами") {
    // Ahri: totalMinionsKilled 296 + neutralMinionsKilled 6.
    // Забыть второе слагаемое — типовая ошибка, и на леснике она даёт
    // CS/min втрое меньше настоящего.
    const auto entry = ParseMatchEntry(ReadFixture("fixture_win_mid.json"), kFixturePuuid);

    REQUIRE(entry.has_value());
    CHECK(entry->line.minions == 302);
}

TEST_CASE("CS леснка: основная часть фарма — нейтралы") {
    // Nidalee: totalMinionsKilled 11 + neutralMinionsKilled 74.
    const auto entry = ParseMatchEntry(ReadFixture("fixture_loss_jungle.json"), kFixturePuuid);

    REQUIRE(entry.has_value());
    CHECK(entry->champion_name == "Nidalee");
    CHECK(entry->line.minions == 85);
    CHECK_FALSE(entry->line.win);
}

TEST_CASE("пустой teamPosition не мешает разбору") {
    // В fixture_loss_jungle у нужного участника teamPosition пустой:
    // так бывает, когда игрок ушёл с линии на ранней стадии.
    // Роль этот адаптер вообще не читает — проверяем, что он не падает.
    const auto entry = ParseMatchEntry(ReadFixture("fixture_loss_jungle.json"), kFixturePuuid);

    REQUIRE(entry.has_value());
    CHECK(entry->line.kills == 2);
    CHECK(entry->line.deaths == 6);
}

TEST_CASE("битая фикстура не роняет разбор") {
    // fixture_malformed.json: у нужного участника deaths == null,
    // а gameDuration отсутствует. get<int>() на таком поле бросает
    // исключение — этот тест проверяет, что до него дело не доходит.
    const auto entry = ParseMatchEntry(ReadFixture("fixture_malformed.json"), kFixturePuuid);

    CHECK_FALSE(entry.has_value());
}

TEST_CASE("чужой матч: игрока с таким puuid нет") {
    // Не ошибка: в каталоге могут лежать матчи, где меня не было.
    const auto entry = ParseMatchEntry(ReadFixture("fixture_win_mid.json"), "нет-такого-puuid");

    CHECK_FALSE(entry.has_value());
}

TEST_CASE("мусор вместо JSON") {
    CHECK_FALSE(ParseMatchEntry("не json вовсе", kFixturePuuid).has_value());
    CHECK_FALSE(ParseMatchEntry("", kFixturePuuid).has_value());
    CHECK_FALSE(ParseMatchEntry("{\"info\": ", kFixturePuuid).has_value());
}

TEST_CASE("валидный JSON без нужной структуры") {
    CHECK_FALSE(ParseMatchEntry("{}", kFixturePuuid).has_value());
    CHECK_FALSE(ParseMatchEntry("[1, 2, 3]", kFixturePuuid).has_value());
    CHECK_FALSE(ParseMatchEntry("{\"info\": {}}", kFixturePuuid).has_value());
    CHECK_FALSE(ParseMatchEntry("{\"info\": {\"participants\": 5}}", kFixturePuuid).has_value());
}

TEST_CASE("настоящий матч из выгрузки") {
    // Не фикстура, а файл из project/data/matches — тот самый формат,
    // который приедет из Riot API.
    const std::filesystem::path path =
        std::filesystem::path(SINTENCE_MATCHES_DIR) / "RU_528252891.json";
    std::ifstream file(path);
    REQUIRE_MESSAGE(file.good(), "нет файла выгрузки: " << path.string());

    std::ostringstream buffer;
    buffer << file.rdbuf();

    const auto entry = ParseMatchEntry(
        buffer.str(),
        "csZCcpsbfg3pwMdH7DlTxjw14mrcXuufMghn9QDGs1f0ua6X-HT120NA7POURMeO2sie4ujBzR07MQ");

    REQUIRE(entry.has_value());
    CHECK(entry->champion_name == "Darius");
    CHECK(entry->line.kills == 3);
    CHECK(entry->line.deaths == 11);
    CHECK(entry->line.assists == 9);
    CHECK_FALSE(entry->line.win);
    CHECK(entry->line.minions == 173);
    CHECK(entry->line.duration_seconds == 2315);
}
