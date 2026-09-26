#ifndef SINTENCE_ANALYSIS_PROFILE_SUMMARY_H
#define SINTENCE_ANALYSIS_PROFILE_SUMMARY_H

#include <string>
#include <vector>

#include "match_detail.h"  // MatchDetail из core/

// analysis/profile_summary — сводка игрока по его последним матчам:
// роли и чемпионы с винрейтом и KDA. То, что окно профиля показывает
// справа от списка игр.
//
// Чистая функция над типами core/: ни хранилища, ни сети.

namespace sintence {

struct ProfileRole {
    std::string role;  // "MIDDLE"; "" — режимы без ролей (ARAM, Арена)
    int games = 0;
    int wins = 0;
};

struct ProfileChampion {
    int champion_id = 0;
    std::string champion;  // "Vladimir"
    int games = 0;
    int wins = 0;
    int kills = 0;
    int deaths = 0;
    int assists = 0;
    int cs = 0;
    int duration_seconds = 0;  // сумма длительностей — для CS в минуту

    // (убийства + помощь) / смерти; ноль смертей делим на единицу —
    // то же соглашение, что в ChampionReport::Kda().
    double Kda() const;
};

struct ProfileSummary {
    int games = 0;
    int wins = 0;
    std::vector<ProfileRole> roles;          // больше игр — выше
    std::vector<ProfileChampion> champions;  // больше игр — выше
};

// Матчи, где игрока (puuid) нет, пропускаются. Равенство игр — порядок
// по имени, чтобы список не прыгал между обновлениями.
ProfileSummary SummarizeProfile(const std::vector<MatchDetail>& matches,
                                const std::string& puuid);

}  // namespace sintence

#endif  // SINTENCE_ANALYSIS_PROFILE_SUMMARY_H
