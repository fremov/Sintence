#ifndef SINTENCE_DATA_HISTORY_SERVICE_H
#define SINTENCE_DATA_HISTORY_SERVICE_H

#include <chrono>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include "match_history.h"
#include "match_store.h"
#include "profile_service.h"
#include "riot_api_client.h"

// data/history_service — история матчей игрока для окна профиля, в фоне.
//
// Заказ «50 последних игр Имя#TAG» превращается в очередь работ:
//   1. puuid, иконка, уровень, ранги (одиночная и гибкая) — 3-4 запроса;
//   2. id последних матчей — 1 запрос на каждые 100;
//   3. сами матчи, которых ещё нет в хранилище, — по запросу на матч,
//      новые первыми.
// Интерфейс раз в пару секунд спрашивает состояние и забирает то, что уже
// в хранилище: строки списка заполняются сверху вниз по мере загрузки.
//
// Приоритеты. Timeline — сразу: его ждёт открытая панель подробностей.
// Всё остальное уступает профилям лобби (ProfileService): пока там есть
// работа, история ждёт, иначе загрузка 50 матчей отняла бы лимит ключа
// у табло идущей игры. Клиент Riot общий — лимит считается один.

namespace sintence {

class HistoryService {
public:
    // store и profiles должны жить дольше службы; profiles может быть nullptr.
    HistoryService(std::shared_ptr<RiotApiClient> client, MatchStore& store,
                   const ProfileService* profiles);
    ~HistoryService();

    HistoryService(const HistoryService&) = delete;
    HistoryService& operator=(const HistoryService&) = delete;

    // Заказать depth последних игр игрока. Возвращает сразу. Повторный
    // заказ того же или меньшего размера чаще раза в минуту ничего не делает:
    // интерфейс зовёт это при каждом опросе.
    void Request(const std::string& riot_id, int depth);

    // Заказать timeline матча (подробности). Уже в хранилище или уже
    // провалился — ничего; retry = true снимает отметку о провале.
    void RequestTimeline(const std::string& match_id, bool retry = false);

    // Timeline не скачался за kMaxAttempts попыток — панели показать ошибку.
    bool TimelineFailed(const std::string& match_id) const;

    // Состояние загрузки игрока; nullopt — его ещё не заказывали.
    std::optional<HistoryStatus> Status(const std::string& riot_id) const;

private:
    struct PlayerJob {
        std::string riot_id;
        int depth = 0;
    };

    void Worker();
    void RunPlayerJob(const PlayerJob& job);
    void RunMatchJob(const std::string& match_id);
    void RunTimelineJob(const std::string& match_id);
    bool OverlayBusy() const;

    // Сбой сети или 429 после повтора: вернуть работу в конец очереди,
    // пока попыток меньше kMaxAttempts. true — ещё будет попытка.
    bool Retry(std::deque<std::string>& queue, const std::string& id);

    std::shared_ptr<RiotApiClient> client_;
    MatchStore& store_;
    const ProfileService* profiles_;

    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<std::string> timeline_queue_;
    std::deque<PlayerJob> player_queue_;
    std::deque<std::string> match_queue_;
    std::unordered_map<std::string, HistoryStatus> status_;  // riot_id -> состояние
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> refreshed_;
    std::unordered_map<std::string, int> attempts_;  // id матча или timeline -> неудач
    std::unordered_set<std::string> failed_timelines_;
    bool stop_ = false;
    std::thread thread_;
};

}  // namespace sintence

#endif  // SINTENCE_DATA_HISTORY_SERVICE_H
