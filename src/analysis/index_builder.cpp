#include "index_builder.h"

namespace sintence {
ChampionIndex BuildChampionIndex(const MatchSource& source) {
    ChampionIndex index;
    if (!source.IsAvailable()) {
        return index;
    }
    std::vector<MatchEntry> entries = source.LoadMatches();
    std::vector<ChampionReport> reports;
    for (auto& entry : entries) {
        if(entry.champion_name.empty()) continue;
        bool found = false;
        for (auto& report : reports) {
            if (report.ChampionName() == entry.champion_name) {
                report.Add(entry.line);
                found = true;
            }
        }
        if (!found) {
            ChampionReport rpt(entry.champion_name);
            rpt.Add(entry.line);
            reports.push_back(std::move(rpt));
        }
    }
    for (auto& report : reports) {
        index.AddReport(std::move(report));
    }
    return index;
}
} // namespace sintence