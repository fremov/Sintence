// ============================================================================
// Playground — живая сборка всего, что уже написано.
//
// Здесь нет ни одной строки логики анализатора: файл только создаёт твои
// классы и вызывает твои методы, чтобы было видно, что они делают на
// настоящих данных. Всё, что печатается ниже, посчитано твоим кодом.
//
// Запуск:  cmake --build build --config Debug --target playground
//          build\playground\Debug\playground.exe
//
// Файл мой, а не твой: правь свободно, ломай, дописывай свои эксперименты —
// на тесты курса он не влияет и в ctest не зарегистрирован.
// ============================================================================

#include <cstdio>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "champion.h"        // тема 1, задача 1.1
#include "champion_index.h"  // тема 2, задача 2.3
#include "champion_report.h" // тема 1, задача 1.3
#include "fallback_source.h" // тема 3, задача 3.2
#include "index_builder.h"   // тема 3, задача 3.3
#include "match_file.h"      // тема 2, задача 2.1
#include "match_payload.h"   // тема 2, задача 2.2
#include "match_source.h"    // тема 3, задача 3.1
#include "player_stats.h"    // тема 1, задача 1.2

using course::BuildChampionIndex;
using course::Champion;
using course::ChampionIndex;
using course::ChampionReport;
using course::FallbackMatchSource;
using course::FixtureMatchSource;
using course::MatchEntry;
using course::MatchFile;
using course::MatchLine;
using course::MatchPayload;
using course::MatchSource;
using course::PlayerStats;

