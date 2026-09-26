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

#include <process.h>  // _getpid

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>

#include "app_log.h"
#include "history_service.h"
#include "live_api_server.h"
#include "live_client_source.h"
#include "lobby_service.h"
#include "overlay_window.h"
#include "preference_pack.h"
#include "profile_service.h"
#include "riot_api_client.h"
#include "sqlite_match_store.h"

using sintence::Log;
using sintence::LogError;

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
    // Консоли нет (оконное приложение, /SUBSYSTEM:WINDOWS в CMakeLists):
    // всё, что происходит, пишется в журнал %LOCALAPPDATA%\Sintence\logs.
    const int port = PortFromEnv();

    // Вторая копия не нужна: она подралась бы с первой за порт и за PgDn.
    // Повторный запуск просто показывает окно уже запущенной.
    if (!sintence::AcquireSingleInstance(port)) {
        return 0;
    }

    const auto log_path = sintence::DefaultLogPath(port);
    sintence::OpenLogFile(log_path);
    Log("===== запуск Sintence, pid {}, порт {} =====", _getpid(), port);

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
        Log("ключ Riot найден: профили игроков включены");
    } else {
        Log("ключа Riot нет — профили выключены "
                     "(SINTENCE_RIOT_KEY или %LOCALAPPDATA%\\Sintence\\riot_key.txt)");
    }

    // Пак предпочтений: результат работы scripts/crawl.py и build_pack.py.
    // Приложение не знает ни про базу матчей, ни про SQLite — только
    // про готовый файл. Пака нет — /api/preferences отвечает 501.
    std::optional<sintence::PreferencePack> pack =
        sintence::PreferencePack::LoadFromDirectory(SINTENCE_PACKS_DIR);
    if (pack) {
        Log("пак предпочтений: патч {}, регион {}, бакетов {}",
                     pack->Patch(), pack->Region(), pack->Size());
    } else {
        Log("пака предпочтений нет — соберите его: "
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
        sintence::ShowErrorBox(std::format(
            "Интерфейс не собран: нет {}/index.html.\n\n"
            "Соберите его (cd ../sintence-web && npm run build) или укажите каталог "
            "переменной SINTENCE_WEB_DIR. Окна будут пустыми.",
            web_dir));
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
        Log("история матчей: {}", history_path);
    } else if (!store) {
        LogError("история матчей недоступна: не открыть {}", history_path);
    }

    // Состав до начала матча: выбор чемпиона из клиента League (LCU)
    // и экран загрузки через spectator-v5. Работает и без ключа Riot —
    // тогда без профилей. Клиент не запущен — просто пустое лобби.
    const sintence::LobbyService lobby(profiles.get());

    sintence::LiveApiServer server(live_source, web_dir, port, profiles.get(),
                                   pack ? &*pack : nullptr, &lobby, store.get(),
                                   history.get());
    if (!server.Start()) {
        sintence::ShowErrorBox(std::format(
            "Порт {} занят другой программой — Sintence не может запустить сервер данных.\n\n"
            "Закройте её или задайте другой порт переменной SINTENCE_PORT.",
            server.Port()));
        return 1;
    }
    Log("сервер данных: http://127.0.0.1:{}/api/live", server.Port());
    Log("игра {}", live_source.IsAvailable() ? "идёт" : "не запущена");

    sintence::OverlayOptions options;
    options.port = port;
    // Рядом с журналом: %LOCALAPPDATA%\Sintence\WebView2.
    options.user_data_dir = (log_path.parent_path().parent_path() / L"WebView2").wstring();
    const std::string own_url = std::format("http://127.0.0.1:{}/", port);
    options.url = std::wstring(own_url.begin(), own_url.end());
    if (!ui_url.empty()) {
        // Адрес — ASCII (схема, хост, порт), поэтому побайтовое расширение
        // до wchar_t здесь корректно.
        options.url = std::wstring(ui_url.begin(), ui_url.end());
        Log("окно показывает {} (SINTENCE_UI_URL)", ui_url);
    } else {
        Log("интерфейс: {}", web_dir);
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
    // Оверлей открывается, только когда на нём что-то есть: с первой секунды
    // выбора чемпиона, на экране загрузки и в матче (фазы клиента League).
    // Без клиента (реплей, Practice Tool с отдельным запуском) — по ответу
    // Live Client. Снимок лобби — память, запрос к 2999 — только если фаза
    // ничего не сказала: без игры он отказывает мгновенно.
    options.has_content = [&live_source, &lobby] {
        const std::string phase = lobby.Snapshot().phase;
        if (phase == "ChampSelect" || phase == "GameStart" || phase == "InProgress" ||
            phase == "Reconnect") {
            return true;
        }
        return live_source.IsAvailable();
    };

    const int code = sintence::RunOverlay(options);

    // Выход сразу, без деструкторов служб: история может спать в ограничителе
    // запросов Riot до двух минут, и процесс висел бы невидимым уже после
    // «Выход». Терять нечего — матчи в SQLite пишутся транзакциями, журнал
    // сбрасывается построчно; снятие в диспетчере задач ровно такое же.
    Log("===== выход, код {} =====", code);
    std::_Exit(code);
}
