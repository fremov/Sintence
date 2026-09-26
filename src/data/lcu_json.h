#ifndef SINTENCE_DATA_LCU_JSON_H
#define SINTENCE_DATA_LCU_JSON_H

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "lobby.h"  // Lobby, LobbyMember из core/

// data/lcu_json — разбор ответов клиента League (LCU API) в типы core/.
//
// LCU — неофициальный локальный API клиента: порт и пароль лежат
// в lockfile, меняются при каждом запуске. Подробности и границы —
// project/CHAMP_SELECT.md.
//
// nlohmann живёт внутри .cpp, наружу — только доменные типы.

namespace sintence {

struct LcuCredentials {
    int port = 0;
    std::string password;  // никогда не печатается и не сохраняется
    std::string protocol;  // "https"
};

// lockfile: "LeagueClient:12345:54321:AbCdEf123:https"
//            имя процесса : pid : порт : пароль : протокол
// Любое отклонение от пяти полей или нечисловой порт — nullopt.
std::optional<LcuCredentials> ParseLockfile(std::string_view text);

// GET /lol-gameflow/v1/gameflow-phase отвечает JSON-строкой: "\"ChampSelect\"".
std::optional<std::string> ParseGameflowPhase(std::string_view json_text);

// GET /lol-summoner/v1/current-summoner -> puuid и Riot ID ("Имя#TAG").
struct LcuSummoner {
    std::string puuid;
    std::string riot_id;
};
std::optional<LcuSummoner> ParseCurrentSummoner(std::string_view json_text);

// GET /lol-champ-select/v1/session -> состав, баны, таймер.
//
// Правила:
//   - myTeam -> LobbySide::Ally, theirTeam -> LobbySide::Enemy;
//   - is_self — у того, чей cellId равен localPlayerCellId;
//   - nameVisibilityType == "HIDDEN" -> hidden = true, puuid и riot_id
//     пустые, даже если клиент их прислал: скрытое имя не раскрываем;
//   - у противников puuid и имя не берутся никогда — в выборе чемпиона
//     игрок их не видит;
//   - assignedPosition приходит строчными ("middle", "utility") и
//     переводится в роли пака ("MIDDLE", "UTILITY"); пусто — пусто;
//   - spell1Id/spell2Id вне разумного диапазона (клиент шлёт 2^64-1,
//     когда заклинание не выбрано) -> 0.
// Битый JSON или не объект — nullopt. Поле phase заполняет вызывающий.
std::optional<Lobby> ParseChampSelectSession(std::string_view json_text);

// GET /lol-gameflow/v1/session — сессия игры в клиенте. На экране загрузки
// и в матче в gameData.teamOne/teamTwo лежат все десять: puuid, имя,
// чемпион. Это тот же состав, что игрок видит на экране загрузки, —
// раньше и дешевле spectator-v5 (локально, без ключа и лимитов).
//
// Разбор намеренно терпимый: поля имени у Riot менялись (gameName+tagLine,
// summonerName), поэтому берётся то, что есть. Заклинания — из
// gameData.playerChampionSelections по puuid, если они там есть.
//
// Сторона — относительно self_puuid: его команда — Ally. Себя в составе
// нет — nullopt (сессия не про идущую игру). Нет gameData — nullopt.
std::optional<std::vector<LobbyMember>> ParseGameflowSession(std::string_view json_text,
                                                             std::string_view self_puuid);

}  // namespace sintence

#endif  // SINTENCE_DATA_LCU_JSON_H
