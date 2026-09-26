#ifndef SINTENCE_DATA_SQLITE_MATCH_STORE_H
#define SINTENCE_DATA_SQLITE_MATCH_STORE_H

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "match_store.h"  // интерфейс MatchStore из core/

// data/sqlite_match_store — история матчей в SQLite.
//
// Временное хранилище до сервера: схема (project/data/history_schema.sql)
// написана на общем подмножестве SQL и переедет в MySQL как есть. Всё, что
// специфично для SQLite (INSERT OR REPLACE, PRAGMA), живёт только в этом
// файле: серверный адаптер будет писать то же самое на своём диалекте.
//
// Одно соединение под мьютексом: пишет фоновый поток загрузки, читают
// обработчики HTTP, и нагрузки, ради которой стоило бы держать пул, нет.
// sqlite3.h наружу не выходит — соединение спрятано в Impl.

namespace sintence {

class SqliteMatchStore : public MatchStore {
public:
    // Открывает (или создаёт) базу и применяет схему. Не открылась —
    // nullopt с причиной в журнале; путь ":memory:" — база в памяти (тесты).
    static std::unique_ptr<SqliteMatchStore> Open(const std::string& path);
    ~SqliteMatchStore() override;

    bool HasMatch(const std::string& match_id) const override;
    bool SaveMatch(const MatchDetail& match, std::string_view raw_json) override;
    std::optional<MatchDetail> LoadMatch(const std::string& match_id) const override;
    std::vector<MatchDetail> RecentMatches(const std::string& puuid, int offset, int limit,
                                           int queue_id = 0) const override;
    int CountMatches(const std::string& puuid) const override;
    std::optional<std::string> LoadTimeline(const std::string& match_id) const override;
    bool SaveTimeline(const std::string& match_id, std::string_view raw_json) override;

    // Сырой ответ Riot по матчу — для переноса на сервер и отладки.
    std::optional<std::string> LoadRawMatch(const std::string& match_id) const;

private:
    struct Impl;
    explicit SqliteMatchStore(std::unique_ptr<Impl> impl);

    std::optional<MatchDetail> LoadMatchLocked(const std::string& match_id) const;

    mutable std::mutex mutex_;
    std::unique_ptr<Impl> impl_;
};

}  // namespace sintence

#endif  // SINTENCE_DATA_SQLITE_MATCH_STORE_H
