#ifndef SINTENCE_DATA_LIVE_CLIENT_SOURCE_H
#define SINTENCE_DATA_LIVE_CLIENT_SOURCE_H

#include <optional>
#include <string>
#include <string_view>
#include "live_game.h"  // интерфейс LiveGameSource из core/

// data/live_client_source — источник живого матча по Live Client Data API.
//
// Сеть здесь локальная: https://127.0.0.1:2999, игровой клиент на этой же
// машине. Ключ Riot не нужен — API официально разрешён и работает без
// аутентификации (project/POLICY.md).
//
// Игра не запущена — IsAvailable() == false и LoadGame() == nullopt, без
// исключений и без зависания. Таймауты выставлены явно и не превышают
// двух секунд: источник опрашивается во время матча, и залипшее соединение
// подвесило бы интерфейс. Тип httplib в заголовок не протекает.

namespace sintence {

class LiveClientSource : public LiveGameSource {
public:
    // Адрес игрового клиента. По умолчанию — то, что слушает League:
    // 127.0.0.1:2999. Параметры оставлены ради тестов и Practice Tool
    // на другой машине; трогать их в обычной работе не нужно.
    explicit LiveClientSource(std::string host = "127.0.0.1", int port = 2999);

    // "live-client"
    std::string Name() const override;

    // Отвечает ли клиент на /liveclientdata/gamestats кодом 200.
    // Вне матча порт закрыт, и соединение не устанавливается вовсе —
    // это не HTTP-ошибка, а отсутствие соединения; различать их полезно.
    bool IsAvailable() const override;

    // Один запрос: /liveclientdata/allgamedata. В нём и статистика матча,
    // и табло с предметами всех десяти, и активный игрок с рунами
    // и уровнями способностей — согласованные между собой, потому что
    // это один снимок, а не три запроса в разные моменты игры.
    //
    // Запрос не удался или ответ не разобрался — nullopt.
    std::optional<LiveGame> LoadGame() const override;

private:
    // Один GET к игровому клиенту: создаёт настроенный httplib-клиент,
    // ходит по path, отдаёт тело ответа.
    //
    // Возврат std::optional<std::string>, а не просто строка, потому что
    // пустое тело и несостоявшийся запрос — разные вещи, и вызывающий
    // обязан их различать. nullopt означает любое из трёх: соединение
    // не установилось (игра не запущена), ответ пришёл не с кодом 200,
    // тело не прочиталось.
    //
    // Тип httplib здесь не упоминается намеренно: он остаётся внутри .cpp,
    // и граница слоя data/ не протекает наружу (ARCHITECTURE.md §4).
    std::optional<std::string> Fetch(std::string_view path) const;

    std::string host_;
    int port_ = 2999;
};

}  // namespace sintence

#endif  // SINTENCE_DATA_LIVE_CLIENT_SOURCE_H
