#include "champion_filters.h"

#include <algorithm>
#include <iterator>
#include <ranges>

namespace sintence {

int CountChampionsWithEnoughData(const ChampionIndex& index) {
    const std::vector<std::string> names = index.ChampionNames();
    const auto count = std::ranges::count_if(names, [&index](const std::string& name) {
        const ChampionReport* report = index.Find(name);
        return report != nullptr && report->HasEnoughData();
    });
    return static_cast<int>(count);
}

std::vector<std::string> ChampionsWithMinGames(const ChampionIndex& index, int min_games) {
    const std::vector<std::string> names = index.ChampionNames();

    std::vector<std::string> selected;
    selected.reserve(names.size());
    std::ranges::copy_if(names, std::back_inserter(selected),
                         [&index, min_games](const std::string& name) {
                             const ChampionReport* report = index.Find(name);
                             return report != nullptr && report->Games() >= min_games;
                         });
    return selected;
}

bool HasChampionAboveWinrate(const ChampionIndex& index, double min_winrate) {
    const std::vector<std::string> names = index.ChampionNames();
    return std::ranges::any_of(names, [&index, min_winrate](const std::string& name) {
        const ChampionReport* report = index.Find(name);
        // Достаточность выборки проверяется до винрейта: 100% по двум играм
        // не повод что-то советовать.
        return report != nullptr && report->HasEnoughData() &&
               report->Winrate() > min_winrate;
    });
}

}  // namespace sintence
