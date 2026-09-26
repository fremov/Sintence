#ifndef SINTENCE_DATA_PROFILE_SERVICE_H
#define SINTENCE_DATA_PROFILE_SERVICE_H

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <optional>

#include "lobby.h"
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

    // Клиент общий с окном профиля (HistoryService): один ограничитель
    // запросов на ключ.
    explicit ProfileService(std::shared_ptr<RiotApiClient> client);
    ~ProfileService();

    ProfileService(const ProfileService&) = delete;
    ProfileService& operator=(const ProfileService&) = delete;

    // Поставить в очередь тех, кого ещё нет. Возвращает управление сразу.
    void Request(const std::vector<std::string>& riot_ids);

    // Узнать состав идущей игры через spectator-v5 и заказать профили
    // всех участников. Выполняется тем же фоновым потоком, что и профили:
    // лимит ключа у них общий. Повторный вызов с тем же Riot ID, пока
    // предыдущий не выполнен, ничего не добавляет.
    //
    // Именно Riot ID, а не puuid из клиента League: клиент отдаёт puuid
    // в виде UUID, а Riot API ждёт свой, зашифрованный под ключ, и на
    // UUID отвечает 400. Свой puuid поток узнаёт через account-v1.
    void RequestActiveGame(const std::string& self_riot_id);

    // Результат последнего RequestActiveGame. nullopt — ещё не выполнен
    // или игры нет (Practice Tool, 404).
    struct ActiveGame {
        bool attempted = false;  // запрос выполнен (успешно или нет)
        int status = 0;          // HTTP-статус ответа spectator-v5
        std::optional<std::vector<LobbyMember>> members;
    };
    ActiveGame LastActiveGame() const;

    // Забыть результат spectator-v5: игра кончилась, следующая — другая.
    void ResetActiveGame();

    // Снимок готовых профилей. Порядок — тот, в котором они пришли.
    std::vector<PlayerProfile> Ready() const;

    Progress Status() const;

private:
    void Worker();

    std::shared_ptr<RiotApiClient> client_;

    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<std::string> queue_;
    std::string active_game_request_;  // Riot ID, ждущий spectator-v5
    ActiveGame active_game_;
    std::vector<PlayerProfile> ready_;
    std::unordered_map<std::string, bool> known_;  // riot_id -> дошли ли руки
    int total_ = 0;
    bool running_ = false;
    bool stop_ = false;

    std::thread thread_;
};

}  // namespace sintence

#endif  // SINTENCE_DATA_PROFILE_SERVICE_H
