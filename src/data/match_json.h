#ifndef SINTENCE_DATA_MATCH_JSON_H
#define SINTENCE_DATA_MATCH_JSON_H


#include <optional>
#include <string_view>
#include "json_helpers.h"
#include "match_source.h"  // MatchEntry, MatchLine

// data/match_json — превращает ответ Riot Match-V5 в MatchEntry.
//
// Разбор чужого формата живёт в одном месте; ни одного типа nlohmann наружу
// не выходит.
//
// nullopt возвращается, если JSON не разбирается, если обязательное поле
// отсутствует или не того типа, и если нужного игрока в матче нет —
// последнее не ошибка.

namespace sintence {

// Разбирает один матч Match-V5 и достаёт из него строку нужного игрока.
//
// Контракт:
//   - json_text — содержимое файла вида project/data/matches/RU_*.json;
//   - puuid — игрок, чью строку надо достать (их в матче десять);
//   - вернулось значение: champion_name и line заполнены;
//   - вернулся nullopt, если: JSON не разбирается, нужного игрока в матче нет,
//     или обязательное поле отсутствует / не того типа.
//
// Откуда берутся поля (project/data/README.md, раздел про схему):
//   info.participants[].puuid                -> найти себя среди десяти
//   info.participants[].championName         -> champion_name
//   info.participants[].kills/deaths/assists -> line.kills/deaths/assists
//   info.participants[].win                  -> line.win
//   info.participants[].totalMinionsKilled
//     + info.participants[].neutralMinionsKilled -> line.minions
//   info.gameDuration                        -> line.duration_seconds
//
// Внимание: gameDuration лежит в info, а не в participant — это единственное
// поле, которое берётся не из строки игрока.

std::optional<MatchEntry> ParseMatchEntry(std::string_view json_text, std::string_view puuid);

}  // namespace sintence

#endif  // SINTENCE_DATA_MATCH_JSON_H
