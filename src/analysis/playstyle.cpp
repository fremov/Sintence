#include "playstyle.h"

#include <algorithm>
#include <map>
#include <string_view>

namespace sintence {

namespace {

// Типичный игрок роли. Медианы по 119 матчам Ущелья из базы истории
// (ранговые и обычные, сентябрь 2026) — см. заголовок. Поменяется мета
// или база вырастет — пересчитать здесь, а не подкручивать пороги.
struct RoleBaseline {
    std::string_view role;
    double cs_per_minute;
    double buildings_per_minute;  // урон по постройкам в минуту
    double early_takedowns;       // убийства + помощь до 10-й минуты
    double vision_per_minute;
    double kill_participation;
    double deaths;
    double lane_lead_rate;        // доля игр, где вышел из лайнинга впереди
    double early_deaths;          // смертей до 10-й минуты (по timeline)
};

constexpr RoleBaseline kRoleBaselines[] = {
    {"TOP", 6.5, 330, 3.0, 0.72, 0.38, 7.0, 0.25, 1.9},
    {"JUNGLE", 6.2, 20, 5.0, 0.84, 0.46, 7.0, 0.10, 1.25},
    {"MIDDLE", 6.6, 210, 4.0, 0.62, 0.43, 7.0, 0.15, 1.45},
    {"BOTTOM", 7.0, 250, 6.0, 0.62, 0.45, 7.0, 0.13, 1.95},
    {"UTILITY", 1.3, 50, 6.0, 2.35, 0.50, 8.0, 0.09, 1.75},
};
// Ранние смерти — средние по 199 игрокам из 22 timeline (сентябрь 2026).
// Выборка маленькая, но оценка «около одной» оказалась вдвое ниже правды:
// с ней «рано умирает» стояло бы почти у всех.

// Игра короче — ремейк: ни стиля, ни исхода.
constexpr int kRemakeSeconds = 300;

// Серия — от трёх игр подряд.
constexpr int kStreak = 3;

const RoleBaseline* BaselineFor(std::string_view role) {
    for (const RoleBaseline& baseline : kRoleBaselines) {
        if (baseline.role == role) {
            return &baseline;
        }
    }
    return nullptr;
}

// Среднее у игрока и у типичного игрока тех же ролей — по одним и тем же играм.
struct Pair {
    double own = 0;
    double typical = 0;
    int games = 0;

