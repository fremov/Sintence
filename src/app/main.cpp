// ============================================================================
// analyzer — точка входа.
//
// Этот файл мой, а не твой: он ничего не считает и не разбирает, только
// связывает готовые куски и печатает результат. Вся работа — в core/,
// data/ и analysis/.
//
// Запуск:
//   analyzer                          каталог и puuid берутся из выгрузки
//   analyzer <каталог>                puuid из <каталог>/index.json
//   analyzer <каталог> <puuid>        всё явно
// ============================================================================

#include <filesystem>
#include <fstream>
#include <print>
#include <sstream>
#include <string>
#include <vector>

#include "json.hpp"

#include "champion_index.h"
#include "champion_report.h"
#include "file_match_source.h"
#include "games_by_champion.h"
#include "index_by_map.h"
#include "top_champions.h"

namespace {

// puuid игрока лежит в манифесте выгрузки рядом с матчами.
// Если манифеста нет — вернётся пустая строка, и приложение честно об этом скажет.
std::string ReadPuuidFromIndex(const std::filesystem::path& dir) {
    std::ifstream file(dir / "index.json");
    if (!file) {
        return {};
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();

    const auto doc = nlohmann::json::parse(buffer.str(), nullptr, false);
    if (doc.is_discarded()) {
        return {};
    }
    return doc.value("puuid", std::string{});
}

void PrintTable(const course::ChampionIndex& index) {
    std::println("{:<16} {:>5} {:>9} {:>7} {:>8}  {}", "чемпион", "игр", "winrate",
                 "KDA", "CS/min", "данных хватает");
    std::println("{}", std::string(64, '-'));

    for (const std::string& name : index.ChampionNames()) {
        const course::ChampionReport* report = index.Find(name);
        if (report == nullptr) {
            continue;
        }
        std::println("{:<16} {:>5} {:>8.1f}% {:>7.2f} {:>8.1f}  {}", name, report->Games(),
                     report->Winrate() * 100.0, report->Kda(), report->CsPerMinute(),
                     report->HasEnoughData() ? "да" : "нет");
    }
}

}  // namespace

int main(int argc, char** argv) {
    const std::filesystem::path dir =
        argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path(ANALYZER_DEFAULT_DATA_DIR);
    const std::string puuid = argc > 2 ? std::string(argv[2]) : ReadPuuidFromIndex(dir);

    std::println("Каталог: {}", dir.string());

    if (puuid.empty()) {
        std::println("Не удалось определить puuid: нет {}/index.json или в нём нет поля puuid.",
                     dir.string());
        std::println("Передай его вторым аргументом: analyzer <каталог> <puuid>");
        return 1;
    }
    std::println("Игрок:   {}...{}", puuid.substr(0, 8), puuid.substr(puuid.size() - 4));

    const course::FileMatchSource source(dir, puuid);
    if (!source.IsAvailable()) {
        std::println("Источник недоступен. Либо каталога нет, либо в нём нет матчей,");
        std::println("либо FileMatchSource::IsAvailable ещё не реализован.");
        return 1;
    }

    const std::vector<course::MatchEntry> entries = source.LoadMatches();
    std::println("Прочитано записей: {} из {} файлов\n", entries.size(), source.JsonFileCount());

    const course::ChampionIndex index = course::BuildChampionIndexFast(source);
    if (index.Empty()) {
        std::println("Индекс пуст. Дальше печатать нечего — значит, данные");
        std::println("до анализа не доехали: смотри ParseMatchEntry и LoadMatches.");
        return 1;
    }

    PrintTable(index);

    // Ниже — то, что появится, когда будут написаны функции analysis/.
    // Сейчас они возвращают пустоту, и это видно прямо в выводе.
    std::println("");
    const auto top = course::TopChampionsByGames(course::CountGamesByChampion(entries), 5);
    if (top.empty()) {
        std::println("[TopChampionsByGames пока не реализован — топ-5 будет здесь]");
    } else {
        std::println("Топ-5 по числу игр:");
        for (const course::ChampionGames& row : top) {
            std::println("  {:<16} {:>4}", row.champion_name, row.games);
        }
    }

    return 0;
}
