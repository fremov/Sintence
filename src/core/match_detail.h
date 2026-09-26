#ifndef SINTENCE_CORE_MATCH_DETAIL_H
#define SINTENCE_CORE_MATCH_DETAIL_H

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "match_source.h"  // MatchEntry — для агрегаций analysis/

// core/match_detail — законченный матч целиком: все десять участников.
//
// Отдельно от MatchEntry, потому что вопросы разные. MatchEntry — строка
// «как сыграл один игрок» для агрегаций по чемпионам; здесь — сам матч,
// каким его показывает окно профиля: кто был в игре, что собрал, сколько
// нанёс. Для агрегаций участник переводится в MatchEntry (ToMatchEntry),
// и анализ из analysis/ работает как раньше.
//
// Слой core: ни JSON, ни SQL, ни HTTP.

namespace sintence {

struct MatchParticipant {
    std::string puuid;
    std::string riot_id;       // "Имя#TAG" — как было в этом матче
    int team_id = 0;           // 100 синие, 200 красные
    bool win = false;
    int champion_id = 0;
    std::string champion;      // "Vladimir"
    std::string role;          // "MIDDLE" или "" (ARAM, Арена)
    int champ_level = 0;
    int kills = 0;
    int deaths = 0;
    int assists = 0;
    int cs = 0;                // миньоны + нейтральные
    int gold = 0;
    int damage_dealt = 0;      // по чемпионам
    int damage_taken = 0;
    int vision_score = 0;
    int wards_placed = 0;
    int wards_killed = 0;
    std::array<int, 7> items{};  // 0..5 и тринкет в 6-й, 0 — пусто
    int spell1 = 0;
    int spell2 = 0;
    int keystone = 0;
    int primary_style = 0;
    int sub_style = 0;
};

struct MatchDetail {
    std::string match_id;      // "RU_528252891"
    std::string platform;      // "RU"
    int queue_id = 0;          // 420 одиночная, 440 гибкая, 450 ARAM...
    std::string game_mode;     // "CLASSIC"
    std::string game_version;  // "15.12.688.6522"
    std::string patch;         // "15.12"
    long long started_at_ms = 0;
    int duration_seconds = 0;
    std::vector<MatchParticipant> participants;

    // Участник по puuid или nullptr: игрока в этом матче нет.
    const MatchParticipant* Find(const std::string& puuid) const;
};

// Строка для агрегаций analysis/ (ChampionReport, CountGamesByChampion...).
MatchEntry ToMatchEntry(const MatchParticipant& participant, int duration_seconds);

}  // namespace sintence

#endif  // SINTENCE_CORE_MATCH_DETAIL_H
