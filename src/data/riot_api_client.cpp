#include "riot_api_client.h"

#include <httplib.h>

#include <cctype>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <thread>
#include <utility>

#include "app_log.h"
#include "riot_api_json.h"

namespace sintence {

namespace {

// Тайм-аут на запрос к Riot. Это интернет, а не локальный порт 2999:
// двух секунд, которых хватает Live Client API, здесь мало.
constexpr int kTimeoutSeconds = 5;

bool IsUnreserved(unsigned char byte) {
    return std::isalnum(byte) != 0 || byte == '-' || byte == '.' || byte == '_' ||
           byte == '~';
}

std::string Trim(std::string_view text) {
    const auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };

    std::size_t begin = 0;
    while (begin < text.size() && is_space(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin && is_space(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}

std::optional<std::string> EnvVar(const char* name) {
    // getenv, а не _dupenv_s: _CRT_SECURE_NO_WARNINGS уже определён в сборке.
    const char* value = std::getenv(name);
    if (value == nullptr) {
        return std::nullopt;
    }
    std::string text = Trim(value);
    if (text.empty()) {
        return std::nullopt;
    }
    return text;
}

// Riot отдаёт Retry-After в целых секундах.
int RetryAfterSeconds(const httplib::Response& response) {
    if (!response.has_header("Retry-After")) {
        return 1;
    }
    const std::string value = response.get_header_value("Retry-After");
    int seconds = 0;
    const auto* first = value.data();
    const auto* last = value.data() + value.size();
    const auto result = std::from_chars(first, last, seconds);
    if (result.ec != std::errc{} || seconds <= 0) {
        return 1;
    }
    return seconds;
}

}  // namespace

std::string UrlEncodePathSegment(std::string_view segment) {
    std::string encoded;
    encoded.reserve(segment.size());

    for (const char symbol : segment) {
        // Приведение обязательно: char в MSVC знаковый, и байт 0xD0
        // без него напечатался бы как FFFFFFD0.
        const auto byte = static_cast<unsigned char>(symbol);
        if (IsUnreserved(byte)) {
            encoded.push_back(symbol);
        } else {
            encoded += std::format("%{:02X}", byte);
        }
    }
    return encoded;
}

std::optional<std::string> LoadRiotApiKey() {
    if (auto from_env = EnvVar("SINTENCE_RIOT_KEY")) {
        return from_env;
    }

    const auto local_app_data = EnvVar("LOCALAPPDATA");
    if (!local_app_data) {
        return std::nullopt;
    }

    const std::filesystem::path key_file =
        std::filesystem::path(*local_app_data) / "Sintence" / "riot_key.txt";
    std::ifstream stream(key_file);
    if (!stream) {
        return std::nullopt;
    }

    std::string line;
    std::getline(stream, line);
    // Ключ, скопированный с сайта, приезжает с \r\n. Заголовок с переводом
    // строки Riot отвергает с 403, а причина по ответу не видна.
    const std::string key = Trim(line);
    if (key.empty()) {
        return std::nullopt;
    }
    return key;
}

RiotApiClient::RiotApiClient(std::string api_key, std::string fallback_platform_host,
                             std::string regional_host)
    : api_key_(std::move(api_key)),
      fallback_platform_host_(std::move(fallback_platform_host)),
      regional_host_(std::move(regional_host)),
      limiter_(RateLimiter::RiotPersonalKey()) {}

std::optional<std::string> RiotApiClient::Get(const std::string& host,
                                              const std::string& path) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    const httplib::Headers headers{{"X-Riot-Token", api_key_}};

    for (int attempt = 0; attempt < 2; ++attempt) {
        const auto delay = limiter_.DelayUntilAllowed(RateLimiter::Clock::now());
        if (delay > std::chrono::milliseconds{0}) {
            Log("riot: жду {} мс — лимит ключа ({} за секунду, {} за 2 минуты)",
                         delay.count(), limiter_.Used(0, RateLimiter::Clock::now()),
                         limiter_.Used(1, RateLimiter::Clock::now()));
            std::this_thread::sleep_for(delay);
        }

        httplib::SSLClient client(host);
        client.set_connection_timeout(kTimeoutSeconds);
        client.set_read_timeout(kTimeoutSeconds);

        const auto started = std::chrono::steady_clock::now();
        auto response = client.Get(path, headers);
        const auto now = RateLimiter::Clock::now();
        limiter_.Record(now);
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started);

        if (!response) {
            last_status_ = 0;
            LogError("riot: нет ответа от {} ({}) за {} мс", host, path, elapsed.count());
            return std::nullopt;
        }
        last_status_ = response->status;

        Log("riot: {} {} -> {} за {} мс [окно: {}/20 за с, {}/100 за 2 мин]",
                     host, path, response->status, elapsed.count(),
                     limiter_.Used(0, now), limiter_.Used(1, now));

        if (response->status == 200) {
            return response->body;
        }
        if (response->status == 429) {
            const int seconds = RetryAfterSeconds(*response);
            Log("riot: 429, пауза {} с, один повтор", seconds);
            limiter_.BlockUntil(now + std::chrono::seconds{seconds});
            continue;  // один повтор, и только после паузы
        }
        if (response->status == 401) {
            // Ключа в заголовке фактически не было: пустая строка, BOM
            // в начале файла или мусор вместо RGAPI-...
            LogError("riot: 401 — ключ не дошёл до Riot; проверь "
                         "SINTENCE_RIOT_KEY или riot_key.txt (длина ключа {})",
                         api_key_.size());
            return std::nullopt;
        }
        if (response->status == 403) {
            // Ключ в журнал не попадает ни при каких обстоятельствах.
            LogError("riot: 403 — ключ недействителен или истёк");
            return std::nullopt;
        }
        if (response->status == 404) {
            // Обычная ситуация: у игрока нет ранга или он сменил регион.
            return std::nullopt;
        }
        return std::nullopt;
    }
    LogError("riot: повтор после 429 не удался ({})", path);
    return std::nullopt;
}

std::optional<std::string> RiotApiClient::ResolvePuuid(std::string_view riot_id) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    const std::string key(riot_id);
    if (const auto cached = puuid_cache_.find(key); cached != puuid_cache_.end()) {
        Log("riot: puuid для {} взят из кеша — запроса нет", key);
        return cached->second;
    }

