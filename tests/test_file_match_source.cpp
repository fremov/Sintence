#include "doctest.h"

#include "file_match_source.h"

#include <filesystem>
#include <fstream>
#include <string>

#include "index_by_map.h"

using course::FileMatchSource;
using course::MatchSource;

namespace {

constexpr std::string_view kRealPuuid =
    "csZCcpsbfg3pwMdH7DlTxjw14mrcXuufMghn9QDGs1f0ua6X-HT120NA7POURMeO2sie4ujBzR07MQ";

std::filesystem::path MatchesDir() {
    return std::filesystem::path(COURSE_MATCHES_DIR);
}

}  // namespace

TEST_CASE("каталог выгрузки доступен") {
    const FileMatchSource source(MatchesDir(), std::string(kRealPuuid));

    CHECK(source.IsAvailable());
    CHECK(source.Name() == "files:matches");
}

TEST_CASE("несуществующий каталог — не исключение") {
    // Главный граничный случай: путь может не существовать, и это
    // не повод падать. Ни одна из трёх функций не должна бросить.
    const FileMatchSource source(MatchesDir() / "нет-такого-каталога",
                                 std::string(kRealPuuid));

    CHECK_FALSE(source.IsAvailable());
    CHECK(source.LoadMatches().empty());
    CHECK(source.JsonFileCount() == 0);
}

TEST_CASE("путь ведёт на файл, а не на каталог") {
    const FileMatchSource source(MatchesDir() / "index.json", std::string(kRealPuuid));

    CHECK_FALSE(source.IsAvailable());
    CHECK(source.LoadMatches().empty());
}

TEST_CASE("index.json не считается матчем") {
    // Он лежит в том же каталоге, но это манифест выгрузки.
    // Попав в разбор, он дал бы ноль записей и путаницу в счётчиках.
    const FileMatchSource source(MatchesDir(), std::string(kRealPuuid));

    std::size_t json_files = 0;
    for (const auto& entry : std::filesystem::directory_iterator(MatchesDir())) {
        if (entry.path().extension() == ".json") {
            ++json_files;
        }
    }

    REQUIRE(json_files > 1);
    CHECK(source.JsonFileCount() == json_files - 1);  // без index.json
}

TEST_CASE("загрузка настоящей выгрузки") {
    const FileMatchSource source(MatchesDir(), std::string(kRealPuuid));

    const auto entries = source.LoadMatches();

    // В выгрузке 193 матча одного игрока: каждый файл должен дать запись.
    CHECK(entries.size() == source.JsonFileCount());
    REQUIRE(!entries.empty());

    for (const auto& entry : entries) {
        CHECK_FALSE(entry.champion_name.empty());
        CHECK(entry.line.duration_seconds > 0);
    }
}

TEST_CASE("чужой puuid — ноль записей, но не ошибка") {
    const FileMatchSource source(MatchesDir(), "меня-в-этих-матчах-не-было");

    CHECK(source.IsAvailable());          // каталог-то на месте
    CHECK(source.LoadMatches().empty());  // а записей нет
}

TEST_CASE("источник работает через интерфейс MatchSource") {
    // Проверка того, ради чего в теме 3 заводился интерфейс: анализ
    // не знает, что перед ним файлы.
    const FileMatchSource source(MatchesDir(), std::string(kRealPuuid));
    const MatchSource& as_interface = source;

    const auto index = course::BuildChampionIndexFast(as_interface);

    CHECK_FALSE(index.Empty());
    CHECK(index.Size() > 0);
}
