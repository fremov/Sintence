#ifndef SINTENCE_CORE_MATCH_STORE_H
#define SINTENCE_CORE_MATCH_STORE_H

#include <optional>
#include <string>
#include <string_view>
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

class MatchStore {
public:
    virtual ~MatchStore() = default;

    virtual bool HasMatch(const std::string& match_id) const = 0;

    // Сохранить матч и сырой ответ Riot. Повторное сохранение — замена.
    virtual bool SaveMatch(const MatchDetail& match, std::string_view raw_json) = 0;

    virtual std::optional<MatchDetail> LoadMatch(const std::string& match_id) const = 0;

    // Последние матчи игрока, новые первыми: offset, limit — для «ещё 20».
    // queue_id 0 — все очереди.
    virtual std::vector<MatchDetail> RecentMatches(const std::string& puuid, int offset,
                                                   int limit, int queue_id = 0) const = 0;

    // Сколько матчей игрока уже в хранилище.
    virtual int CountMatches(const std::string& puuid) const = 0;

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
