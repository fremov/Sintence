#include "champion_report.h"

namespace course {
ChampionReport::ChampionReport(std::string champion_name) : champion_name_(
    std::move(champion_name)) {
}

namespace {
bool IsValid(const MatchLine& line) {
    if (line.duration_seconds > 0 and line.kills >= 0 and line.deaths >= 0 and
        line.assists >= 0 and line.minions >= 0) {
        return true;
    }
    return false;
}
}

void ChampionReport::Add(const MatchLine& line) {
    // Если условие проверки не помещается в одну читаемую строку,
    // это намёк, что ему нужна своя функция с именем.
    if (IsValid(line)) {
        lines_.push_back(line);
    }
}

const std::string& ChampionReport::ChampionName() const {
    return champion_name_;
}

int ChampionReport::Games() const {
    return lines_.size();
}

double ChampionReport::Kda() const {
    if (lines_.empty()) {
        return 0.0;
    }
    auto kills = 0;
    auto death = 0;
    auto assist = 0;
    for (const MatchLine& line : lines_) {
        kills += line.kills;
        death += line.deaths;
        assist += line.assists;
    }
    if (death == 0) {
        return (static_cast<double>(kills) + assist) / 1;
    } else {
        return (static_cast<double>(kills) + assist) / death;
    }
}

double ChampionReport::Winrate() const {
    if (lines_.empty()) {
        return 0.0;
    }
    auto wins = 0;
    for (const MatchLine& line : lines_) {
        if (line.win) {
            ++wins;
        }
    }
    return static_cast<double>(wins) / lines_.size();
}

double ChampionReport::CsPerMinute() const {
    if (lines_.empty()) {
        return 0.0;
    }
    auto minions = 0;
    auto minutes = 0;
    for (const MatchLine& line : lines_) {
        minions += line.minions;
        minutes += line.duration_seconds;
    }
    return static_cast<double>(minions) / (static_cast<double>(minutes) / 60);
}

bool ChampionReport::HasEnoughData() const {
    if (lines_.empty()) {
        return false;
    }
    if (lines_.size() >= kMinGamesForConclusion) {
        return true;
    }
    return false;
}
} // namespace course