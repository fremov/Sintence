#include "ranges_summary.h"

#include <algorithm>
#include <numeric>
#include <ranges>

namespace course {
std::vector<ChampionSummary> BuildSummaries(const ChampionIndex& index) {
    return index.ChampionNames() |
           std::views::transform([&index](const std::string& name) {
               const ChampionReport* report = index.Find(name);
               if (report == nullptr) {
                   return ChampionSummary{name, 0, 0.0, 0.0, false};
               }
               return ChampionSummary{report->ChampionName(), report->Games(),
                                      report->Winrate(), report->Kda(),
                                      report->HasEnoughData()};
           }) |
           std::ranges::to<std::vector>();
}

std::vector<ChampionSummary> TopByWinrate(
    const std::vector<ChampionSummary>& summaries, std::size_t limit) {
    auto filtered_rows = summaries |
                         std::views::filter(&ChampionSummary::enough_data) |
                         std::ranges::to<std::vector>();
    std::ranges::sort(filtered_rows, std::ranges::greater{},
                      &ChampionSummary::winrate);
    return filtered_rows | std::views::take(limit) |
           std::ranges::to<std::vector>();
}

int TotalGames(const std::vector<ChampionSummary>& summaries) {
    return std::ranges::fold_left(
        summaries, 0,
        [](int acc, const ChampionSummary& s) { return s.games + acc; });
}
}  // namespace course