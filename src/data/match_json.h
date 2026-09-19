#ifndef ANALYZER_DATA_MATCH_JSON_H
#define ANALYZER_DATA_MATCH_JSON_H

#include <optional>
#include <string>
#include <string_view>

#include "match_source.h"  // MatchEntry, MatchLine

// ============================================================================
// data/match_json — превращает ответ Riot Match-V5 в MatchEntry.
//
// Это первый адаптер из project/ARCHITECTURE.md: «каждый умеет ровно одно —
// превратить чужой JSON в типы core/». Ни одного nlohmann-типа наружу
// не выходит, и это проверяется тем, что в этом заголовке его нет.
//
// Критерии приёмки:
//   1. В match_json.h не появилось ни одного типа из nlohmann — библиотека
//      видна только внутри .cpp. Правило 4 из ARCHITECTURE.md.
//   2. Битый JSON не роняет программу: возвращается std::nullopt.
//   3. Отсутствующее или нечисловое поле — тоже nullopt, а не мусор в числе.
//   4. Матч, в котором нужного игрока нет, — nullopt, и это не ошибка.
//   5. Заглушек и TODO в файле не осталось.
//
// Одна идея: разбор чужого формата живёт в одном месте, и всё остальное
// приложение о JSON не знает.
// ============================================================================

namespace course {

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

}  // namespace course

#endif  // ANALYZER_DATA_MATCH_JSON_H
