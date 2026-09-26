#ifndef SINTENCE_CORE_MATCH_STORE_H
#define SINTENCE_CORE_MATCH_STORE_H

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "match_detail.h"

// core/match_store — где живёт история матчей игроков.
//
// Сейчас это SQLite на машине разработчика (data/sqlite_match_store),
// потом — база на сервере, и на место адаптера встанет клиент его API.
// Окно профиля и анализ знают только этот интерфейс: смена хранилища
// их не касается (тот же приём, что у MatchSource и LiveGameSource).
//
// Все методы потокобезопасны: пишет фоновый поток загрузки, читают
// обработчики HTTP.

namespace sintence {

// Какие матчи игрока нужны. Ноль в поле — без ограничения.
struct MatchFilter {
    int queue_id = 0;     // 420 одиночная, 440 гибкая, 450 ARAM...
    int champion_id = 0;  // только игры игрока на этом чемпионе
};

// Игрок, встреченный в сохранённых матчах, — подсказка при вводе ника.
struct PlayerSuggestion {
    std::string riot_id;  // "Имя#TAG" — как в последнем матче с ним
    int games = 0;        // в скольких сохранённых матчах встречался
};

class MatchStore {
public:
    virtual ~MatchStore() = default;

    virtual bool HasMatch(const std::string& match_id) const = 0;

    // Сохранить матч и сырой ответ Riot. Повторное сохранение — замена.
    virtual bool SaveMatch(const MatchDetail& match, std::string_view raw_json) = 0;

    virtual std::optional<MatchDetail> LoadMatch(const std::string& match_id) const = 0;

    // Последние матчи игрока, новые первыми: offset, limit — для «ещё 20».
    virtual std::vector<MatchDetail> RecentMatches(const std::string& puuid, int offset,
                                                   int limit,
                                                   const MatchFilter& filter = {}) const = 0;

    // Игроки из сохранённых матчей, чей Riot ID содержит query (без учёта
    // регистра, кириллица тоже). Сначала те, у кого с query начинается имя,
    // внутри — кто чаще встречался. Riot поиска по части ника не даёт:
    // account-v1 знает только точное Имя#TAG, поэтому подсказки — из своих
    // данных. Пустой query — пустой ответ.
    virtual std::vector<PlayerSuggestion> SearchPlayers(std::string_view query,
                                                        int limit) const = 0;

    // Сколько матчей игрока уже в хранилище.
    virtual int CountMatches(const std::string& puuid) const = 0;

    // Смерти игрока до 10-й минуты по матчам, где timeline уже скачан:
    // match_id -> число. Считаются при сохранении timeline.
    virtual std::unordered_map<std::string, int> EarlyDeaths(const std::string& puuid) const = 0;

    // Есть ли timeline, не читая его: сам он весит около мегабайта.
    virtual bool HasTimeline(const std::string& match_id) const = 0;

    // Timeline матча — сырой ответ Riot; нет — nullopt.
    virtual std::optional<std::string> LoadTimeline(const std::string& match_id) const = 0;
    virtual bool SaveTimeline(const std::string& match_id, std::string_view raw_json) = 0;

    MatchStore(const MatchStore&) = delete;
    MatchStore& operator=(const MatchStore&) = delete;

protected:
    MatchStore() = default;
};

}  // namespace sintence

#endif  // SINTENCE_CORE_MATCH_STORE_H
