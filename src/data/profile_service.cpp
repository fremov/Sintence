#include "profile_service.h"

#include <chrono>
#include <print>
#include <utility>

namespace sintence {

ProfileService::ProfileService(std::shared_ptr<RiotApiClient> client)
    : client_(std::move(client)) {
    thread_ = std::thread([this] { Worker(); });
}

ProfileService::~ProfileService() {
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    wake_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void ProfileService::Request(const std::vector<std::string>& riot_ids) {
    bool added = false;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        for (const std::string& riot_id : riot_ids) {
            if (riot_id.empty() || known_.contains(riot_id)) {
                continue;
            }
            known_.emplace(riot_id, false);
            queue_.push_back(riot_id);
            ++total_;
            added = true;
        }
    }
    if (added) {
        wake_.notify_one();
    }
}

void ProfileService::HintPuuid(const std::string& riot_id, const std::string& puuid) {
    if (riot_id.empty() || puuid.empty()) {
        return;
    }
    const std::lock_guard<std::mutex> lock(mutex_);
    hints_.emplace_back(riot_id, puuid);
}

void ProfileService::RequestActiveGame(const std::string& self_puuid) {
    if (self_puuid.empty()) {
        return;
    }
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (active_game_request_ == self_puuid) {
            return;
        }
        active_game_request_ = self_puuid;
    }
    wake_.notify_one();
}

ProfileService::ActiveGame ProfileService::LastActiveGame() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return active_game_;
}

void ProfileService::ResetActiveGame() {
    const std::lock_guard<std::mutex> lock(mutex_);
    active_game_ = ActiveGame{};
    active_game_request_.clear();
}

std::vector<PlayerProfile> ProfileService::Ready() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return ready_;
}

ProfileService::Progress ProfileService::Status() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    Progress progress;
    progress.done = static_cast<int>(ready_.size());
    progress.total = total_;
    progress.running = running_ || !queue_.empty();
    return progress;
}

void ProfileService::Worker() {
    for (;;) {
        std::string riot_id;
        std::string spectate_puuid;
        std::vector<std::pair<std::string, std::string>> hints;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock, [this] {
                return stop_ || !queue_.empty() || !active_game_request_.empty();
            });
            if (stop_) {
                return;
            }
            hints.swap(hints_);
            // Состав игры важнее очередного профиля: он сам заказывает
            // профили всех десяти, и чем раньше, тем раньше они готовы.
            if (!active_game_request_.empty()) {
                spectate_puuid = std::move(active_game_request_);
                active_game_request_.clear();
            } else {
                riot_id = std::move(queue_.front());
                queue_.pop_front();
            }
            running_ = true;
        }

        for (const auto& [hint_riot_id, hint_puuid] : hints) {
            client_->RememberPuuid(hint_riot_id, hint_puuid);
        }

        if (!spectate_puuid.empty()) {
            std::println("лобби: spectator-v5 — состав идущей игры");
            auto members = client_->LoadActiveGame(spectate_puuid);
            const int status = client_->LastStatus();
            std::vector<std::string> riot_ids;
            if (members) {
                for (const LobbyMember& member : *members) {
                    riot_ids.push_back(member.riot_id);
                }
                std::println("лобби: в игре {} участников, заказываю профили", members->size());
            } else {
                std::println("лобби: spectator-v5 игры не знает (ещё не началась "
                             "или не наблюдаема)");
            }
            {
                const std::lock_guard<std::mutex> lock(mutex_);
                running_ = false;
                active_game_.self_puuid = spectate_puuid;
                active_game_.attempted = true;
                active_game_.status = status;
                active_game_.members = std::move(members);
            }
            Request(riot_ids);
            continue;
        }

        std::println("профиль: запрашиваю {}", riot_id);
        const auto started = std::chrono::steady_clock::now();

        // Сеть — вне блокировки: пока идёт запрос, интерфейс продолжает
        // забирать уже готовые профили, а не ждёт мьютекс сорок секунд.
        auto profile = client_->LoadProfile(riot_id);

        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started);

        {
            const std::lock_guard<std::mutex> lock(mutex_);
            running_ = false;
            known_[riot_id] = true;
            if (profile) {
                std::println("профиль: {} готов за {} мс (соло-ранг: {}, мастери: {})",
                             riot_id, elapsed.count(),
                             profile->solo_queue ? "есть" : "нет",
                             profile->top_masteries.size());
                ready_.push_back(std::move(*profile));
            } else {
                // Не вышло — из общего счётчика вычитаем, иначе прогресс
                // навсегда застрянет на «9 из 10».
                std::println("профиль: {} не получен за {} мс", riot_id, elapsed.count());
                --total_;
            }
        }
    }
}

}  // namespace sintence
