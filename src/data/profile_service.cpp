#include "profile_service.h"

#include <chrono>
#include <print>
#include <utility>

namespace sintence {

ProfileService::ProfileService(RiotApiClient client)
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
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock, [this] { return stop_ || !queue_.empty(); });
            if (stop_) {
                return;
            }
            riot_id = std::move(queue_.front());
            queue_.pop_front();
            running_ = true;
        }

        std::println("профиль: запрашиваю {}", riot_id);
        const auto started = std::chrono::steady_clock::now();

        // Сеть — вне блокировки: пока идёт запрос, интерфейс продолжает
        // забирать уже готовые профили, а не ждёт мьютекс сорок секунд.
        auto profile = client_.LoadProfile(riot_id);

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
