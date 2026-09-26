#ifndef SINTENCE_DATA_PROFILE_SERVICE_H
#define SINTENCE_DATA_PROFILE_SERVICE_H

#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "player_profile.h"
#include "riot_api_client.h"

// data/profile_service — фоновая докачка профилей лобби.
//
// Зачем отдельный класс: десять игроков — это тридцать запросов, а лимит
// personal-ключа (100 за 2 минуты) растягивает их примерно на сорок секунд.
// Ждать это в обработчике HTTP-запроса нельзя: интерфейс встанет колом.
// Поэтому запросы идут в фоновом потоке, а интерфейс каждый раз забирает
// то, что уже готово, и показывает прогресс.
//
// Повторные запросы бесплатны: игрок, чей профиль уже лежит в памяти,
// в очередь второй раз не попадает. Между двумя матчами подряд с теми же
// людьми не уходит ни одного обращения к Riot.

namespace sintence {

class ProfileService {
public:
    struct Progress {
        int done = 0;      // сколько профилей уже готово
        int total = 0;     // сколько всего заказано за сессию
        bool running = false;  // идёт ли докачка прямо сейчас
    };

    explicit ProfileService(RiotApiClient client);
    ~ProfileService();

    ProfileService(const ProfileService&) = delete;
    ProfileService& operator=(const ProfileService&) = delete;

    // Поставить в очередь тех, кого ещё нет. Возвращает управление сразу.
    void Request(const std::vector<std::string>& riot_ids);

    // Снимок готовых профилей. Порядок — тот, в котором они пришли.
    std::vector<PlayerProfile> Ready() const;

    Progress Status() const;

private:
    void Worker();

    RiotApiClient client_;  // трогает только фоновый поток

    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<std::string> queue_;
    std::vector<PlayerProfile> ready_;
    std::unordered_map<std::string, bool> known_;  // riot_id -> дошли ли руки
    int total_ = 0;
    bool running_ = false;
    bool stop_ = false;

    std::thread thread_;
};

}  // namespace sintence

#endif  // SINTENCE_DATA_PROFILE_SERVICE_H
