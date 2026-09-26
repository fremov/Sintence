#ifndef SINTENCE_CORE_MATCH_HISTORY_H
#define SINTENCE_CORE_MATCH_HISTORY_H

#include <optional>
#include <string>
#include <vector>

#include "player_profile.h"  // RankedStats

// core/match_history — то, что окно профиля знает об игроке, и timeline
// матча для подробностей. Слой core: ни JSON, ни SQL.

namespace sintence {

// Аккаунт в игре: иконка и уровень призывателя (summoner-v4).
struct SummonerInfo {
    int profile_icon_id = 0;
    int level = 0;
};

// Состояние загрузки истории игрока — для прогресса «23 из 50».
struct HistoryStatus {
    std::string riot_id;
    std::string puuid;       // пусто, пока не узнали
    SummonerInfo summoner;
    std::optional<RankedStats> solo;
    std::optional<RankedStats> flex;
    int depth = 0;           // сколько последних игр заказано
    int ids_known = 0;       // сколько id матчей Riot уже назвал
    int stored = 0;          // сколько из них уже в хранилище
    bool running = false;    // загрузка идёт прямо сейчас
    std::string error;       // «игрок не найден» и т.п.; пусто — всё в порядке
};

// Timeline матча, сведённый к тому, что показывают подробности.
struct TimelinePurchase {
    int item_id = 0;
    int minute = 0;
};

struct TimelineParticipant {
    std::string puuid;
    std::string skills;                       // "QWEQQRQ..." — порядок прокачки
    std::vector<TimelinePurchase> purchases;  // по порядку, без отменённых
    std::vector<int> death_seconds;           // когда умирал, секунды от начала
};

struct MatchTimeline {
    // Разница золота «синие минус красные» на каждой минуте, с нулевой.
    std::vector<int> gold_diff;
    std::vector<TimelineParticipant> participants;
};

}  // namespace sintence

#endif  // SINTENCE_CORE_MATCH_HISTORY_H
