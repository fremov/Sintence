#include "sqlite_match_store.h"

#include <sqlite3.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "app_log.h"
#include "history_schema.h"  // kHistorySchema — встроен из project/data при сборке
#include "match_detail_json.h"  // миграция: переразбор сохранённых ответов Riot
#include "timeline_json.h"      // смерти до 10-й минуты при сохранении timeline

namespace sintence {

namespace {

// 2 — колонки для плашек стиля игры (damage_buildings, early_takedowns,
// solo_kills, lane_lead); старая база догоняется в MigrateToV2.
constexpr const char* kSchemaVersion = "2";

// Riot ID в виде для сравнения без учёта регистра: латиница и кириллица
// в нижний регистр, «ё» как «е» (в никах пишут и так и так). Остальные
// символы — как есть. Строка UTF-8; битая последовательность копируется
// побайтно, без попытки её исправить.
std::string FoldCase(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size();) {
        const auto byte = static_cast<unsigned char>(text[i]);
        if (byte < 0x80) {
            out.push_back(static_cast<char>(byte >= 'A' && byte <= 'Z' ? byte + 32 : byte));
            ++i;
            continue;
        }
        // Кириллица — двухбайтовая: 110xxxxx 10xxxxxx.
        if ((byte & 0xE0) == 0xC0 && i + 1 < text.size()) {
            char32_t code = ((byte & 0x1Fu) << 6) | (static_cast<unsigned char>(text[i + 1]) & 0x3Fu);
            if (code >= 0x0410 && code <= 0x042F) {
                code += 0x20;  // А-Я -> а-я
            } else if (code == 0x0401 || code == 0x0451) {
                code = 0x0435;  // Ё, ё -> е
            }
            out.push_back(static_cast<char>(0xC0 | (code >> 6)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            i += 2;
            continue;
        }
        out.push_back(static_cast<char>(byte));
        ++i;
    }
    return out;
}

long long NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

// Подготовленный запрос с автоматическим sqlite3_finalize.
class Statement {
public:
    Statement(sqlite3* db, const char* sql) {
        if (sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr) != SQLITE_OK) {
            LogError("история: не подготовлен запрос: {}", sqlite3_errmsg(db));
            stmt_ = nullptr;
        }
    }
    ~Statement() { sqlite3_finalize(stmt_); }
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    bool Ok() const { return stmt_ != nullptr; }

    Statement& Bind(int index, std::string_view text) {
        sqlite3_bind_text(stmt_, index, text.data(), static_cast<int>(text.size()),
                          SQLITE_TRANSIENT);
        return *this;
    }
    Statement& Bind(int index, long long value) {
        sqlite3_bind_int64(stmt_, index, value);
        return *this;
    }
    Statement& Bind(int index, int value) {
        sqlite3_bind_int(stmt_, index, value);
        return *this;
    }

    bool Step() { return stmt_ != nullptr && sqlite3_step(stmt_) == SQLITE_ROW; }
    bool Run() {
        if (stmt_ == nullptr) {
            return false;
        }
        const int code = sqlite3_step(stmt_);
        return code == SQLITE_DONE || code == SQLITE_ROW;
    }

    int Int(int column) const { return sqlite3_column_int(stmt_, column); }
    long long Int64(int column) const { return sqlite3_column_int64(stmt_, column); }
    std::string Text(int column) const {
        const auto* text = sqlite3_column_text(stmt_, column);
        return text ? std::string(reinterpret_cast<const char*>(text),
                                  static_cast<std::size_t>(sqlite3_column_bytes(stmt_, column)))
                    : std::string();
    }

private:
    sqlite3_stmt* stmt_ = nullptr;
};

bool Exec(sqlite3* db, const char* sql) {
    char* error = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &error) != SQLITE_OK) {
        LogError("история: {}", error ? error : "ошибка SQL");
        sqlite3_free(error);
        return false;
    }
    return true;
}

constexpr const char* kParticipantColumns =
    "puuid, riot_id, team_id, win, champion_id, champion, role, champ_level, kills, deaths, "
    "assists, cs, gold, damage_dealt, damage_taken, vision_score, wards_placed, wards_killed, "
    "item0, item1, item2, item3, item4, item5, item6, spell1, spell2, keystone, "
    "primary_style, sub_style, damage_buildings, early_takedowns, solo_kills, lane_lead";

