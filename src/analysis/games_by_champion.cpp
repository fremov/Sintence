#include "games_by_champion.h"

namespace sintence {
std::map<std::string, int> CountGamesByChampion(
    const std::vector<MatchEntry>& entries) {
    std::map<std::string, int> games;
    for (const auto& entry : entries) {
        if (entry.champion_name.empty())
            continue;
        ++games[entry.champion_name];
    }
    return games;
}

int GamesOf(const std::map<std::string, int>& games,
            const std::string& champion_name) {
    if (const auto found = games.find(champion_name); found == games.end()) {
        return 0;
    } else {
        return found->second;
    }
}

std::string MostPlayedChampion(const std::map<std::string, int>& games) {
    if (games.empty()) {
        return "";
    }
    int best_count = 0;
    std::string champion_name;
    for (const auto& [name, count] : games) {
        if (count > best_count) {
            best_count = count;
            champion_name = name;
        }
    }
    return champion_name;
}
} // namespace sintence