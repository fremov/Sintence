#ifndef SINTENCE_APP_LIVE_API_SERVER_H
#define SINTENCE_APP_LIVE_API_SERVER_H

#include <memory>
#include <string>

#include "live_game.h"         // интерфейс LiveGameSource
#include "history_service.h"   // HistoryService из data/
#include "lobby_service.h"     // LobbyService из data/
#include "match_store.h"       // MatchStore из core/
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
//   GET /api/profiles -> 200 + готовые профили (ранг, топ-3 мастери) и прогресс
//                        докачки, с матчем и без; идёт матч — заказывает тех,
//                        кто на табло. 501, если ключ Riot не задан.
//   GET /api/lobby    -> 200 + состав до начала матча: выбор чемпиона (LCU)
//                        и экран загрузки (spectator-v5). phase пустая —
//                        клиент League не запущен.
//   GET /api/preferences -> 200 + советы активному игроку против каждого
//                        противника: руны, порядок прокачки, предметы.
//                        Идёт матч — по табло; нет матча — по параметрам
//                        ?champion=Ahri&role=MIDDLE&enemies=Zed,LeeSin
//                        (выбор чемпиона). 503 — ни того ни другого,
//                        501 — пак не загружен.
//   POST /api/champselect/runes  -> записать и выбрать страницу рун;
//   POST /api/champselect/spells -> выбрать пару заклинаний призывателя.
//                        Только по клику в интерфейсе; защищены заголовком
//                        X-Sintence-Action, проверкой Host и Origin — иначе
//                        любой сайт в браузере мог бы менять руны игрока.
//                        Всегда 200 с полем status (ok, need_replace, ...),
//                        403 — запрос не из интерфейса.
//   GET /api/profile?riotId=&depth=50&queue=0 -> иконка, уровень, ранги,
//                        прогресс загрузки истории и сводка по ролям
//                        и чемпионам. Заказывает загрузку depth игр.
//                        riotId нет — свой аккаунт из клиента League.
//   GET /api/matches?riotId=&offset=0&limit=20&queue=0 -> матчи игрока
//                        из хранилища, новые первыми, все десять участников.
//   GET /api/matches/{id}          -> один матч целиком.
//   GET /api/matches/{id}/timeline -> порядок прокачки и покупок, золото
//                        по минутам; 202, пока timeline качается, 502 — не
//                        скачался за три попытки (?retry=1 — заново).
//                        501 у всех четырёх — нет ключа или хранилища.
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
    // lobby    — состав до начала матча или nullptr.
    LiveApiServer(const LiveGameSource& source, std::string web_root, int port = 8777,
                  ProfileService* profiles = nullptr,
                  const PreferencePack* pack = nullptr,
                  const LobbyService* lobby = nullptr,
                  MatchStore* store = nullptr, HistoryService* history = nullptr);
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

// Лобби в JSON для /api/lobby. puuid наружу не отдаётся.
std::string LobbyToJson(const Lobby& lobby);

}  // namespace sintence

#endif  // SINTENCE_APP_LIVE_API_SERVER_H
