#include "index_by_map.h"

#include <map>
#include <string>

namespace sintence {
ChampionIndex BuildChampionIndexFast(const MatchSource& source) {
    ChampionIndex index;
    if (!source.IsAvailable()) {
        return index;
    }
    auto matches = source.LoadMatches();
    std::map<std::string, ChampionReport> reports;
    for (const auto& entry : matches) {
        if (entry.champion_name.empty()) {
            continue;
        }
        reports.try_emplace(entry.champion_name, entry.champion_name).first
               ->second.Add(entry.line);
    }
    for (auto& report : reports) {
        index.AddReport(report.second);
    }
    return index;
}
} // namespace sintence