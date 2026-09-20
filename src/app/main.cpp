// Точка входа. Файл твой: пиши, ломай, переписывай.
//
//   cmake --build build --config Debug --target analyzer
//   build\src\Debug\analyzer.exe
//
// Прошлая версия с таблицей: git show 912b154:src/app/main.cpp

#include <print>
#include <fstream>
#include <iostream>
#include <sstream>
#include "../data/match_json.h"

int main() {
    // 1. Путь к одному файлу: ANALYZER_DEFAULT_DATA_DIR + "/RU_528252891.json"
    //    (макрос подставляет CMake, это project/data/matches)
    //
    // 2. Прочитать файл целиком в std::string — идиома из главы 6, раздел 3:
    //    ifstream, потом ostringstream << file.rdbuf(), потом .str()
    //
    // 3. const auto entry = course::ParseMatchEntry(текст, puuid);
    //    puuid — поле "puuid" в project/data/matches/index.json, скопируй строкой
    //
    // 4. if (!entry) — напечатать, что вернулся nullopt, и на этом всё
    //
    // 5. Иначе печатать поля и смотреть, совпадают ли они с файлом:
    //    entry->champion_name, entry->line.kills, .deaths, .assists,
    //    .win, .minions, .duration_seconds
    //    В RU_528252891.json должно получиться: Darius 3/11/9, поражение,
    //    173 миньона, 2315 секунд.
    std::ifstream file(
        std::string(ANALYZER_DEFAULT_DATA_DIR) + "/RU_528252891.json");
    if (!file) {
        return 0;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();
    auto match = course::ParseMatchEntry(
        text,
        "csZCcpsbfg3pwMdH7DlTxjw14mrcXuufMghn9QDGs1f0ua6X-HT120NA7POURMeO2sie4ujBzR07MQ");
    if (!match) {
        std::println("Return nullopt");
        return 0;
    }
    std::println(
        "Match duration: {} Name {} kills {} death {} assist {} win {} minions {}",
        match->line.duration_seconds, match->champion_name, match->line.kills,
        match->line.deaths, match->line.assists, match->line.win,
        match->line.minions);
    return 0;
}