// Версия 1 -> 2: четыре колонки для плашек стиля игры. CREATE TABLE IF NOT
// EXISTS существующую таблицу не меняет, поэтому колонки добавляются здесь
// и заполняются из сохранённого ответа Riot — без единого запроса к нему.
// ALTER TABLE ... ADD COLUMN ... DEFAULT есть и в MySQL: при переезде
// на сервер миграция та же.
bool MigrateToV2(sqlite3* db) {
    bool has_columns = false;
    {
        Statement columns(db, "PRAGMA table_info(participants)");
        while (columns.Step()) {
            has_columns = has_columns || columns.Text(1) == "damage_buildings";
        }
    }
    if (has_columns) {
        return true;
    }
    if (!Exec(db,
              "ALTER TABLE participants ADD COLUMN damage_buildings INTEGER NOT NULL DEFAULT 0;"
              "ALTER TABLE participants ADD COLUMN early_takedowns INTEGER NOT NULL DEFAULT 0;"
              "ALTER TABLE participants ADD COLUMN solo_kills INTEGER NOT NULL DEFAULT 0;"
              "ALTER TABLE participants ADD COLUMN lane_lead INTEGER NOT NULL DEFAULT -1;")) {
        return false;
    }

    std::vector<std::string> raws;
    {
        Statement rows(db, "SELECT raw_json FROM matches");
        while (rows.Step()) {
            raws.push_back(rows.Text(0));
        }
    }
    Exec(db, "BEGIN");
    int updated = 0;
    for (const std::string& raw : raws) {
        const auto match = ParseMatchDetail(raw);
        if (!match) {
            continue;
        }
        for (const MatchParticipant& p : match->participants) {
            Statement update(db,
                             "UPDATE participants SET damage_buildings = ?, early_takedowns = ?, "
                             "solo_kills = ?, lane_lead = ? WHERE match_id = ? AND puuid = ?");
            update.Bind(1, p.damage_to_buildings)
                .Bind(2, p.early_takedowns)
                .Bind(3, p.solo_kills)
                .Bind(4, p.lane_lead)
                .Bind(5, match->match_id)
                .Bind(6, p.puuid)
                .Run();
        }
        ++updated;
    }
    Exec(db, "COMMIT");
    Log("история: база обновлена до версии 2, матчей переразобрано {}", updated);
    return true;
}

// Смерти до 10-й минуты из разобранного timeline — в timeline_deaths.
// Разбор мегабайта JSON — в потоке загрузки, а не в обработчике HTTP.
void SaveEarlyDeaths(sqlite3* db, const std::string& match_id, const MatchTimeline& timeline) {
    for (const TimelineParticipant& p : timeline.participants) {
        const auto early = std::ranges::count_if(p.death_seconds, [](int second) { return second < 600; });
        Statement insert(db,
                         "INSERT OR REPLACE INTO timeline_deaths (match_id, puuid, deaths_before_10) "
                         "VALUES (?, ?, ?)");
        insert.Bind(1, match_id).Bind(2, p.puuid).Bind(3, static_cast<int>(early)).Run();
    }
}

// Timeline, скачанные до появления timeline_deaths: досчитать один раз.
void BackfillEarlyDeaths(sqlite3* db) {
    std::vector<std::string> missing;
    {
        Statement rows(db,
                       "SELECT match_id FROM timelines WHERE match_id NOT IN "
                       "(SELECT DISTINCT match_id FROM timeline_deaths)");
        while (rows.Step()) {
            missing.push_back(rows.Text(0));
        }
    }
    if (missing.empty()) {
        return;
    }
    Exec(db, "BEGIN");
    for (const std::string& match_id : missing) {
        Statement raw(db, "SELECT raw_json FROM timelines WHERE match_id = ?");
        if (raw.Bind(1, match_id).Step()) {
            if (const auto timeline = ParseMatchTimeline(raw.Text(0))) {
                SaveEarlyDeaths(db, match_id, *timeline);
            }
        }
    }
    Exec(db, "COMMIT");
    Log("история: смерти до 10-й минуты посчитаны по {} timeline", missing.size());
}

}  // namespace

struct SqliteMatchStore::Impl {
    sqlite3* db = nullptr;
    ~Impl() { sqlite3_close(db); }
};

