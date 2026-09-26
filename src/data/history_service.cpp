#include "history_service.h"

#include <algorithm>
#include <format>
#include <utility>
#include <vector>

#include "app_log.h"
#include "match_detail_json.h"
#include "riot_api_json.h"

namespace sintence {

namespace {

// Повторный заказ того же игрока не чаще раза в минуту: новых игр за это
// время почти не бывает, а интерфейс спрашивает каждые пару секунд.
constexpr auto kRefreshInterval = std::chrono::minutes(1);

// Больше Riot за раз не отдаёт.
constexpr int kIdsPerRequest = 100;

// Попыток на матч и на timeline. Таймаут у Riot — обычное дело (5 с на
// ответ, а матч весит сотню килобайт), и одна неудача не должна оставлять
// дыру в списке до следующего заказа.
constexpr int kMaxAttempts = 3;

// 404 — матча нет и не будет; повторять бессмысленно.
constexpr int kNotFound = 404;

}  // namespace

HistoryService::HistoryService(std::shared_ptr<RiotApiClient> client, MatchStore& store,
                               const ProfileService* profiles)
    : client_(std::move(client)), store_(store), profiles_(profiles) {
    thread_ = std::thread([this] { Worker(); });
}

HistoryService::~HistoryService() {
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    wake_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void HistoryService::Request(const std::string& riot_id, int depth, bool with_account) {
    if (riot_id.empty() || depth <= 0) {
        return;
    }
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        const auto now = std::chrono::steady_clock::now();
        auto& status = status_[riot_id];
        status.riot_id = riot_id;
        const auto last = refreshed_.find(riot_id);
        if (last != refreshed_.end() && depth <= status.depth &&
            now - last->second < kRefreshInterval) {
            return;
        }
        refreshed_[riot_id] = now;
        status.depth = std::max(status.depth, depth);
        status.running = true;
        player_queue_.push_back({riot_id, status.depth, with_account});
    }
    wake_.notify_one();
}

void HistoryService::RequestTimeline(const std::string& match_id, bool retry) {
    if (match_id.empty() || store_.HasTimeline(match_id)) {
        return;
    }
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (retry) {
            failed_timelines_.erase(match_id);
            attempts_.erase(match_id + "/timeline");
        }
        if (failed_timelines_.contains(match_id)) {
            return;
        }
        if (std::find(timeline_queue_.begin(), timeline_queue_.end(), match_id) !=
            timeline_queue_.end()) {
            return;
        }
        timeline_queue_.push_back(match_id);
    }
    wake_.notify_one();
}

bool HistoryService::TimelineFailed(const std::string& match_id) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return failed_timelines_.contains(match_id);
}

bool HistoryService::Retry(std::deque<std::string>& queue, const std::string& id) {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (++attempts_[id] >= kMaxAttempts) {
        attempts_.erase(id);
        return false;
    }
    queue.push_back(id.ends_with("/timeline") ? id.substr(0, id.size() - 9) : id);
    return true;
}

void HistoryService::PrefetchTimelines(const std::vector<std::string>& match_ids) {
    bool added = false;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        for (const std::string& id : match_ids) {
            if (prefetched_.insert(id).second) {
                prefetch_queue_.push_back(id);
                added = true;
            }
        }
    }
    if (added) {
        wake_.notify_one();
    }
}

std::optional<HistoryStatus> HistoryService::Status(const std::string& riot_id) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    const auto found = status_.find(riot_id);
    if (found == status_.end()) {
        return std::nullopt;
    }
    HistoryStatus status = found->second;
    if (!status.puuid.empty()) {
        status.stored = store_.CountMatches(status.puuid);
    }
    return status;
}

bool HistoryService::OverlayBusy() const {
    return profiles_ != nullptr && profiles_->Status().running;
}

void HistoryService::Worker() {
    for (;;) {
        std::string timeline;
        std::optional<PlayerJob> player;
        std::string match;
        std::string prefetch;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock, [this] {
                return stop_ || !timeline_queue_.empty() || !player_queue_.empty() ||
                       !match_queue_.empty() || !prefetch_queue_.empty();
            });
            if (stop_) {
                return;
            }
            if (!timeline_queue_.empty()) {
                timeline = std::move(timeline_queue_.front());
                timeline_queue_.pop_front();
            }
        }

        // Timeline ждёт открытая панель — он не уступает никому.
        if (!timeline.empty()) {
            RunTimelineJob(timeline);
            continue;
        }

        // Остальное уступает профилям лобби: табло идущей игры важнее.
        if (OverlayBusy()) {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait_for(lock, std::chrono::milliseconds(300), [this] { return stop_; });
            continue;
        }

        {
            const std::lock_guard<std::mutex> lock(mutex_);
            if (!player_queue_.empty()) {
                player = std::move(player_queue_.front());
                player_queue_.pop_front();
            } else if (!match_queue_.empty()) {
                match = std::move(match_queue_.front());
                match_queue_.pop_front();
            } else if (!prefetch_queue_.empty()) {
                prefetch = std::move(prefetch_queue_.front());
                prefetch_queue_.pop_front();
            }
        }
        if (player) {
            RunPlayerJob(*player);
        } else if (!match.empty()) {
            RunMatchJob(match);
        } else if (!prefetch.empty()) {
            RunPrefetchJob(prefetch);
        }

        // Очередь матчей опустела — загрузка всех игроков закончена.
        const std::lock_guard<std::mutex> lock(mutex_);
        if (player_queue_.empty() && match_queue_.empty()) {
            for (auto& [riot_id, status] : status_) {
                status.running = false;
            }
        }
    }
}

