#ifndef SINTENCE_APP_LIVE_API_SERVER_H
#define SINTENCE_APP_LIVE_API_SERVER_H

#include <memory>
#include <string>

#include "live_game.h"         // интерфейс LiveGameSource
#include "preference_pack.h"   // PreferencePack из data/
#include "profile_service.h"   // ProfileService из data/

// app/live_api_server — локальный HTTP-сервер, которым питается интерфейс.
//
// Источник данных приходит снаружи ссылкой на интерфейс: сервер не знает,
// что за ним — Live Client Data API или фикстура.
//
// Эндпоинты:
//   GET /api/health   -> 200 + версия контракта и включённые функции
//                        (профили, пак). Интерфейс спрашивает при старте.
//   GET /api/live     -> 200 + JSON снимка матча, либо 503, если матча нет.
//                        503, а не 404: ресурс существует, он временно
//                        недоступен, и клиент должен просто спросить позже.
//   GET /api/profiles -> 200 + профили игроков лобби (ранг, топ-3 мастери)
//                        и прогресс докачки; 503, если матча нет, и 501,
//                        если ключ Riot не задан.
//   GET /api/preferences -> 200 + предпочтения по каждому игроку лобби:
//                        руны, порядок скиллов, первый предмет, ядро.
//                        503, если матча нет, 501, если пак не загружен.
//   GET /*            -> статика из каталога собранного интерфейса
//                        (репозиторий sintence-web, его dist/).
//
// Сервер слушает только 127.0.0.1: снаружи машины к нему подключиться
// нельзя, поэтому ни TLS, ни аутентификации здесь нет и не нужно.

namespace sintence {

class LiveApiServer {
public:
    // source   — откуда брать данные; должен жить дольше сервера.
    // web_root — каталог со собранным фронтендом (web/dist).
    // profiles — служба профилей Riot или nullptr, если ключа нет;
    //            тоже должна жить дольше сервера.
    // pack     — предпочтения по чемпионам или nullptr, если пака нет.
    LiveApiServer(const LiveGameSource& source, std::string web_root, int port = 8777,
                  ProfileService* profiles = nullptr,
                  const PreferencePack* pack = nullptr);
    ~LiveApiServer();

    LiveApiServer(const LiveApiServer&) = delete;
    LiveApiServer& operator=(const LiveApiServer&) = delete;

    // Поднимает сервер в отдельном потоке и возвращает управление сразу:
    // httplib::Server::listen блокирует навсегда, а главный поток нужен окну.
    bool Start();

    // Останавливает сервер и дожидается потока. Деструктор зовёт сам.
    void Stop();

    int Port() const;

private:
    // Реализация прячет httplib и std::thread: заголовок app-слоя не должен
    // тащить их за собой в каждый файл, который его включит.
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Снимок матча в JSON того вида, который ждёт web/src/types.ts.
// Поля называются так же, как в core/live_game.h, но в camelCase —
// это граница между C++ и фронтендом, и держать её один в один проще,
// чем переименовывать на полпути.
std::string LiveGameToJson(const LiveGame& game);

// Профили лобби в JSON для web/src/types.ts:
//   {"progress": {...}, "profiles": [...]}
std::string ProfilesToJson(const std::vector<PlayerProfile>& profiles,
                           const ProfileService::Progress& progress);

}  // namespace sintence

#endif  // SINTENCE_APP_LIVE_API_SERVER_H
