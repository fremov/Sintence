#ifndef SINTENCE_DATA_LIVE_CLIENT_JSON_H
#define SINTENCE_DATA_LIVE_CLIENT_JSON_H

#include <optional>
#include <string_view>
#include <vector>
#include "json_helpers.h"

#include "live_game.h"  // LivePlayer, LiveGameStats из core/

// data/live_client_json — превращает ответы Live Client Data API в типы core/.
//
// nlohmann живёт внутри .cpp, наружу выходят только доменные типы.
//
// Правила разбора табло:
//   - битый JSON даёт nullopt, а не исключение;
//   - обязательные поля игрока — championName, team и объект scores
//     с четырьмя числами; участник без любого из них пропускается,
//     остальные девять разбираются;
//   - необязательные — level, position, isBot, isDead: их отсутствие
//     оставляет значение по умолчанию из LivePlayer (в ARAM position
//     приходит пустым всегда);
//   - неизвестная команда (не ORDER и не CHAOS) — участник пропускается,
//     а не уезжает в Order по умолчанию.

namespace sintence {

// Разбирает ответ GET /liveclientdata/playerlist — массив из десяти игроков.
//
// Контракт:
//   - json_text — тело ответа, это JSON-МАССИВ, а не объект;
//   - результат: по одной LivePlayer на каждого корректного участника,
//     порядок сохраняется (Riot отдаёт сначала ORDER, потом CHAOS);
//   - JSON не разбирается или это не массив — nullopt;
//   - пустой массив — пустой вектор, это не ошибка (ещё не все загрузились).
//
// Откуда берутся поля (сверено с developer.riotgames.com/docs/lol,
// раздел Game Client API -> Live Client Data API -> All Players):
//   championName      -> champion_name
//   riotId            -> riot_id          ("Riot Tuxedo#TXC1")
//   position          -> position          (в ARAM приходит "")
//   team              -> team              ("ORDER" | "CHAOS")
//   level             -> level
//   isBot             -> is_bot
//   isDead            -> is_dead
//   scores.kills      -> kills
//   scores.deaths     -> deaths
//   scores.assists    -> assists
//   scores.creepScore -> creep_score
//
// Внимание: kills/deaths/assists лежат во вложенном объекте scores,
// а не в корне записи игрока. Это единственное место с вложенностью.
std::optional<std::vector<LivePlayer>> ParseLivePlayers(std::string_view json_text);

// Каноническое имя чемпиона ("Zed", "MonkeyKing") из служебной строки клиента.
//
// Форматов у Riot несколько, и все встречаются в одном матче:
//   "game_character_displayname_Zed"             -> "Zed"
//   "Character_Aatrox_Name"                      -> "Aatrox"  (так пришёл Атрокс)
//   "game_character_skin_displayname_Vladimir_5" -> "Vladimir" (rawSkinName)
// Правило: из частей через '_' берётся та, что не служебное слово и не число.
// Хвост после последнего подчёркивания, как было раньше, давал для Атрокса
// "Name" — и у него не было ни иконки, ни советов, ни способностей.
// Ничего подходящего — пустая строка.
std::string ChampionKeyFromRaw(std::string_view raw);

// Разбирает ответ GET /liveclientdata/gamestats.
//
// Контракт:
//   - gameMode  -> game_mode;
//   - mapName   -> map_name;
//   - gameTime  -> game_time_seconds (ДРОБНОЕ число: 1234.56789);
//   - JSON не разбирается или это не объект — nullopt;
//   - отсутствующее поле — nullopt: без времени матча снимок бесполезен.
std::optional<LiveGameStats> ParseLiveGameStats(std::string_view json_text);

// Разбирает ответ GET /liveclientdata/allgamedata — весь снимок разом.
//
// Один запрос вместо трёх, и это не экономия ради экономии: gamestats,
// playerlist и activeplayer, взятые по отдельности, относятся к РАЗНЫМ
// моментам игры, а здесь всё согласовано между собой.
//
// Контракт:
//   - отсутствует gameData или allPlayers -> nullopt: без табло и времени
//     снимок бесполезен;
//   - activePlayer отсутствует или пришёл не объектом (режим наблюдателя,
//     реплей) -> игра возвращается БЕЗ активного игрока, это не ошибка;
//   - разбор игроков и статистики — те же правила, что у функций выше.
//
// Что добавляется по сравнению с playerlist:
//   allPlayers[].items            -> LivePlayer::items (у всех десяти)
//   activePlayer.abilities        -> уровни Q/W/E/R и пассивки
//   activePlayer.fullRunes        -> keystone, два дерева, малые руны
//   activePlayer.currentGold      -> золото на руках
std::optional<LiveGame> ParseAllGameData(std::string_view json_text);

}  // namespace sintence

#endif  // SINTENCE_DATA_LIVE_CLIENT_JSON_H
