#include "lobby_service.h"

#include <format>
#include <print>
#include <string>
#include <utility>
#include <vector>

namespace sintence {

namespace {

// spectator-v5 узнаёт игру не сразу: в первые секунды экрана загрузки
// бывает 404. Повторяем раз в десять секунд, но не бесконечно —
// Practice Tool и пользовательские игры не наблюдаемы никогда.
constexpr auto kSpectateRetry = std::chrono::seconds(10);
// Пять минут: экран загрузки у медленного компьютера длится дольше минуты.
constexpr int kSpectateAttempts = 30;

bool InGamePhase(const std::string& phase) {
    return phase == "GameStart" || phase == "InProgress" || phase == "Reconnect";
}

}  // namespace

LobbyService::LobbyService(ProfileService* profiles, std::chrono::milliseconds interval)
    : profiles_(profiles), interval_(interval) {
    thread_ = std::thread([this] { Worker(); });
}

LobbyService::~LobbyService() {
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    wake_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
}

Lobby LobbyService::Snapshot() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return lobby_;
}

void LobbyService::Worker() {
    for (;;) {
        Tick();
        std::unique_lock<std::mutex> lock(mutex_);
        if (wake_.wait_for(lock, interval_, [this] { return stop_; })) {
            return;
        }
    }
}

void LobbyService::Tick() {
    Lobby next;
    const auto phase_body = lcu_.Get("/lol-gameflow/v1/gameflow-phase");
    const auto phase = phase_body ? ParseGameflowPhase(*phase_body) : std::nullopt;

    if (!phase) {
        // Клиент закрыт. Свой аккаунт мог смениться — забываем.
        if (!last_phase_.empty()) {
            std::println("лобби: клиент League не найден");
        }
        self_.reset();
        last_phase_.clear();
        const std::lock_guard<std::mutex> lock(mutex_);
        lobby_ = Lobby{};
        return;
    }

    next.phase = *phase;
    if (next.phase != last_phase_) {
        std::println("лобби: этап {} -> {}", last_phase_.empty() ? "-" : last_phase_,
                     next.phase);
        // Новый выбор чемпиона или возврат в лобби — прошлая игра кончилась.
        if (!InGamePhase(next.phase) && profiles_ != nullptr) {
            profiles_->ResetActiveGame();
            spectate_attempts_ = 0;
        }
        last_phase_ = next.phase;
    }

    if (!self_) {
        if (const auto body = lcu_.Get("/lol-summoner/v1/current-summoner")) {
            self_ = ParseCurrentSummoner(*body);
            if (self_) {
                std::println("лобби: клиент League найден, аккаунт {}", self_->riot_id);
            }
        }
    }

    if (self_) {
        next.self_riot_id = self_->riot_id;
    }

    if (next.phase == "ChampSelect") {
        TickChampSelect(next);
    } else if (InGamePhase(next.phase)) {
        TickInGame(next);
    }

    const std::lock_guard<std::mutex> lock(mutex_);
    lobby_ = std::move(next);
}

void LobbyService::TickChampSelect(Lobby& next) {
    const auto body = lcu_.Get("/lol-champ-select/v1/session");
    if (!body) {
        return;
    }
    auto session = ParseChampSelectSession(*body);
    if (!session) {
        return;
    }
    session->phase = next.phase;
    next = std::move(*session);

    // Своё имя клиент в сессии иногда не присылает — берём из аккаунта.
    std::vector<std::string> riot_ids;
    for (LobbyMember& member : next.members) {
        if (member.is_self && self_) {
            if (member.riot_id.empty()) member.riot_id = self_->riot_id;
            if (member.puuid.empty()) member.puuid = self_->puuid;
        }
        if (member.side != LobbySide::Ally || member.riot_id.empty()) {
            continue;
        }
        if (profiles_ != nullptr) {
            profiles_->HintPuuid(member.riot_id, member.puuid);
        }
        riot_ids.push_back(member.riot_id);
    }
    if (profiles_ != nullptr && !riot_ids.empty()) {
        profiles_->Request(riot_ids);
    }
}

void LobbyService::TickInGame(Lobby& next) {
    if (!self_) {
        next.note = "не знаю свой аккаунт: клиент не ответил на current-summoner";
        return;
    }

    // Сначала сессия игры в самом клиенте: локально, без ключа, сразу
    // с экрана загрузки, и имена в ней те, что игрок видит на этом экране.
    if (const auto body = lcu_.Get("/lol-gameflow/v1/session")) {
        if (auto members = ParseGameflowSession(*body, self_->puuid);
            members && !members->empty()) {
            std::vector<std::string> riot_ids;
            for (const LobbyMember& member : *members) {
                if (profiles_ != nullptr && !member.riot_id.empty()) {
                    profiles_->HintPuuid(member.riot_id, member.puuid);
                    riot_ids.push_back(member.riot_id);
                }
            }
            if (profiles_ != nullptr && !riot_ids.empty()) {
                profiles_->Request(riot_ids);
            }
            if (!riot_ids.empty()) {
                next.source = "lcu-game";
                next.members = std::move(*members);
                return;
            }
            // Имён клиент не дал — чемпионов покажем, но состав с именами
            // попробуем взять из spectator-v5 ниже.
            next.source = "lcu-game";
            next.members = std::move(*members);
        }
    }

    if (profiles_ == nullptr) {
        if (next.members.empty()) {
            next.note = "нет ключа Riot — spectator-v5 недоступен";
        }
        return;
    }

    const ProfileService::ActiveGame game = profiles_->LastActiveGame();
    if (game.members) {
        next.source = "spectator";
        next.members = *game.members;
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (spectate_attempts_ < kSpectateAttempts &&
        (spectate_attempts_ == 0 || now - last_spectate_ >= kSpectateRetry)) {
        ++spectate_attempts_;
        last_spectate_ = now;
        profiles_->RequestActiveGame(self_->puuid);
    }
    if (next.members.empty()) {
        next.note = game.attempted
                        ? std::format("жду spectator-v5: попытка {} из {}, последний ответ {}",
                                      spectate_attempts_, kSpectateAttempts,
                                      game.status == 0 ? std::string("— нет сети")
                                                       : std::to_string(game.status))
                        : "жду spectator-v5: первый запрос в очереди";
    }
}

}  // namespace sintence