    void Add(double value, double baseline) {
        own += value;
        typical += baseline;
        ++games;
    }
    double Own() const { return games ? own / games : 0; }
    double Typical() const { return games ? typical / games : 0; }
    double Ratio() const { return typical > 0 ? own / typical : 0; }
};

StyleFinding Finding(StyleTag tag, const Pair& pair) {
    return {tag, pair.Own(), pair.Typical(), pair.games, 0};
}

}  // namespace

PlayStyle AnalyzePlayStyle(const std::vector<MatchDetail>& matches, const std::string& puuid,
                           const std::unordered_map<std::string, int>& early_deaths) {
    PlayStyle style;
    std::map<int, int> champions;
    std::vector<bool> results;  // новые первыми

    Pair farm, buildings, early, vision, kill_share, deaths, lane, dying_early;

    for (const MatchDetail& match : matches) {
        const MatchParticipant* self = match.Find(puuid);
        if (self == nullptr || match.duration_seconds < kRemakeSeconds) {
            continue;
        }
        ++style.games;
        ++champions[self->champion_id];
        results.push_back(self->win);

        const RoleBaseline* baseline = BaselineFor(self->role);
        if (baseline == nullptr) {
            continue;  // ARAM, Арена: ни ролей, ни линий
        }
        ++style.role_games;
        const double minutes = match.duration_seconds / 60.0;
        const std::string_view role = self->role;

        if (role != "UTILITY") {
            farm.Add(self->cs / minutes, baseline->cs_per_minute);
        }
        // Урон по постройкам у леса и поддержки — случайность добиваний,
        // а не стиль: плашку «давит башни» им не считаем.
        if (role != "JUNGLE" && role != "UTILITY") {
            buildings.Add(self->damage_to_buildings / minutes, baseline->buildings_per_minute);
            if (self->lane_lead >= 0) {
                lane.Add(self->lane_lead, baseline->lane_lead_rate);
            }
        }
        early.Add(self->early_takedowns, baseline->early_takedowns);
        vision.Add(self->vision_score / minutes, baseline->vision_per_minute);
        deaths.Add(self->deaths, baseline->deaths);

        int team_kills = 0;
        for (const MatchParticipant& p : match.participants) {
            team_kills += p.team_id == self->team_id ? p.kills : 0;
        }
        if (team_kills > 0) {
            kill_share.Add(static_cast<double>(self->kills + self->assists) / team_kills,
                           baseline->kill_participation);
        }
        if (const auto found = early_deaths.find(match.match_id); found != early_deaths.end()) {
            dying_early.Add(found->second, baseline->early_deaths);
        }
    }

    // Серии: считаются от самой свежей игры.
    if (!results.empty()) {
        int streak = 1;
        while (streak < static_cast<int>(results.size()) && results[streak] == results[0]) {
            ++streak;
        }
        if (streak >= kStreak) {
            style.tags.push_back(
                {results[0] ? StyleTag::kWinStreak : StyleTag::kLossStreak, double(streak), 0, streak, 0});
        }
    }

    const auto enough = [](const Pair& pair) { return pair.games >= kMinStyleGames; };

    if (enough(dying_early) && dying_early.Ratio() >= 1.4) {
        style.tags.push_back(Finding(StyleTag::kEarlyDeaths, dying_early));
    }
    if (enough(deaths) && deaths.Ratio() >= 1.3) {
        style.tags.push_back(Finding(StyleTag::kDiesOften, deaths));
    } else if (enough(deaths) && deaths.Ratio() <= 0.65) {
        style.tags.push_back(Finding(StyleTag::kRarelyDies, deaths));
    }
    if (enough(lane) && lane.Own() >= std::max(0.3, 2 * lane.Typical())) {
        style.tags.push_back(Finding(StyleTag::kLaneWinner, lane));
    }
    if (enough(early) && early.Ratio() >= 1.35) {
        style.tags.push_back(Finding(StyleTag::kEarlyAggression, early));
    }
    if (enough(buildings) && buildings.Ratio() >= 1.5) {
        style.tags.push_back(Finding(StyleTag::kPusher, buildings));
    }
    if (enough(farm) && farm.Ratio() >= 1.15) {
        style.tags.push_back(Finding(StyleTag::kFarmer, farm));
    } else if (enough(farm) && farm.Ratio() <= 0.8) {
        style.tags.push_back(Finding(StyleTag::kLowFarm, farm));
    }
    if (enough(kill_share) && kill_share.Own() - kill_share.Typical() >= 0.1) {
        style.tags.push_back(Finding(StyleTag::kTeamPlayer, kill_share));
    } else if (enough(kill_share) && kill_share.Own() - kill_share.Typical() <= -0.1) {
        style.tags.push_back(Finding(StyleTag::kSoloPlayer, kill_share));
    }
    if (enough(vision) && vision.Ratio() >= 1.35) {
        style.tags.push_back(Finding(StyleTag::kVisionControl, vision));
    } else if (enough(vision) && vision.Ratio() <= 0.65) {
        style.tags.push_back(Finding(StyleTag::kLowVision, vision));
    }

    for (const auto& [champion_id, games] : champions) {
        style.champion_games.emplace_back(champion_id, games);
    }
    std::ranges::stable_sort(style.champion_games,
                             [](const auto& a, const auto& b) { return a.second > b.second; });
    if (!style.champion_games.empty()) {
        const auto [champion_id, games] = style.champion_games.front();
        const double share = static_cast<double>(games) / style.games;
        if (games >= 6 && share >= 0.6) {
            style.tags.push_back({StyleTag::kOneTrick, share, 0, games, champion_id});
        }
    }
    return style;
}

}  // namespace sintence