void HistoryService::RunPlayerJob(const PlayerJob& job) {
    Log("история: {} — последние {} игр", job.riot_id, job.depth);

    const auto puuid = client_->ResolvePuuid(job.riot_id);
    if (!puuid) {
        const std::lock_guard<std::mutex> lock(mutex_);
        auto& status = status_[job.riot_id];
        status.error = "игрок не найден";
        status.running = false;
        return;
    }

    // Иконка, уровень и ранги — только для окна профиля; у игроков лобби
    // их уже знает ProfileService, и два запроса на игрока — лишние.
    std::optional<SummonerInfo> summoner;
    std::optional<std::vector<RankedStats>> ranked;
    if (job.with_account) {
        summoner = client_->LoadSummoner(*puuid);
        ranked = client_->LoadRankedEntries(*puuid);
    }

    std::vector<std::string> ids;
    for (int start = 0; start < job.depth; start += kIdsPerRequest) {
        const int count = std::min(kIdsPerRequest, job.depth - start);
        const auto page = client_->LoadMatchIds(*puuid, start, count);
        if (!page && start == 0) {
            // Список игр не пришёл вовсе — это ошибка, а не «игр нет»:
            // прежние puuid, ранги и счётчики не затираем нулями.
            const int code = client_->LastStatus();
            LogError("история: {} — список игр не получен ({})", job.riot_id, code);
            const std::lock_guard<std::mutex> lock(mutex_);
            auto& status = status_[job.riot_id];
            status.error = std::format("Riot не отдал список игр (код {})", code);
            status.running = false;
            return;
        }
        if (!page) {
            break;
        }
        ids.insert(ids.end(), page->begin(), page->end());
        if (static_cast<int>(page->size()) < count) {
            break;  // игр меньше, чем заказано
        }
    }

    std::vector<std::string> missing;
    for (const std::string& id : ids) {
        if (!store_.HasMatch(id)) {
            missing.push_back(id);
        }
    }
    Log("история: {} — матчей {}, скачать {}", job.riot_id, ids.size(), missing.size());

    const std::lock_guard<std::mutex> lock(mutex_);
    auto& status = status_[job.riot_id];
    status.puuid = *puuid;
    status.error.clear();
    if (summoner) {
        status.summoner = *summoner;
    }
    if (ranked) {
        if (const RankedStats* solo = FindQueue(*ranked, "RANKED_SOLO_5x5")) {
            status.solo = *solo;
        }
        if (const RankedStats* flex = FindQueue(*ranked, "RANKED_FLEX_SR")) {
            status.flex = *flex;
        }
    }
    status.ids_known = static_cast<int>(ids.size());
    for (const std::string& id : missing) {
        if (std::find(match_queue_.begin(), match_queue_.end(), id) == match_queue_.end()) {
            match_queue_.push_back(id);
        }
    }
}

void HistoryService::RunMatchJob(const std::string& match_id) {
    if (store_.HasMatch(match_id)) {
        return;
    }
    const auto raw = client_->LoadMatchJson(match_id);
    if (!raw) {
        if (client_->LastStatus() != kNotFound && !Retry(match_queue_, match_id)) {
            LogError("история: {} не скачался за {} попытки", match_id, kMaxAttempts);
        }
        return;
    }
    const auto match = ParseMatchDetail(*raw);
    if (!match) {
        LogError("история: {} не разобрался", match_id);
        return;
    }
    store_.SaveMatch(*match, *raw);
}

void HistoryService::RunPrefetchJob(const std::string& match_id) {
    if (store_.HasTimeline(match_id)) {
        return;
    }
    // Без повторов и без отметки о провале: фоновая подкачка — не просьба
    // пользователя. Откроет подробности — там timeline закажется заново.
    if (const auto raw = client_->LoadTimelineJson(match_id)) {
        store_.SaveTimeline(match_id, *raw);
    }
}

void HistoryService::RunTimelineJob(const std::string& match_id) {
    if (store_.HasTimeline(match_id)) {
        return;
    }
    if (const auto raw = client_->LoadTimelineJson(match_id)) {
        store_.SaveTimeline(match_id, *raw);
        return;
    }
    if (client_->LastStatus() != kNotFound && Retry(timeline_queue_, match_id + "/timeline")) {
        return;
    }
    LogError("история: timeline {} не скачался", match_id);
    const std::lock_guard<std::mutex> lock(mutex_);
    failed_timelines_.insert(match_id);
}

}  // namespace sintence
