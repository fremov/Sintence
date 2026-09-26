#ifndef SINTENCE_ANALYSIS_PLAYSTYLE_H
#define SINTENCE_ANALYSIS_PLAYSTYLE_H

#include <string>
#include <unordered_map>
#include <vector>

#include "match_detail.h"  // MatchDetail из core/

// analysis/playstyle — как играет игрок: плашки вроде «рано умирает»,
// «давит башни», «серия поражений» по его последним матчам.
//
// Каждая плашка — сравнение с типичным игроком той же роли: фарм, урон по
// постройкам, убийства в начале и обзор у поддержки и у топера разные,
// и мерить их одной меркой бессмысленно. Типичные значения — медианы
// по ранговым и обычным играм Ущелья из базы истории (сентябрь 2026,
// 119 матчей, в основном Платина — Изумруд), см. kRoleBaselines в .cpp.
//
// Только прошлое: плашка описывает сыгранные матчи, а не предсказывает
// действия в идущем (POLICY.md — выводов за игрока не делаем). Всё из
// публичной истории match-v5.
//
// Чистая функция над типами core/: ни хранилища, ни сети.

namespace sintence {

enum class StyleTag {
    kFarmer,           // фармит заметно больше типичного на своей роли
    kLowFarm,          // заметно меньше
    kPusher,           // много урона по постройкам — давит линии, сплитпушит
    kEarlyAggression,  // много убийств и помощи в первые 10 минут
    kLaneWinner,       // часто выходит из лайнинга впереди соперника
    kEarlyDeaths,      // часто умирает в первые 10 минут (нужен timeline)
    kDiesOften,        // много смертей за игру
    kRarelyDies,       // мало смертей за игру
    kTeamPlayer,       // высокое участие в убийствах команды
    kSoloPlayer,       // низкое участие — играет отдельно от команды
    kVisionControl,    // много обзора для своей роли
    kLowVision,        // мало обзора
    kWinStreak,        // несколько побед подряд в последних играх
    kLossStreak,       // несколько поражений подряд
    kOneTrick,         // почти всё время на одном чемпионе
};

struct StyleFinding {
    StyleTag tag = StyleTag::kFarmer;
    double value = 0;    // у игрока: в среднем за игру, доля, серия...
    double typical = 0;  // у типичного игрока тех же ролей; 0 — сравнения нет
    int games = 0;       // на скольких играх посчитано
    int champion_id = 0; // для kOneTrick — какой чемпион
};

struct PlayStyle {
    int games = 0;                      // сколько матчей игрока разобрано
    int role_games = 0;                 // из них на Ущелье с ролью
    std::vector<StyleFinding> tags;     // в порядке важности: серии, смерти, стиль
    std::vector<std::pair<int, int>> champion_games;  // id чемпиона -> игр, больше — выше
};

// Меньше этого числа игр с ролью — плашки по роли не ставятся: по трём
// играм «слабо фармит» — случайность, а не стиль.
inline constexpr int kMinStyleGames = 5;

// matches — последние матчи игрока, новые первыми (как отдаёт хранилище);
// early_deaths — смерти игрока до 10-й минуты по тем матчам, где timeline
// уже скачан (match_id -> число). Матчи, где игрока нет, пропускаются.
PlayStyle AnalyzePlayStyle(const std::vector<MatchDetail>& matches, const std::string& puuid,
                           const std::unordered_map<std::string, int>& early_deaths = {});

}  // namespace sintence

#endif  // SINTENCE_ANALYSIS_PLAYSTYLE_H
