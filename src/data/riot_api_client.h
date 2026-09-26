#ifndef SINTENCE_DATA_RIOT_API_CLIENT_H
#define SINTENCE_DATA_RIOT_API_CLIENT_H

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "player_profile.h"  // PlayerProfile из core/
#include "rate_limiter.h"

// data/riot_api_client — профили игроков лобби из Riot API.
//
// Что собирает на каждого игрока (три запроса):
//   1. account-v1  by-riot-id       -> puuid
//   2. league-v4   entries/by-puuid -> тир, дивизион, LP, W/L соло
//   3. champion-mastery-v4 top?count=3 -> три чемпиона, уровень, дата
//
// Десять игроков = 30 запросов. При лимитах personal-ключа
// (20/с и 100/2 мин) это около сорока секунд на полное лобби —
// поэтому запросы идут через RateLimiter, а puuid кешируются:
// вторая игра с тем же человеком не стоит ни одного обращения.
//
// ДВА РАЗНЫХ ХОСТА, и это главный источник ошибок 404 здесь:
//   account-v1               -> региональный: europe.api.riotgames.com
//   league-v4, mastery-v4    -> платформенный: euw1.api.riotgames.com
// Перепутать их — получить 404 на совершенно правильный запрос.
//
// Ключ в репозиторий, в бинарник и во фронтенд не попадает (POLICY.md):
// он читается из переменной окружения или из файла в профиле пользователя.
//
// КРИТЕРИИ ГОТОВНОСТИ (задача 3 из трёх):
//   1. Тесты test_riot_api_client.cpp (чистые функции) зелёные.
//   2. Живая проверка: профиль собственного аккаунта приходит целиком —
//      тир, LP, W/L и три чемпиона с датами.
//   3. Ключ нигде не печатается в журнал, даже при ошибке.
//   4. Отсутствие ключа — понятное сообщение, а не падение и не пустой ответ.
//   5. Ответ 429 не роняет выгрузку: пауза по Retry-After и продолжение.

namespace sintence {

// Кодирует один сегмент пути URL.
//
// Riot ID содержит пробелы ("Riot Tuxedo"), а пробел в пути — это
// сломанный запрос. Кодируются все символы, кроме незарезервированных
// (RFC 3986): A-Z a-z 0-9 - . _ ~
//
//   "Riot Tuxedo" -> "Riot%20Tuxedo"
//   "Ёж"          -> "%D0%81%D0%B6"  (UTF-8, по байтам)
//
// Внимание на знаковость: char в MSVC знаковый, и байт 0xD0 в нём
// отрицателен. Печатать его в hex без приведения к unsigned char —
// получить "%FFFFFFD0". Тест на это есть.
std::string UrlEncodePathSegment(std::string_view segment);

// Читает ключ Riot: сначала переменная окружения SINTENCE_RIOT_KEY,
// потом файл %LOCALAPPDATA%\Sintence\riot_key.txt (первая строка,
// пробелы и перевод строки обрезаются).
//
// Ни того, ни другого нет — nullopt. Ключ не логируется никогда.
std::optional<std::string> LoadRiotApiKey();

class RiotApiClient {
public:
    // fallback_platform_host — куда идти, если Riot не сказал регион.
    // regional_host         — надрегион для account-v1 (europe, americas, asia).
    //
    // Обычно платформа не задаётся руками: она определяется по puuid
    // запросом region/by-game и кешируется. Зашитый регион — главный
    // источник «ранга нет» там, где ранг есть: чужая платформа отвечает
    // пустым массивом, а не ошибкой.
    RiotApiClient(std::string api_key,
                  std::string fallback_platform_host = "euw1.api.riotgames.com",
                  std::string regional_host = "europe.api.riotgames.com");

    // Riot ID -> puuid. Второй вызов с тем же именем сети не касается.
    std::optional<std::string> ResolvePuuid(std::string_view riot_id);

    // puuid -> платформенный хост ("ru.api.riotgames.com"). Один запрос
    // на игрока, результат кешируется. Riot не ответил — возвращается
    // запасной хост из конструктора.
    std::string ResolvePlatformHost(const std::string& puuid);

    // Полный профиль одного игрока: три запроса (или меньше, если puuid
    // уже в кеше). Не удался первый запрос — nullopt: без puuid остальные
    // бессмысленны. Не удался второй или третий — профиль всё равно
    // возвращается, просто без ранга или без мастери. Частичные данные
    // полезнее, чем пустая карточка.
    std::optional<PlayerProfile> LoadProfile(std::string_view riot_id);

    // Все десять разом. Порядок результата совпадает с порядком входа;
    // игроки, по которым не вышло вообще ничего, пропускаются.
    std::vector<PlayerProfile> LoadProfiles(const std::vector<std::string>& riot_ids);

private:
    // Один GET с заголовком X-Riot-Token.
    //
    // Перед отправкой спрашивает у лимитера DelayUntilAllowed и спит,
    // после отправки зовёт Record. Ответ 429 — читает Retry-After
    // (секунды), зовёт BlockUntil и повторяет запрос ОДИН раз.
    //
    // nullopt означает любое из: сеть недоступна, код не 200,
    // повтор после 429 тоже не удался.
    std::optional<std::string> Get(const std::string& host, const std::string& path);

    std::string api_key_;
    std::string fallback_platform_host_;
    std::string regional_host_;
    RateLimiter limiter_;
    std::unordered_map<std::string, std::string> puuid_cache_;
    std::unordered_map<std::string, std::string> platform_cache_;  // puuid -> хост
};

}  // namespace sintence

#endif  // SINTENCE_DATA_RIOT_API_CLIENT_H