    const auto split = SplitRiotId(riot_id);
    if (!split) {
        // Бот или заглушка клиента: у живого игрока тег есть всегда.
        Log("riot: {} — не Riot ID (нет тега), запрос не отправляю", key);
        return std::nullopt;
    }

    // account-v1 живёт на РЕГИОНАЛЬНОМ хосте (europe), а не на платформенном.
    const std::string path = "/riot/account/v1/accounts/by-riot-id/" +
                             UrlEncodePathSegment(split->first) + "/" +
                             UrlEncodePathSegment(split->second);

    const auto body = Get(regional_host_, path);
    if (!body) {
        return std::nullopt;
    }
    auto puuid = ParsePuuid(*body);
    if (!puuid) {
        return std::nullopt;
    }

    puuid_cache_.emplace(key, *puuid);
    return puuid;
}

std::string RiotApiClient::ResolvePlatformHost(const std::string& puuid) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (const auto cached = platform_cache_.find(puuid); cached != platform_cache_.end()) {
        return cached->second;
    }

    const auto body = Get(regional_host_,
                          "/riot/account/v1/region/by-game/lol/by-puuid/" + puuid);
    std::string host = fallback_platform_host_;
    if (body) {
        if (const auto region = ParseActiveRegion(*body)) {
            if (auto resolved = PlatformHost(*region)) {
                Log("riot: регион аккаунта — {} ({})", *region, *resolved);
                host = std::move(*resolved);
            }
        }
    } else {
        Log("riot: регион не определён, беру запасной {}", host);
    }

    platform_cache_.emplace(puuid, host);
    return host;
}

