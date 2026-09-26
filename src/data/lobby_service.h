#ifndef SINTENCE_DATA_LOBBY_SERVICE_H
#define SINTENCE_DATA_LOBBY_SERVICE_H

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "lcu_client.h"
#include "lobby.h"
#include "profile_service.h"

// data/lobby_service — состав игры до начала матча, в фоне.
//
// Раз в полторы секунды спрашивает клиент League, на каком он этапе:
//   ChampSelect         -> сессия выбора чемпиона из LCU: союзники, пики,
//                          баны, назначенная роль; профили видимых союзников
//                          заказываются сразу;
//   GameStart/InProgress -> один запрос spectator-v5 по своему puuid: все
//                          десять участников и их профили — ещё на экране
//                          загрузки, до того как ответит Live Client;
//   остальное           -> состава нет.
//
// Скрытые клиентом имена не восстанавливаются (project/CHAMP_SELECT.md).
// К LCU — только GET.

namespace sintence {

class LobbyService {
public:
    // profiles — служба профилей или nullptr (нет ключа Riot): тогда лобби
    // показывается без профилей и без spectator-v5. Должна жить дольше.
    explicit LobbyService(ProfileService* profiles,
                          std::chrono::milliseconds interval = std::chrono::milliseconds(1500));
    ~LobbyService();

    LobbyService(const LobbyService&) = delete;
    LobbyService& operator=(const LobbyService&) = delete;

    // Последний снимок. phase пустая — клиент League не найден.
    Lobby Snapshot() const;

private:
    void Worker();
    void Tick();
    void TickChampSelect(Lobby& next);
    void TickInGame(Lobby& next);

    LcuClient lcu_;                   // трогает только фоновый поток
    ProfileService* profiles_;
    std::chrono::milliseconds interval_;

    // Состояние фонового потока.
    std::optional<LcuSummoner> self_;
    std::string last_phase_;
    std::chrono::steady_clock::time_point last_spectate_{};
    int spectate_attempts_ = 0;

    mutable std::mutex mutex_;
    std::condition_variable wake_;
    Lobby lobby_;
    bool stop_ = false;
    std::thread thread_;
};

}  // namespace sintence

#endif  // SINTENCE_DATA_LOBBY_SERVICE_H