SqliteMatchStore::SqliteMatchStore(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
SqliteMatchStore::~SqliteMatchStore() = default;

std::unique_ptr<SqliteMatchStore> SqliteMatchStore::Open(const std::string& path) {
    if (path != ":memory:") {
        std::error_code error;
        const auto parent = std::filesystem::path(path).parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent, error);
        }
    }

    auto impl = std::make_unique<Impl>();
    if (sqlite3_open(path.c_str(), &impl->db) != SQLITE_OK) {
        LogError("история: не открыть {}: {}", path,
                     impl->db ? sqlite3_errmsg(impl->db) : "нет памяти");
        return nullptr;
    }
    // WAL — чтобы чтение из HTTP не ждало записи загрузчика. Настройка
    // SQLite; у серверной базы будут свои.
    Exec(impl->db, "PRAGMA journal_mode = WAL; PRAGMA foreign_keys = ON;");
    if (!Exec(impl->db, kHistorySchema) || !MigrateToV2(impl->db)) {
        return nullptr;
    }
    BackfillEarlyDeaths(impl->db);
    {
        Statement version(impl->db,
                          "INSERT OR REPLACE INTO meta (meta_key, meta_value) VALUES ('schema_version', ?)");
        version.Bind(1, std::string_view(kSchemaVersion)).Run();
    }
    return std::unique_ptr<SqliteMatchStore>(new SqliteMatchStore(std::move(impl)));
}

bool SqliteMatchStore::HasMatch(const std::string& match_id) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    Statement query(impl_->db, "SELECT 1 FROM matches WHERE match_id = ?");
    return query.Bind(1, match_id).Step();
}

