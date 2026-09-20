// Точка входа. Файл твой: пиши, ломай, переписывай.
//
//   cmake --build build --config Debug --target analyzer
//   build\src\Debug\analyzer.exe
//
// Прошлая версия с таблицей: git show 912b154:src/app/main.cpp

#include <print>
#include <iostream>
#include <chrono>

#include "file_match_source.h"

int main() {
    auto match = course::FileMatchSource(ANALYZER_DEFAULT_DATA_DIR, "csZCcpsbfg3pwMdH7DlTxjw14mrcXuufMghn9QDGs1f0ua6X-HT120NA7POURMeO2sie4ujBzR07MQ");
    if (!match.IsAvailable()) {
        std::println("Return nullopt");
        return 0;
    }
    const auto start = std::chrono::steady_clock::now();
    auto matches = match.LoadMatches();
    const auto end = std::chrono::steady_clock::now();
    const auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    size_t count = 0;
    for (const auto& match1 : matches) {
        std::println(
        "Match duration: {} Name {} kills {} death {} assist {} win {} minions {}",
        match1.line.duration_seconds, match1.champion_name, match1.line.kills,
        match1.line.deaths, match1.line.assists, match1.line.win,
        match1.line.minions);
        ++count;
    }
    std::println("Count {}", count);
    std::println("LoadMatches took {} ms", elapsed_ms);
    return 0;
}