namespace {

void Header(const std::string& title) {
    std::cout << "\n" << std::string(70, '=') << "\n" << title << "\n" << std::string(70, '=') << "\n";
}

MatchEntry Entry(std::string champion, int kills, int deaths, int assists, bool win, int minions,
                 int duration_seconds) {
    MatchEntry entry;
    entry.champion_name = std::move(champion);
    entry.line.kills = kills;
    entry.line.deaths = deaths;
    entry.line.assists = assists;
    entry.line.win = win;
    entry.line.minions = minions;
    entry.line.duration_seconds = duration_seconds;
    return entry;
}

// Похоже на настоящую выгрузку: несколько чемпионов, у каждого разное
// количество игр, одна запись намеренно битая (duration_seconds == 0).
std::vector<MatchEntry> SampleEntries() {
    return {
        Entry("Ahri", 10, 2, 8, true, 200, 1800),   Entry("Ahri", 4, 5, 6, false, 160, 1500),
        Entry("Ahri", 7, 3, 12, true, 210, 1920),   Entry("LeeSin", 4, 6, 9, false, 90, 1200),
        Entry("LeeSin", 8, 4, 14, true, 120, 1680), Entry("Thresh", 1, 5, 22, true, 40, 2100),
        Entry("Yasuo", 12, 8, 3, false, 240, 0),  // битая: duration_seconds == 0
    };
}

void PrintTableHeader() {
    std::cout << std::left << std::setw(10) << "champion" << std::right << std::setw(7) << "games"
              << std::setw(9) << "KDA" << std::setw(11) << "winrate" << std::setw(9) << "CS/min"
              << std::setw(8) << "30+?" << "\n"
              << std::string(54, '-') << "\n";
}

void PrintReport(const ChampionReport& report) {
    std::cout << std::left << std::setw(10) << report.ChampionName() << std::right << std::setw(7)
              << report.Games() << std::setw(9) << std::fixed << std::setprecision(2) << report.Kda()
              << std::setw(10) << std::setprecision(1) << (report.Winrate() * 100.0) << "%"
              << std::setw(9) << std::setprecision(1) << report.CsPerMinute() << std::setw(8)
              << (report.HasEnoughData() ? "yes" : "no") << "\n";
}

void PrintIndex(const ChampionIndex& index) {
    PrintTableHeader();
    for (const std::string& name : index.ChampionNames()) {
        const ChampionReport* report = index.Find(name);
        if (report != nullptr) {
            PrintReport(*report);
        }
    }
}

// Печатает, что видно снаружи через интерфейс. Функция не знает и не может
// узнать, какая именно реализация ей досталась, — в этом весь смысл темы 3.
void Describe(const MatchSource& source) {
    std::cout << "  Name()         = " << source.Name() << "\n"
              << "  IsAvailable()  = " << (source.IsAvailable() ? "true" : "false") << "\n"
              << "  LoadMatches()  = " << source.LoadMatches().size() << " записей\n";
}

void DemoTopic01() {
    Header("Тема 1 — Champion: инвариант держит конструктор");

    const Champion mid("Ahri", "middle");      // роль в нижнем регистре
    const Champion broken("", "");             // пустые строки на входе
    std::cout << "Champion(\"Ahri\", \"middle\")  -> DisplayName() = " << mid.DisplayName() << "\n"
              << "                             HasRole(\"MIDDLE\") = " << (mid.HasRole("MIDDLE") ? "true" : "false")
              << ", HasRole(\"top\") = " << (mid.HasRole("top") ? "true" : "false") << "\n"
              << "Champion(\"\", \"\")           -> DisplayName() = " << broken.DisplayName()
              << "   (пустое не пролезло)\n";

    Header("Тема 1 — PlayerStats: испорченный матч не учитывается вообще");

    PlayerStats stats("Ahri");
    stats.AddMatch(10, 2, 8, true);
    stats.AddMatch(4, 5, 6, false);
    stats.AddMatch(-1, 3, 3, true);  // мусор из JSON: null превратился в -1

    std::cout << "три AddMatch, из них один с kills = -1\n"
              << "  Games()  = " << stats.Games() << "   (битый матч отброшен целиком)\n"
              << "  Wins()   = " << stats.Wins() << ",  Losses() = " << stats.Losses() << "\n"
              << "  Totals   = " << stats.TotalKills() << "/" << stats.TotalDeaths() << "/"
              << stats.TotalAssists() << "\n";

    Header("Тема 1 — ChampionReport: метрики считаются на лету");

    ChampionReport report("Ahri");
    report.Add(MatchLine{10, 2, 8, true, 200, 1800});
    report.Add(MatchLine{4, 5, 6, false, 160, 1500});
    report.Add(MatchLine{7, 3, 12, true, 210, 1920});
    report.Add(MatchLine{99, 99, 99, true, 999, 0});  // duration_seconds == 0 → отброшен

    std::cout << "четыре Add, последний с duration_seconds = 0\n";
    PrintTableHeader();
    PrintReport(report);
    std::cout << "\nHasEnoughData() == нет, потому что порог — "
              << ChampionReport::kMinGamesForConclusion << " игр, а тут " << report.Games() << ".\n";
}

void DemoTopic02() {
    Header("Тема 2 — MatchFile: RAII на настоящем файле");

    const std::string path = std::string(COURSE_FIXTURES_DIR) + "/fixture_win_mid.json";
    const MatchFile file(path);
    std::cout << "MatchFile(\"" << path << "\")\n"
              << "  IsOpen() = " << (file.IsOpen() ? "true" : "false")
              << ", Handle() = " << (file.Handle() != nullptr ? "не nullptr" : "nullptr") << "\n";

    const MatchFile missing(std::string(COURSE_FIXTURES_DIR) + "/no_such_file.json");
    std::cout << "MatchFile(несуществующий путь)\n"
              << "  IsOpen() = " << (missing.IsOpen() ? "true" : "false")
              << "   (объект создан, деструктор отработает корректно)\n";

    Header("Тема 2 — MatchPayload: правило пяти на сырых байтах");

    std::string head;
    if (file.IsOpen()) {
        char buffer[120] = {};
        const std::size_t read = std::fread(buffer, 1, sizeof(buffer) - 1, file.Handle());
        head.assign(buffer, read);
    }

    MatchPayload payload(head);
    std::string preview = payload.ToString().substr(0, 70);
    for (char& c : preview) {
        if (c == '\n' || c == '\r') {
            c = ' ';
        }
    }
    std::cout << "прочитали первые " << payload.Size() << " байт файла:\n"
              << "  " << preview << "...\n";

    const MatchPayload copy = payload;                  // глубокая копия
    const MatchPayload moved = std::move(payload);      // перемещение
    std::cout << "после копии и перемещения:\n"
              << "  copy.Size()    = " << copy.Size() << "   (свой буфер)\n"
              << "  moved.Size()   = " << moved.Size() << "   (забрал буфер оригинала)\n"
              << "  payload.Size() = " << payload.Size() << "   (источник пуст и валиден)\n";

    Header("Тема 2 — ChampionIndex: правило нуля");

    ChampionIndex index;
    ChampionReport ahri("Ahri");
    ahri.Add(MatchLine{10, 2, 8, true, 200, 1800});
    ChampionReport lee("LeeSin");
    lee.Add(MatchLine{4, 6, 9, false, 90, 1200});

    index.AddReport(std::move(ahri));
    index.AddReport(std::move(lee));

    const ChampionIndex copy_of_index = index;  // копируется сам, писать было нечего
    std::cout << "Size() = " << index.Size() << ", Empty() = " << (index.Empty() ? "true" : "false")
              << "\nFind(\"Ahri\")    -> " << (index.Find("Ahri") != nullptr ? "нашёлся" : "nullptr")
              << "\nFind(\"Zed\")     -> " << (index.Find("Zed") != nullptr ? "нашёлся" : "nullptr")
              << "\nкопия индекса   -> Size() = " << copy_of_index.Size()
              << "   (пять спецфункций сгенерированы компилятором)\n";
}

void DemoTopic03() {
    Header("Тема 3 — FixtureMatchSource через ссылку на интерфейс");

    const FixtureMatchSource fixtures("fixtures", SampleEntries(), true);
    const FixtureMatchSource offline("live-client", SampleEntries(), false);

    std::cout << "доступный источник:\n";
    Describe(fixtures);
    std::cout << "недоступный источник (записи внутри есть, но читать нельзя):\n";
    Describe(offline);

    Header("Тема 3 — FallbackMatchSource: выбор источника без единого if снаружи");

    const FallbackMatchSource live_first(offline, fixtures);
    std::cout << "FallbackMatchSource(offline, fixtures) — основной лежит:\n";
    Describe(live_first);
    std::cout << "  Active()       = " << (live_first.Active() != nullptr ? live_first.Active()->Name() : "nullptr")
              << "   (переключился на резервный сам)\n";

    const FallbackMatchSource both_up(fixtures, offline);
    std::cout << "FallbackMatchSource(fixtures, offline) — основной жив:\n";
    Describe(both_up);

    const FixtureMatchSource dead_a("match-v5", {}, false);
    const FixtureMatchSource dead_b("lcu", {}, false);
    const FallbackMatchSource nothing(dead_a, dead_b);
    std::cout << "FallbackMatchSource(dead, dead) — данных нет ниоткуда:\n";
    Describe(nothing);

    // Вложенность: FallbackMatchSource сам является MatchSource, поэтому его
    // можно передать следующему как основной или резервный. Объект обязан быть
    // именованным: внутри хранятся const-ссылки, и временный объект умер бы
    // сразу после точки с запятой, оставив висячую ссылку.
    const FallbackMatchSource chain(dead_a, live_first);
    std::cout << "FallbackMatchSource(dead, FallbackMatchSource(...)) — три источника цепочкой:\n";
    Describe(chain);

    Header("Тема 3 — BuildChampionIndex: слой анализа не знает про источники");

    const ChampionIndex index = BuildChampionIndex(fixtures);
    std::cout << "BuildChampionIndex(fixtures) — " << index.Size() << " чемпионов из "
              << fixtures.LoadMatches().size() << " записей:\n\n";
    PrintIndex(index);

    std::cout << "\nYasuo остался с нулём игр: его единственная запись битая "
                 "(duration_seconds == 0),\nи отбросил её ChampionReport::Add, а не builder — "
                 "чемпион из выгрузки не теряется.\n";

    const ChampionIndex from_fallback = BuildChampionIndex(live_first);
    const ChampionIndex from_nothing = BuildChampionIndex(nothing);
    std::cout << "\nта же функция, другие источники:\n"
              << "  BuildChampionIndex(live_first) -> " << from_fallback.Size() << " чемпионов\n"
              << "  BuildChampionIndex(nothing)    -> " << from_nothing.Size()
              << " чемпионов   (недоступный источник = пустой индекс)\n";
}

}  // namespace

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    std::cout << "Анализатор матчей League of Legends — то, что уже работает.\n"
              << "Темы 1, 2 и 3: " << "Champion, PlayerStats, ChampionReport, MatchFile, "
              << "MatchPayload,\nChampionIndex, MatchSource, FallbackMatchSource, BuildChampionIndex.\n";

    DemoTopic01();
    DemoTopic02();
    DemoTopic03();

    Header("Чего ещё нет");
    std::cout << "JSON из project/data/matches/ пока не читается — разбор появится в теме 8,\n"
                 "обход каталога в теме 6, сеть в теме 15. До тех пор источник данных —\n"
                 "записи, собранные руками в SampleEntries() в этом файле.\n\n";

    return 0;
}
