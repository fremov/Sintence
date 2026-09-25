#include "champion_index.h"

namespace sintence {
void ChampionIndex::AddReport(ChampionReport report) {
    for (auto& existing : reports_) {
        if (report.ChampionName() == existing.ChampionName()) {
            existing = std::move(report);
            return;
        }
    }
    reports_.push_back(std::move(report));
}

const ChampionReport* ChampionIndex::Find(
    const std::string& champion_name) const {
    for (const auto& report : reports_) {
        if (report.ChampionName() == champion_name) {
            return &report;
        }
    }
    return nullptr;
}

int ChampionIndex::Size() const {
    return static_cast<int>(reports_.size());
}

bool ChampionIndex::Empty() const {
    return reports_.empty();
}

std::vector<std::string> ChampionIndex::ChampionNames() const {
    std::vector<std::string> res;
    for (const auto& report : reports_) {
        res.push_back(report.ChampionName());
    }
    return res;
}
} // namespace sintence