#ifndef SINTENCE_DATA_MATCH_DETAIL_JSON_H
#define SINTENCE_DATA_MATCH_DETAIL_JSON_H

#include <optional>
#include <string_view>

#include "match_detail.h"  // MatchDetail из core/

namespace sintence {

// data/match_detail_json — ответ GET /lol/match/v5/matches/{id} целиком
// в MatchDetail: все десять участников с предметами, рунами и уроном.
//
// Правила:
//   - нет metadata.matchId или info.participants — nullopt;
//   - участник без puuid пропускается (бывает у ботов в пользовательских
//     играх), остальные разбираются;
//   - отсутствующее числовое поле — 0: старые матчи и режимы (Арена)
//     присылают не всё, и это не повод терять матч;
//   - patch — первые два числа gameVersion: "15.12.688.6522" -> "15.12";
//   - started_at — gameStartTimestamp, а если его нет — gameCreation.
std::optional<MatchDetail> ParseMatchDetail(std::string_view json_text);

}  // namespace sintence

#endif  // SINTENCE_DATA_MATCH_DETAIL_JSON_H
