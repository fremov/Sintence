#ifndef SINTENCE_DATA_TIMELINE_JSON_H
#define SINTENCE_DATA_TIMELINE_JSON_H

#include <optional>
#include <string_view>

#include "match_history.h"  // MatchTimeline из core/

// data/timeline_json — ответ GET /lol/match/v5/matches/{id}/timeline,
// сведённый к подробностям матча: порядок прокачки и покупок каждого
// участника и разница золота команд по минутам.
//
// Правила:
//   - участники — metadata.participants по порядку: participantId 1..10;
//     1..5 — синие (100), 6..10 — красные (200);
//   - ITEM_UNDO отменяет последнюю покупку этого предмета (промах мышью
//     не должен попасть в «порядок покупок»);
//   - прокачка — SKILL_LEVEL_UP со skillSlot 1..4 (Q/W/E/R);
//   - золото — сумма participantFrames[].totalGold по командам на кадр;
//   - нет info.frames или metadata.participants — nullopt.

namespace sintence {

std::optional<MatchTimeline> ParseMatchTimeline(std::string_view json_text);

}  // namespace sintence

#endif  // SINTENCE_DATA_TIMELINE_JSON_H
