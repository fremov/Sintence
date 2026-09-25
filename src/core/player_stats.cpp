#include "player_stats.h"

namespace sintence {
PlayerStats::PlayerStats(std::string champion_name) : champion_name_(
    std::move(champion_name)) {
}

void PlayerStats::AddMatch(int kills, int deaths, int assists, bool win) {
    // Объект, который успел изменить два поля из пяти и передумал, —
    // это худший из возможных вариантов.
    if (kills >= 0 && deaths >= 0 && assists >= 0) {
        total_kills_ += kills;
        total_deaths_ += deaths;
        total_assists_ += assists;
        wins_ += win;
        games_ += 1;
    }
}

const std::string& PlayerStats::ChampionName() const {
    return champion_name_;
}

int PlayerStats::Games() const {
    return games_;
}

int PlayerStats::Wins() const {
    return wins_;
}

int PlayerStats::Losses() const {
    return Games() - Wins();
}

int PlayerStats::TotalKills() const {
    return total_kills_;
}

int PlayerStats::TotalDeaths() const {
    return total_deaths_;
}

int PlayerStats::TotalAssists() const {
    return total_assists_;
}
} // namespace sintence