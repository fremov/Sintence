#include "profile_summary.h"

#include <algorithm>
#include <map>

namespace sintence {

double ProfileChampion::Kda() const {
    const int divisor = deaths == 0 ? 1 : deaths;
    return static_cast<double>(kills + assists) / divisor;
}

ProfileSummary SummarizeProfile(const std::vector<MatchDetail>& matches,
                                const std::string& puuid) {
    ProfileSummary summary;
    std::map<std::string, ProfileRole> roles;
    std::map<int, ProfileChampion> champions;

    for (const MatchDetail& match : matches) {
        const MatchParticipant* self = match.Find(puuid);
        if (self == nullptr) {
            continue;
        }
        ++summary.games;
        summary.wins += self->win ? 1 : 0;

        ProfileRole& role = roles[self->role];
        role.role = self->role;
        ++role.games;
        role.wins += self->win ? 1 : 0;

        ProfileChampion& champion = champions[self->champion_id];
        champion.champion_id = self->champion_id;
        champion.champion = self->champion;
        ++champion.games;
        champion.wins += self->win ? 1 : 0;
        champion.kills += self->kills;
        champion.deaths += self->deaths;
        champion.assists += self->assists;
        champion.cs += self->cs;
        champion.duration_seconds += match.duration_seconds;
    }

    for (auto& [key, role] : roles) {
        summary.roles.push_back(role);
    }
    for (auto& [id, champion] : champions) {
        summary.champions.push_back(champion);
    }
    std::ranges::stable_sort(summary.roles, [](const ProfileRole& a, const ProfileRole& b) {
        return a.games > b.games;
    });
    std::ranges::sort(summary.champions,
                      [](const ProfileChampion& a, const ProfileChampion& b) {
                          if (a.games != b.games) {
                              return a.games > b.games;
                          }
                          return a.champion < b.champion;
                      });
    return summary;
}

}  // namespace sintence