std::optional<PlayerProfile> RiotApiClient::LoadProfile(std::string_view riot_id) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto puuid = ResolvePuuid(riot_id);
    if (!puuid) {
        return std::nullopt;
    }

    PlayerProfile profile;
    profile.riot_id = std::string(riot_id);
    profile.puuid = std::move(*puuid);

    const std::string platform_host = ResolvePlatformHost(profile.puuid);

    if (const auto body = Get(platform_host,
                              "/lol/league/v4/entries/by-puuid/" + profile.puuid)) {
        if (const auto entries = ParseRankedEntries(*body)) {
            if (const RankedStats* solo = FindQueue(*entries, "RANKED_SOLO_5x5")) {
                profile.solo_queue = *solo;
            } else {
                Log("riot: {} — записей {}, соло-очереди среди них нет",
                             profile.riot_id, entries->size());
            }
        }
    }

    if (const auto body =
            Get(platform_host,
                "/lol/champion-mastery/v4/champion-masteries/by-puuid/" +
                    profile.puuid + "/top?count=3")) {
        if (auto masteries = ParseChampionMasteries(*body)) {
            profile.top_masteries = std::move(*masteries);
        }
    }

    // Неудача ранга или мастери профиль не отменяет: частичная карточка
    // полезнее пустой.
    return profile;
}

void RiotApiClient::RememberPuuid(const std::string& riot_id, const std::string& puuid) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!riot_id.empty() && !puuid.empty()) {
        puuid_cache_.insert_or_assign(riot_id, puuid);
    }
}

std::optional<std::vector<LobbyMember>> RiotApiClient::LoadActiveGame(
    const std::string& self_puuid) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (self_puuid.empty()) {
        return std::nullopt;
    }
    const std::string platform_host = ResolvePlatformHost(self_puuid);
    const auto body =
        Get(platform_host, "/lol/spectator/v5/active-games/by-summoner/" + self_puuid);
    if (!body) {
        return std::nullopt;
    }
    auto members = ParseActiveGame(*body, self_puuid);
    if (members) {
        for (const LobbyMember& member : *members) {
            RememberPuuid(member.riot_id, member.puuid);
            // Участники одной игры играют на одной платформе: регион
            // каждого не спрашиваем, это сэкономило бы до десяти запросов.
            if (!member.puuid.empty()) {
                platform_cache_.emplace(member.puuid, platform_host);
            }
        }
    }
    return members;
}

std::vector<PlayerProfile> RiotApiClient::LoadProfiles(
    const std::vector<std::string>& riot_ids) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<PlayerProfile> profiles;
    profiles.reserve(riot_ids.size());

    for (const std::string& riot_id : riot_ids) {
        if (auto profile = LoadProfile(riot_id)) {
            profiles.push_back(std::move(*profile));
        }
    }
    return profiles;
}

std::optional<SummonerInfo> RiotApiClient::LoadSummoner(const std::string& puuid) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    const auto body = Get(ResolvePlatformHost(puuid), "/lol/summoner/v4/summoners/by-puuid/" + puuid);
    return body ? ParseSummonerInfo(*body) : std::nullopt;
}

std::optional<std::vector<RankedStats>> RiotApiClient::LoadRankedEntries(const std::string& puuid) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    const auto body = Get(ResolvePlatformHost(puuid), "/lol/league/v4/entries/by-puuid/" + puuid);
    return body ? ParseRankedEntries(*body) : std::nullopt;
}

std::optional<std::vector<std::string>> RiotApiClient::LoadMatchIds(const std::string& puuid,
                                                                    int start, int count) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    // match-v5 живёт на РЕГИОНАЛЬНОМ хосте, как account-v1.
    const auto body = Get(regional_host_, std::format("/lol/match/v5/matches/by-puuid/{}/ids?start={}&count={}",
                                                      puuid, start, count));
    return body ? ParseMatchIds(*body) : std::nullopt;
}

std::optional<std::string> RiotApiClient::LoadMatchJson(const std::string& match_id) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    return Get(regional_host_, "/lol/match/v5/matches/" + match_id);
}

std::optional<std::string> RiotApiClient::LoadTimelineJson(const std::string& match_id) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    return Get(regional_host_, "/lol/match/v5/matches/" + match_id + "/timeline");
}

}  // namespace sintence
