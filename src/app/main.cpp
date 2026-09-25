// Точка входа: поднимает сервер данных и окно поверх игры.
//
//   cmake --build build --config Debug --target sintence
//   build\src\Debug\sintence.exe
//
// Здесь только связывание готовых частей, без логики:
//   LiveClientSource  — откуда берутся данные (data/)
//   LiveApiServer     — отдаёт их интерфейсу по HTTP (app/)
//   RunOverlay        — окно с WebView2, показывает web/dist (app/)
//
// Интерфейс правится в web/ и не требует пересборки C++:
//   npm run build   — собрать в web/dist, окно подхватит при перезапуске
//   npm run dev     — :5173 с горячей перезагрузкой, запросы к /api
//                     проксируются сюда же

#include <cstdio>
#include <print>
#include <string>

#include "live_api_server.h"
#include "live_client_source.h"
#include "overlay_window.h"

int main() {
    // Консоль здесь — журнал отладки. Буферизация означает, что при аварийном
    // завершении последние сообщения (ровно те, что объясняют причину)
    // теряются вместе с буфером.
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    const sintence::LiveClientSource live_source;

    sintence::LiveApiServer server(live_source, SINTENCE_WEB_DIR, 8777);
    if (!server.Start()) {
        std::println("не удалось занять порт {} — он уже кем-то занят", server.Port());
        return 1;
    }
    std::println("сервер данных: http://127.0.0.1:{}/api/live", server.Port());
    std::println("игра {}", live_source.IsAvailable() ? "идёт" : "не запущена");

    sintence::OverlayOptions options;
    options.url = L"http://127.0.0.1:8777/";
    return sintence::RunOverlay(options);
}
