#include "rate_limiter.h"

#include <algorithm>
#include <utility>

namespace sintence {

RateLimiter::RateLimiter(std::vector<Window> windows) {
    buckets_.reserve(windows.size());
    for (const Window& window : windows) {
        buckets_.push_back(Bucket{window, {}});
    }
}

RateLimiter RateLimiter::RiotPersonalKey() {
    // Порядок окон задаёт индексы для Used(): 0 — секундное, 1 — двухминутное.
    return RateLimiter({{20, std::chrono::milliseconds{1000}},
                        {100, std::chrono::milliseconds{120000}}});
}

std::chrono::milliseconds RateLimiter::DelayUntilAllowed(TimePoint now) const {
    std::chrono::milliseconds delay{0};

    if (now < blocked_until_) {
        delay = std::chrono::duration_cast<std::chrono::milliseconds>(
            blocked_until_ - now);
    }

    for (const Bucket& bucket : buckets_) {
        const TimePoint edge = now - bucket.window.period;

        // Запись, сделанная РОВНО period назад, уже вышла из окна.
        std::size_t used = 0;
        TimePoint oldest_inside{};
        for (auto it = bucket.hits.rbegin(); it != bucket.hits.rend(); ++it) {
            if (*it <= edge) {
                break;
            }
            oldest_inside = *it;
            ++used;
        }

        if (bucket.window.limit <= 0 || used < static_cast<std::size_t>(bucket.window.limit)) {
            continue;
        }

        const auto wait = std::chrono::duration_cast<std::chrono::milliseconds>(
            oldest_inside + bucket.window.period - now);
        delay = std::max(delay, wait);
    }

    return std::max(delay, std::chrono::milliseconds{0});
}

void RateLimiter::Record(TimePoint now) {
    for (Bucket& bucket : buckets_) {
        bucket.hits.push_back(now);
        // Хвост старше окна не нужен никогда: без обрезки deque растёт
        // до десятков тысяч записей за длинную сессию.
        const TimePoint edge = now - bucket.window.period;
        while (!bucket.hits.empty() && bucket.hits.front() <= edge) {
            bucket.hits.pop_front();
        }
    }
}

void RateLimiter::BlockUntil(TimePoint until) {
    // Два подряд 429 не должны сокращать уже назначенную паузу.
    blocked_until_ = std::max(blocked_until_, until);
}

int RateLimiter::Used(std::size_t window_index, TimePoint now) const {
    if (window_index >= buckets_.size()) {
        return 0;
    }

    const Bucket& bucket = buckets_[window_index];
    const TimePoint edge = now - bucket.window.period;

    int used = 0;
    for (auto it = bucket.hits.rbegin(); it != bucket.hits.rend(); ++it) {
        if (*it <= edge) {
            break;
        }
        ++used;
    }
    return used;
}

}  // namespace sintence