bool SqliteMatchStore::SaveMatch(const MatchDetail& match, std::string_view raw_json) {
    const std::lock_guard<std::mutex> lock(mutex_);
    sqlite3* db = impl_->db;
    if (!Exec(db, "BEGIN")) {
        return false;
    }

    bool ok = true;
    {
        Statement insert(db,
                         "INSERT OR REPLACE INTO matches (match_id, platform, queue_id, game_mode, "
                         "game_version, patch, started_at, duration_s, fetched_at, raw_json) "
                         "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
        ok = insert.Bind(1, match.match_id)
                 .Bind(2, match.platform)
                 .Bind(3, match.queue_id)
                 .Bind(4, match.game_mode)
                 .Bind(5, match.game_version)
                 .Bind(6, match.patch)
                 .Bind(7, match.started_at_ms)
                 .Bind(8, match.duration_seconds)
                 .Bind(9, NowMs())
                 .Bind(10, raw_json)
                 .Run();
    }
    if (ok) {
        Statement clear(db, "DELETE FROM participants WHERE match_id = ?");
        ok = clear.Bind(1, match.match_id).Run();
    }
    for (const MatchParticipant& p : match.participants) {
        if (!ok) {
            break;
        }
        Statement insert(db,
                         "INSERT INTO participants (match_id, puuid, riot_id, team_id, win, "
                         "champion_id, champion, role, champ_level, kills, deaths, assists, cs, "
                         "gold, damage_dealt, damage_taken, vision_score, wards_placed, "
                         "wards_killed, item0, item1, item2, item3, item4, item5, item6, spell1, "
                         "spell2, keystone, primary_style, sub_style, damage_buildings, "
                         "early_takedowns, solo_kills, lane_lead) VALUES (?, ?, ?, ?, ?, ?, ?, "
                         "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
                         "?, ?, ?, ?)");
        insert.Bind(1, match.match_id)
            .Bind(2, p.puuid)
            .Bind(3, p.riot_id)
            .Bind(4, p.team_id)
            .Bind(5, p.win ? 1 : 0)
            .Bind(6, p.champion_id)
            .Bind(7, p.champion)
            .Bind(8, p.role)
            .Bind(9, p.champ_level)
            .Bind(10, p.kills)
            .Bind(11, p.deaths)
            .Bind(12, p.assists)
            .Bind(13, p.cs)
            .Bind(14, p.gold)
            .Bind(15, p.damage_dealt)
            .Bind(16, p.damage_taken)
            .Bind(17, p.vision_score)
            .Bind(18, p.wards_placed)
            .Bind(19, p.wards_killed);
        for (int slot = 0; slot < 7; ++slot) {
            insert.Bind(20 + slot, p.items[static_cast<std::size_t>(slot)]);
        }
        ok = insert.Bind(27, p.spell1)
                 .Bind(28, p.spell2)
                 .Bind(29, p.keystone)
                 .Bind(30, p.primary_style)
                 .Bind(31, p.sub_style)
                 .Bind(32, p.damage_to_buildings)
                 .Bind(33, p.early_takedowns)
                 .Bind(34, p.solo_kills)
                 .Bind(35, p.lane_lead)
                 .Run();
    }

    Exec(db, ok ? "COMMIT" : "ROLLBACK");
    return ok;
}

std::optional<MatchDetail> SqliteMatchStore::LoadMatchLocked(const std::string& match_id) const {
    Statement head(impl_->db,
                   "SELECT match_id, platform, queue_id, game_mode, game_version, patch, "
                   "started_at, duration_s FROM matches WHERE match_id = ?");
    if (!head.Bind(1, match_id).Step()) {
        return std::nullopt;
    }
    MatchDetail match;
    match.match_id = head.Text(0);
    match.platform = head.Text(1);
    match.queue_id = head.Int(2);
    match.game_mode = head.Text(3);
    match.game_version = head.Text(4);
    match.patch = head.Text(5);
    match.started_at_ms = head.Int64(6);
    match.duration_seconds = head.Int(7);

    const std::string sql = std::string("SELECT ") + kParticipantColumns +
                            " FROM participants WHERE match_id = ? ORDER BY team_id, rowid";
    Statement rows(impl_->db, sql.c_str());
    rows.Bind(1, match_id);
    while (rows.Step()) {
        MatchParticipant p;
        p.puuid = rows.Text(0);
        p.riot_id = rows.Text(1);
        p.team_id = rows.Int(2);
        p.win = rows.Int(3) != 0;
        p.champion_id = rows.Int(4);
        p.champion = rows.Text(5);
        p.role = rows.Text(6);
        p.champ_level = rows.Int(7);
        p.kills = rows.Int(8);
        p.deaths = rows.Int(9);
        p.assists = rows.Int(10);
        p.cs = rows.Int(11);
        p.gold = rows.Int(12);
        p.damage_dealt = rows.Int(13);
        p.damage_taken = rows.Int(14);
        p.vision_score = rows.Int(15);
        p.wards_placed = rows.Int(16);
        p.wards_killed = rows.Int(17);
        for (int slot = 0; slot < 7; ++slot) {
            p.items[static_cast<std::size_t>(slot)] = rows.Int(18 + slot);
        }
        p.spell1 = rows.Int(25);
        p.spell2 = rows.Int(26);
        p.keystone = rows.Int(27);
        p.primary_style = rows.Int(28);
        p.sub_style = rows.Int(29);
        p.damage_to_buildings = rows.Int(30);
        p.early_takedowns = rows.Int(31);
        p.solo_kills = rows.Int(32);
        p.lane_lead = rows.Int(33);
        match.participants.push_back(std::move(p));
    }
    return match;
}

std::optional<MatchDetail> SqliteMatchStore::LoadMatch(const std::string& match_id) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return LoadMatchLocked(match_id);
}

std::vector<MatchDetail> SqliteMatchStore::RecentMatches(const std::string& puuid, int offset,
                                                         int limit,
                                                         const MatchFilter& filter) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> ids;
    {
        std::string sql =
            "SELECT m.match_id FROM matches m JOIN participants p "
            "ON p.match_id = m.match_id WHERE p.puuid = ?";
        if (filter.queue_id != 0) {
            sql += " AND m.queue_id = ?";
        }
        if (filter.champion_id != 0) {
            sql += " AND p.champion_id = ?";
        }
        sql += " ORDER BY m.started_at DESC LIMIT ? OFFSET ?";
        Statement query(impl_->db, sql.c_str());
        int index = 1;
        query.Bind(index++, puuid);
        if (filter.queue_id != 0) {
            query.Bind(index++, filter.queue_id);
        }
        if (filter.champion_id != 0) {
            query.Bind(index++, filter.champion_id);
        }
        query.Bind(index++, limit).Bind(index, offset);
        while (query.Step()) {
            ids.push_back(query.Text(0));
        }
    }
    std::vector<MatchDetail> matches;
    matches.reserve(ids.size());
    for (const std::string& id : ids) {
        if (auto match = LoadMatchLocked(id)) {
            matches.push_back(std::move(*match));
        }
    }
    return matches;
}

