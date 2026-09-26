#ifndef SINTENCE_DATA_LCU_CLIENT_H
#define SINTENCE_DATA_LCU_CLIENT_H

#include <mutex>
#include <optional>
#include <string>
#include <string_view>

#include "lcu_json.h"  // LcuCredentials

// data/lcu_client — HTTP к клиенту League (LCU API) на 127.0.0.1.
//
// Порт и пароль лежат в lockfile в каталоге установки и меняются при
// каждом запуске клиента. Каталог ищется так:
//   1. переменная окружения SINTENCE_LOL_DIR;
//   2. каталог запущенного LeagueClientUx.exe (путь процесса);
//   3. C:\Riot Games\League of Legends и D:\Riot Games\League of Legends.
// Клиент перезапустили — первый же неудачный запрос сбрасывает учётные
// данные, и следующий перечитает lockfile.
//
// Чтение — GET. Запись — Send, и только из data/lcu_actions по явному клику
// игрока: страница рун и свои заклинания призывателя в выборе чемпиона
// (project/CHAMP_SELECT.md, «Применить руны и заклинания»). Ни выбора
// чемпиона, ни принятия матча, ни чего-либо ещё за игрока.
// Пароль не печатается и никуда не сохраняется.

namespace sintence {

class LcuClient {
public:
    LcuClient() = default;

    // GET пути клиента. nullopt — клиент не запущен, путь недоступен
    // в текущем этапе (404 вне выбора чемпиона) или ошибка сети.
    std::optional<std::string> Get(std::string_view path);

    // Запрос с телом: метод "POST", "PUT", "PATCH" или "DELETE".
    // status 0 — клиент не найден или не ответил.
    struct Response {
        int status = 0;
        std::string body;
    };
    Response Send(std::string_view method, std::string_view path, std::string_view json_body = {});

    // Найден ли клиент при последнем обращении.
    bool Connected() const;

private:
    std::optional<LcuCredentials> Credentials();

    mutable std::mutex mutex_;
    std::optional<LcuCredentials> credentials_;
};

// Каталог установки League или пустая строка (см. порядок поиска выше).
std::string FindLeagueDirectory();

}  // namespace sintence

#endif  // SINTENCE_DATA_LCU_CLIENT_H
