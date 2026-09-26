#ifndef SINTENCE_DATA_RIOT_API_JSON_H
#define SINTENCE_DATA_RIOT_API_JSON_H

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "lobby.h"           // LobbyMember из core/
#include "player_profile.h"  // RankedStats, ChampionMastery из core/

// data/riot_api_json — разбор ответов Riot API в типы core/.
//
// Здесь только чистые функции: строка на входе, доменные типы на выходе.
// Ни сети, ни ключа, ни файлов — поэтому всё тестируется фикстурами,
// и при отладке живого запроса всегда понятно, где сломалось:
// в транспорте (riot_api_client) или в разборе (здесь).
//
// nlohmann живёт внутри .cpp, наружу не выходит (ARCHITECTURE.md §4).
//
// Общее правило на все функции файла: битый JSON или неожиданная форма
// документа — nullopt, никаких исключений. Ответ Riot приходит из сети,
// и «сеть вернула мусор» — это рабочий режим, а не авария.
//
// КРИТЕРИИ ГОТОВНОСТИ (задача 1 из трёх):
//   1. Все тесты test_riot_api_json.cpp зелёные.
//   2. Ни одного #include "json.hpp" за пределами .cpp.
//   3. Отсутствие ранга и отсутствие мастери не превращаются в ошибку:
//      пустой массив — это пустой vector, а не nullopt.
//   4. Riot ID с пробелом в имени разбирается правильно.

namespace sintence {

// Разбирает ответ GET /riot/account/v1/accounts/by-riot-id/{name}/{tag}.
//
// Форма ответа:
//   {"puuid": "abc...", "gameName": "Riot Tuxedo", "tagLine": "TXC1"}
//
// Нужен только puuid: он ключ ко всем остальным запросам.
// Нет поля или пустая строка — nullopt.
std::optional<std::string> ParsePuuid(std::string_view json_text);

// Разбирает ответ GET /lol/league/v4/entries/by-puuid/{puuid}.
//
// Ответ — МАССИВ записей, по одной на очередь (соло, флекс, иногда
// пустой). Незаполненная калибровка или новый аккаунт дают [].
//
// Контракт:
//   - [] -> пустой вектор (не nullopt: «рангов нет» — это ответ, а не сбой);
//   - запись без tier или без rank пропускается, остальные разбираются;
//   - не массив или битый JSON -> nullopt.
//
// Соответствие полей (LeagueEntryDTO, проверено по актуальной схеме):
//   queueType    -> queue        ("RANKED_SOLO_5x5")
//   tier         -> tier         ("EMERALD")
//   rank         -> division     ("II")   <- внимание: НЕ "division"
//   leaguePoints -> league_points
//   wins         -> wins
//   losses       -> losses
//
// Поля summonerId в ответе больше нет — Riot убрал его в 2025 году,
// вместо него приходит puuid. Опираться на summonerId нельзя.
std::optional<std::vector<RankedStats>> ParseRankedEntries(std::string_view json_text);

// Ищет запись нужной очереди среди разобранных.
//
// nullptr — очередь не найдена, и это нормальный ответ: человек мог
// играть только флекс. Возврат указателя, а не копии, потому что
// вызывающий обычно смотрит одно-два поля и копировать строки незачем.
//
// Указатель действителен, пока жив вектор.
const RankedStats* FindQueue(const std::vector<RankedStats>& entries,
                             std::string_view queue);

// Разбирает ответ GET /lol/champion-mastery/v4/champion-masteries/
//                     by-puuid/{puuid}/top?count=3
//
// Ответ — массив, уже отсортированный Riot по убыванию очков.
// Порядок СОХРАНЯЕТСЯ: пересортировывать не нужно и нельзя.
//
// Контракт:
//   - [] -> пустой вектор (аккаунт без единой игры);
//   - запись без championId пропускается;
//   - не массив или битый JSON -> nullopt.
//
// Соответствие полей (ChampionMasteryDTO):
//   championId     -> champion_id
//   championLevel  -> level
//   championPoints -> points
//   lastPlayTime   -> last_play_time_ms   (миллисекунды, не секунды!)
//
// Внимание на тип: championPoints и lastPlayTime в int не влезают.
// lastPlayTime — это порядка 1.7e12, int переполнится молча.
std::optional<std::vector<ChampionMastery>> ParseChampionMasteries(
    std::string_view json_text);

// Разбивает "Riot Tuxedo#TXC1" на имя и тег.
//
// Нужно для account-v1: имя и тег идут двумя разными сегментами пути.
//
// Контракт:
//   - "Имя#TAG" -> пара {"Имя", "TAG"};
//   - нет '#', пустое имя или пустой тег -> nullopt;
//   - имя может содержать пробелы ("Riot Tuxedo") — это допустимо;
//   - тег '#' содержать не может, поэтому разделитель ищется ПЕРВЫЙ.
std::optional<std::pair<std::string, std::string>> SplitRiotId(
    std::string_view riot_id);

// Разбирает ответ GET /riot/account/v1/region/by-game/lol/by-puuid/{puuid}.
//
// Форма ответа:
//   {"puuid": "...", "game": "lol", "region": "EUW1"}
//
// Нужен, чтобы не зашивать регион в код: league-v4 и champion-mastery-v4
// живут на платформенном хосте, и запрос к ЧУЖОЙ платформе отвечает
// пустым массивом или 404 — то есть выглядит как «ранга нет», а не как
// ошибка. Регион возвращается как есть: "EUW1", "RU", "EUN1".
std::optional<std::string> ParseActiveRegion(std::string_view json_text);

// Платформенный идентификатор -> хост API: "EUW1" -> "euw1.api.riotgames.com".
// Правило у Riot буквальное: идентификатор в нижнем регистре плюс домен.
//
// Пустая строка на входе — nullopt: иначе получился бы хост
// ".api.riotgames.com", и разбираться пришлось бы уже с ошибкой DNS.
std::optional<std::string> PlatformHost(std::string_view platform_id);

// Разбирает ответ GET /lol/spectator/v5/active-games/by-summoner/{puuid} —
// состав идущей игры. Отвечает уже на экране загрузки, раньше Live Client.
//
// Форма ответа (урезанно):
//   {"gameId": 123, "participants": [
//     {"puuid": "...", "riotId": "Имя#TAG", "championId": 103,
//      "teamId": 100, "spell1Id": 4, "spell2Id": 14, "perks": {...}}, ...]}
//
// Контракт:
//   - сторона считается относительно self_puuid: его teamId — Ally;
//     self_puuid нет среди участников — nullopt (это не наша игра);
//   - perks не читаются: малые руны противника игрок в матче не видит,
//     и Sintence их не показывает (ARCHITECTURE.md §9);
//   - участник без puuid и без riotId пропускается (бот).
std::optional<std::vector<LobbyMember>> ParseActiveGame(std::string_view json_text,
                                                        std::string_view self_puuid);

}  // namespace sintence

#endif  // SINTENCE_DATA_RIOT_API_JSON_H