std::vector<PlayerSuggestion> SqliteMatchStore::SearchPlayers(std::string_view query,
                                                              int limit) const {
    const std::string needle = FoldCase(query);
    if (needle.empty() || limit <= 0) {
        return {};
    }
    struct Candidate {
        PlayerSuggestion player;
        bool prefix = false;
    };
    std::vector<Candidate> found;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        // Кириллицу SQLite в LIKE без учёта регистра не сравнивает (только
        // ASCII), поэтому отбор — здесь. Строк — по одной на игрока, их
        // тысячи, а не миллионы: это игроки из просмотренных историй.
        Statement rows(impl_->db,
                       "SELECT riot_id, COUNT(*) FROM participants WHERE riot_id <> '' "
                       "GROUP BY riot_id");
        while (rows.Step()) {
            const std::string riot_id = rows.Text(0);
            const std::string folded = FoldCase(riot_id);
            const std::size_t at = folded.find(needle);
            if (at != std::string::npos) {
                found.push_back({{riot_id, rows.Int(1)}, at == 0});
            }
        }
    }
    std::ranges::sort(found, [](const Candidate& a, const Candidate& b) {
        if (a.prefix != b.prefix) {
            return a.prefix;
        }
        if (a.player.games != b.player.games) {
            return a.player.games > b.player.games;
        }
        return a.player.riot_id < b.player.riot_id;
    });
    std::vector<PlayerSuggestion> result;
    for (const Candidate& candidate : found) {
        if (static_cast<int>(result.size()) >= limit) {
            break;
        }
        result.push_back(candidate.player);
    }
    return result;
}

int SqliteMatchStore::CountMatches(const std::string& puuid) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    Statement query(impl_->db, "SELECT COUNT(*) FROM participants WHERE puuid = ?");
    return query.Bind(1, puuid).Step() ? query.Int(0) : 0;
}

std::unordered_map<std::string, int> SqliteMatchStore::EarlyDeaths(const std::string& puuid) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    std::unordered_map<std::string, int> result;
    Statement rows(impl_->db, "SELECT match_id, deaths_before_10 FROM timeline_deaths WHERE puuid = ?");
    rows.Bind(1, puuid);
    while (rows.Step()) {
        result[rows.Text(0)] = rows.Int(1);
    }
    return result;
}

bool SqliteMatchStore::HasTimeline(const std::string& match_id) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    Statement query(impl_->db, "SELECT 1 FROM timelines WHERE match_id = ?");
    return query.Bind(1, match_id).Step();
}

std::optional<std::string> SqliteMatchStore::LoadTimeline(const std::string& match_id) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    Statement query(impl_->db, "SELECT raw_json FROM timelines WHERE match_id = ?");
    if (!query.Bind(1, match_id).Step()) {
        return std::nullopt;
    }
    return query.Text(0);
}

bool SqliteMatchStore::SaveTimeline(const std::string& match_id, std::string_view raw_json) {
    // Разбор — до блокировки: читатели не ждут, пока разбирается мегабайт.
    const auto timeline = ParseMatchTimeline(raw_json);
    const std::lock_guard<std::mutex> lock(mutex_);
    Exec(impl_->db, "BEGIN");
    bool ok = false;
    {
        Statement insert(impl_->db,
                         "INSERT OR REPLACE INTO timelines (match_id, fetched_at, raw_json) "
                         "VALUES (?, ?, ?)");
        ok = insert.Bind(1, match_id).Bind(2, NowMs()).Bind(3, raw_json).Run();
    }
    if (ok && timeline) {
        SaveEarlyDeaths(impl_->db, match_id, *timeline);
    }
    Exec(impl_->db, ok ? "COMMIT" : "ROLLBACK");
    return ok;
}

std::optional<std::string> SqliteMatchStore::LoadRawMatch(const std::string& match_id) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    Statement query(impl_->db, "SELECT raw_json FROM matches WHERE match_id = ?");
    if (!query.Bind(1, match_id).Step()) {
        return std::nullopt;
    }
    return query.Text(0);
}

}  // namespace sintence
