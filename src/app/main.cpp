// Точка входа: поднимает сервер данных и окно поверх игры.
//
//   cmake --build build --config Debug --target sintence
//   build\src\Debug\sintence.exe
//
// Здесь только связывание готовых частей, без логики:
//   LiveClientSource  — откуда берутся данные (data/)
//   LiveApiServer     — отдаёт их интерфейсу по HTTP (app/)
//   RunOverlay        — окно с WebView2, показывает собранный интерфейс (app/)
//
// Интерфейс живёт в отдельном репозитории sintence-web и не требует
// пересборки C++. Две переменные окружения связывают их без правки кода:
//   SINTENCE_WEB_DIR — где лежит собранный интерфейс (его dist/);
//                      по умолчанию ../sintence-web/dist рядом с этим репозиторием
//   SINTENCE_UI_URL  — что открывать в окне вместо собранного интерфейса,
//                      например http://localhost:5173 (npm run dev в sintence-web):
//                      окно показывает живой Vite с горячей перезагрузкой
//   SINTENCE_PORT    — порт локального сервера, по умолчанию 8777. Нужен, чтобы
//                      запустить вторую копию (отладочную сборку) рядом с рабочей

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <print>
#include <string>

#include "history_service.h"
#include "live_api_server.h"
#include "live_client_source.h"
#include "lobby_service.h"
#include "overlay_window.h"
#include "preference_pack.h"
#include "profile_service.h"
#include "riot_api_client.h"
#include "sqlite_match_store.h"

namespace {

// Переменная окружения или пустая строка. _dupenv_s вместо getenv:
// у этой мишени нет _CRT_SECURE_NO_WARNINGS, а предупреждение C4996
// в журнале сборки шумит зря.
std::string EnvOrEmpty(const char* name) {
#ifdef _MSC_VER
    char* value = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&value, &size, name) != 0 || value == nullptr) {
        return {};
    }
    std::string result(value);
    std::free(value);
    return result;
#else
    const char* value = std::getenv(name);
    return value ? std::string(value) : std::string();
#endif
}

}  // namespace

// Порт из SINTENCE_PORT или 8777. Мусор в переменной — тоже 8777:
// молча слушать непонятно что хуже, чем стандартный порт.
int PortFromEnv() {
    const std::string text = EnvOrEmpty("SINTENCE_PORT");
    int port = 0;
    for (const char symbol : text) {
        if (symbol < '0' || symbol > '9' || port > 65535) {
            return 8777;
        }
        port = port * 10 + (symbol - '0');
    }
    return port > 0 && port <= 65535 ? port : 8777;
}

int main() {
    // Консоль здесь — журнал отладки. Буферизация означает, что при аварийном
    // завершении последние сообщения (ровно те, что объясняют причину)
    // теряются вместе с буфером.
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    const sintence::LiveClientSource live_source;

    // Ключ Riot: переменная окружения SINTENCE_RIOT_KEY или файл
    // %LOCALAPPDATA%\Sintence\riot_key.txt. Без ключа приложение работает,
    // просто без рангов и мастери: /api/profiles отвечает 501.
    // Клиент Riot один на всех: профили лобби и история окна профиля идут
    // через один ограничитель запросов — лимит ключа общий.
    std::shared_ptr<sintence::RiotApiClient> riot;
    std::unique_ptr<sintence::ProfileService> profiles;
    if (const auto key = sintence::LoadRiotApiKey()) {
        riot = std::make_shared<sintence::RiotApiClient>(*key, "ru.api.riotgames.com",
                                                         "europe.api.riotgames.com");
        profiles = std::make_unique<sintence::ProfileService>(riot);
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

    // Собранный интерфейс. Нет index.html — окно покажет пустоту, и лучше
    // сказать об этом сразу, чем гадать потом по чёрному прямоугольнику.
    std::string web_dir = EnvOrEmpty("SINTENCE_WEB_DIR");
    if (web_dir.empty()) {
        web_dir = SINTENCE_WEB_DIR;
    }
    const std::string ui_url = EnvOrEmpty("SINTENCE_UI_URL");
    if (ui_url.empty() && !std::filesystem::exists(std::filesystem::path(web_dir) / "index.html")) {
        std::println("интерфейс не собран: нет {}/index.html\n"
                     "  собери его: cd ../sintence-web && npm run build\n"
                     "  или укажи каталог: SINTENCE_WEB_DIR=<путь к dist>",
                     web_dir);
    }

    // История матчей игроков — окно профиля. Сейчас SQLite на машине
    // разработчика (project/data/history.sqlite, путь меняет
    // SINTENCE_HISTORY_DB), потом — сервер: адаптер сменится, интерфейс нет.
    std::string history_path = EnvOrEmpty("SINTENCE_HISTORY_DB");
    if (history_path.empty()) {
        history_path = SINTENCE_HISTORY_DB;
    }
    std::unique_ptr<sintence::SqliteMatchStore> store = sintence::SqliteMatchStore::Open(history_path);
    std::unique_ptr<sintence::HistoryService> history;
    if (store && riot) {
        history = std::make_unique<sintence::HistoryService>(riot, *store, profiles.get());
        std::println("история матчей: {}", history_path);
    } else if (!store) {
        std::println("история матчей недоступна: не открыть {}", history_path);
    }

    // Состав до начала матча: выбор чемпиона из клиента League (LCU)
    // и экран загрузки через spectator-v5. Работает и без ключа Riot —
    // тогда без профилей. Клиент не запущен — просто пустое лобби.
    const sintence::LobbyService lobby(profiles.get());

    const int port = PortFromEnv();
    sintence::LiveApiServer server(live_source, web_dir, port, profiles.get(),
                                   pack ? &*pack : nullptr, &lobby, store.get(),
                                   history.get());
    if (!server.Start()) {
        std::println("не удалось занять порт {} — он уже кем-то занят", server.Port());
        return 1;
    }
    std::println("сервер данных: http://127.0.0.1:{}/api/live", server.Port());
    std::println("игра {}", live_source.IsAvailable() ? "идёт" : "не запущена");

    sintence::OverlayOptions options;
    const std::string own_url = std::format("http://127.0.0.1:{}/", port);
    options.url = std::wstring(own_url.begin(), own_url.end());
    if (!ui_url.empty()) {
        // Адрес — ASCII (схема, хост, порт), поэтому побайтовое расширение
        // до wchar_t здесь корректно.
        options.url = std::wstring(ui_url.begin(), ui_url.end());
        std::println("окно показывает {} (SINTENCE_UI_URL)", ui_url);
    } else {
        std::println("интерфейс: {}", web_dir);
    }
    // Окно профиля — та же страница с маршрутом #/profile.
    // SINTENCE_NO_PROFILE=1 — только оверлей, как раньше.
    if (EnvOrEmpty("SINTENCE_NO_PROFILE").empty()) {
        std::wstring base = options.url;
        if (const auto hash = base.find(L'#'); hash != std::wstring::npos) {
            base.resize(hash);
        }
        options.profile_url = base + L"#/profile";
    }
    return sintence::RunOverlay(options);
}
