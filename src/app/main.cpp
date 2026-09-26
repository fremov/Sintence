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
#include <memory>
#include <optional>
#include <print>
#include <string>

#include "live_api_server.h"
#include "live_client_source.h"
#include "overlay_window.h"
#include "preference_pack.h"
#include "profile_service.h"
#include "riot_api_client.h"

int main() {
    // Консоль здесь — журнал отладки. Буферизация означает, что при аварийном
    // завершении последние сообщения (ровно те, что объясняют причину)
    // теряются вместе с буфером.
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    const sintence::LiveClientSource live_source;

    // Ключ Riot: переменная окружения SINTENCE_RIOT_KEY или файл
    // %LOCALAPPDATA%\Sintence\riot_key.txt. Без ключа приложение работает,
    // просто без рангов и мастери: /api/profiles отвечает 501.
    std::unique_ptr<sintence::ProfileService> profiles;
    if (const auto key = sintence::LoadRiotApiKey()) {
        profiles = std::make_unique<sintence::ProfileService>(
            sintence::RiotApiClient(*key, "ru.api.riotgames.com",
                                    "europe.api.riotgames.com"));
        std::println("ключ Riot найден: профили игроков включены");
    } else {
        std::println("ключа Riot нет — профили выключены "
                     "(SINTENCE_RIOT_KEY или %LOCALAPPDATA%\\Sintence\\riot_key.txt)");
    }

    // Пак предпочтений: результат работы scripts/crawl.py и build_pack.py.
    // Приложение не знает ни про базу матчей, ни про SQLite — только
    // про готовый файл. Пака нет — /api/preferences отвечает 501.
    std::optional<sintence::PreferencePack> pack =
        sintence::PreferencePack::LoadFromDirectory(SINTENCE_PACKS_DIR);
    if (pack) {
        std::println("пак предпочтений: патч {}, регион {}, бакетов {}",
                     pack->Patch(), pack->Region(), pack->Size());
    } else {
        std::println("пака предпочтений нет — соберите его: "
                     "python scripts/build_pack.py");
    }

    sintence::LiveApiServer server(live_source, SINTENCE_WEB_DIR, 8777, profiles.get(),
                                   pack ? &*pack : nullptr);
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
