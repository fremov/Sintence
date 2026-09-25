#include "top_champions.h"

#include <algorithm>
#include <utility>

namespace sintence {

std::vector<ChampionGames> TopChampionsByGames(const std::map<std::string, int>& games,
                                               std::size_t limit) {
    std::vector<ChampionGames> rows;
    rows.reserve(games.size());
    for (const auto& [name, count] : games) {
        rows.push_back(ChampionGames{name, count});
    }

    // Критерий составной, поэтому проекция на одно поле не годится:
    // сначала по убыванию числа игр, при равенстве — по возрастанию имени.
    std::ranges::sort(rows, [](const ChampionGames& left, const ChampionGames& right) {
        if (left.games != right.games) {
            return left.games > right.games;
        }
        return left.champion_name < right.champion_name;
    });

    if (rows.size() > limit) {
        rows.resize(limit);
    }
    return rows;
}

}  // namespace sintence
