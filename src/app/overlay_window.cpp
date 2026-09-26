#include "overlay_window.h"

// windows.h определяет макросы min и max, и тогда std::min(...) разбирается
// как подстановка макроса: "недопустимая лексема справа от ::" (C2589).
// NOMINMAX обязан стоять ДО включения windows.h.
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>  // GET_X_LPARAM / GET_Y_LPARAM
#include <wrl.h>

#include <WebView2.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <format>
#include <fstream>
#include <optional>
#include <print>
#include <string>

#include "json.hpp"

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace sintence {

namespace {

// Фон окна до первого кадра WebView2. Значение по умолчанию: настоящий
// цвет присылает интерфейс в overlay/config (см. HandleWebMessage),
// и дальше окно красится им — дизайн живёт в web, а не здесь.
constexpr COLORREF kPanelBackground = RGB(0x12, 0x16, 0x1d);
constexpr int kHotkeyVisibilityId = 1;

// Клавиша, которую ловит хук. Меняется из интерфейса (overlay/config),
// читается в потоке хука — отсюда atomic.
std::atomic<UINT> g_toggle_key{VK_NEXT};

// Клавиши, которые интерфейс имеет право назначить. Буквы, цифры и всё,
// что нужно в матче, сюда не входят: перехваченная клавиша съедается,
// и назначить панель на «Q» значило бы сломать игру.
bool IsAllowedToggleKey(UINT key) {
    switch (key) {
        case VK_PRIOR:
        case VK_NEXT:
        case VK_HOME:
        case VK_END:
        case VK_INSERT:
        case VK_PAUSE:
        case VK_SCROLL:
            return true;
        default:
            return key >= VK_F1 && key <= VK_F12;
    }
}

// Сообщение «переключить панель», которое шлёт себе хук клавиатуры.
// Обработчик хука обязан отработать за миллисекунды, иначе Windows снимет
// его без предупреждения, — поэтому он только отправляет сообщение,
// а показывает окно уже оконная процедура.
constexpr UINT WM_APP_TOGGLE_PANEL = WM_APP + 1;

// Хуку нужен доступ к окну, а сигнатуру обратного вызова задаёт Windows:
// протащить туда параметр нельзя, отсюда глобальные переменные.
HHOOK g_keyboard_hook = nullptr;
HWND g_overlay_window = nullptr;

// Журнал рядом с exe. Нужен потому, что главные события происходят, когда
// на экране игра: консоль не видно, а подключиться отладчиком к процессу
// поверх полноэкранного приложения — отдельное приключение.
// Каждая строка сбрасывается на диск сразу: журнал, потерянный при
// аварийном завершении, бесполезен ровно тогда, когда нужен.
void Log(std::string_view message) {
    static std::ofstream file("overlay.log", std::ios::app);
    const auto now = std::chrono::current_zone()->to_local(std::chrono::system_clock::now());
    file << std::format("{:%H:%M:%S} {}\n", std::chrono::floor<std::chrono::seconds>(now), message);
    file.flush();
}

// Запущены ли мы от администратора. Без повышения Windows не отдаёт
// глобальные горячие клавиши, пока активно окно процесса с большими правами
// (League работает с анти-читом Vanguard).
bool IsElevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }
    TOKEN_ELEVATION elevation{};
    DWORD size = sizeof(elevation);
    const bool ok = GetTokenInformation(token, TokenElevation, &elevation, size, &size);
    CloseHandle(token);
    return ok && elevation.TokenIsElevated != 0;
}

// Низкоуровневый хук клавиатуры — единственный способ поймать клавишу,
// пока активна игра.
//
// Почему не RegisterHotKey: игры регистрируют Raw Input с флагом
// RIDEV_NOHOTKEYS, который отключает системные горячие клавиши на время
// своей активности (иначе случайный Win сворачивал бы матч). Системная
// клавиша в этот момент не приходит вообще — проверено журналом: при
// активной игре ни одного WM_HOTKEY. Хук стоит раньше по цепочке обработки
// ввода и этим флагом не отключается.
//
// Это НЕ внедрение в чужой процесс: WH_KEYBOARD_LL живёт в своём процессе,
// ничего никуда не подгружает и анти-читу не интересен. Внедрение в процесс
// игры — то, за что банят, — начинается с перехвата DirectX, а не с этого.
LRESULT CALLBACK KeyboardHook(int code, WPARAM wparam, LPARAM lparam) {
    if (code == HC_ACTION && (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN)) {
        const auto* key = reinterpret_cast<KBDLLHOOKSTRUCT*>(lparam);
        if (key->vkCode == g_toggle_key.load() && g_overlay_window != nullptr) {
            PostMessageW(g_overlay_window, WM_APP_TOGGLE_PANEL, 0, 0);
            // Клавиша съедается: игра и другие программы её не увидят,
            // иначе PgDn заодно пролистывал бы всё, что под панелью.
            return 1;
        }
    }
    return CallNextHookEx(g_keyboard_hook, code, wparam, lparam);
}

// Как панель стоит на экране. Значения по умолчанию — из OverlayOptions;
// дальше их присылает интерфейс сообщением overlay/config.
enum class Anchor { Center, Top };

struct OverlayState {
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;
    // Окно стартует скрытым: оверлей нужен по требованию, а не постоянно.
    bool visible = false;
    // Перекрывается значениями из OverlayOptions при запуске.
    int width = 2240;
    int height = 1280;
    // Какую долю экрана панель может занять, не больше.
    double max_screen_share = 0.9;
    Anchor anchor = Anchor::Center;
    int offset_y = 0;
    COLORREF background = kPanelBackground;
    // Откуда принимать сообщения: origin страницы, которую окно открыло.
    // Сообщение с другого origin (переход по ссылке внутри WebView)
    // окном управлять не должно.
    std::wstring allowed_origin;
};

OverlayState* StateOf(HWND hwnd) {
    return reinterpret_cast<OverlayState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

// Где стоять панели на текущем экране.
//
// Запрошенный размер обрезается по экрану: на 1920x1080 панель
// в 2240 точек шириной уехала бы за край, и половину карточек
// стало бы не видно вовсе.
RECT Placement(const OverlayState& state) {
    const int screen_width = GetSystemMetrics(SM_CXSCREEN);
    const int screen_height = GetSystemMetrics(SM_CYSCREEN);

    const int width = std::min(state.width, static_cast<int>(screen_width * state.max_screen_share));
    const int height =
        std::min(state.height, static_cast<int>(screen_height * state.max_screen_share));
    const int x = (screen_width - width) / 2;
    int y = state.anchor == Anchor::Top ? 0 : (screen_height - height) / 2;
    y = std::clamp(y + state.offset_y, 0, std::max(0, screen_height - height));
    return RECT{x, y, x + width, y + height};
}

// Сообщение странице: overlay/shown, overlay/hidden. Интерфейсу это нужно,
// чтобы, например, обновить данные в момент показа, а не ждать тика опроса.
void NotifyPage(const OverlayState& state, std::string_view type) {
    if (!state.webview) {
        return;
    }
    const std::string json = std::format(R"({{"type":"{}"}})", type);
    const std::wstring wide(json.begin(), json.end());  // только ASCII
    state.webview->PostWebMessageAsJson(wide.c_str());
}

std::string ToUtf8(const wchar_t* text) {
    if (text == nullptr) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) {
        return {};
    }
    std::string result(static_cast<std::size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), size, nullptr, nullptr);
    return result;
}

// "#12161d" -> COLORREF. Любой другой формат — nullopt, цвет не меняется.
std::optional<COLORREF> ParseHexColor(const std::string& text) {
    if (text.size() != 7 || text[0] != '#') {
        return std::nullopt;
    }
    unsigned value = 0;
    for (std::size_t i = 1; i < text.size(); ++i) {
        const char c = text[i];
        value <<= 4;
        if (c >= '0' && c <= '9') {
            value |= static_cast<unsigned>(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            value |= static_cast<unsigned>(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            value |= static_cast<unsigned>(c - 'A' + 10);
        } else {
            return std::nullopt;
        }
    }
    return RGB((value >> 16) & 0xff, (value >> 8) & 0xff, value & 0xff);
}

void ApplyVisibility(HWND hwnd, OverlayState& state, bool visible);

// Сообщение от интерфейса. Весь дизайн окна живёт в web: размер, место
// на экране, фон, клавиша. C++ только применяет то, что прислали, и
// проверяет границы — интерфейс не должен суметь сломать окно или игру.
//
//   {"type":"overlay/config", "width":2240, "height":1280,
//    "maxScreenShare":0.9, "anchor":"center"|"top", "offsetY":0,
//    "background":"#12161d", "toggleKey":34}
//   {"type":"overlay/hide"}
void HandleWebMessage(HWND hwnd, OverlayState& state, const std::string& json_text) {
    const nlohmann::json message = nlohmann::json::parse(json_text, nullptr, false);
    if (message.is_discarded() || !message.is_object() || !message.contains("type") ||
        !message["type"].is_string()) {
        Log("сообщение от интерфейса не разобралось");
        return;
    }
    const std::string type = message["type"].get<std::string>();

    if (type == "overlay/hide") {
        if (state.visible) {
            state.visible = false;
            ApplyVisibility(hwnd, state, false);
        }
        return;
    }
    if (type != "overlay/config") {
        Log(std::format("неизвестное сообщение от интерфейса: {}", type));
        return;
    }

    const auto number = [&](const char* key) -> std::optional<double> {
        if (message.contains(key) && message[key].is_number()) {
            return message[key].get<double>();
        }
        return std::nullopt;
    };

    if (const auto width = number("width")) {
        state.width = std::clamp(static_cast<int>(*width), 480, 7680);
    }
    if (const auto height = number("height")) {
        state.height = std::clamp(static_cast<int>(*height), 320, 4320);
    }
    if (const auto share = number("maxScreenShare")) {
        state.max_screen_share = std::clamp(*share, 0.3, 1.0);
    }
    if (const auto offset = number("offsetY")) {
        state.offset_y = std::clamp(static_cast<int>(*offset), -2000, 2000);
    }
    if (message.contains("anchor") && message["anchor"].is_string()) {
        state.anchor = message["anchor"].get<std::string>() == "top" ? Anchor::Top
                                                                     : Anchor::Center;
    }
    if (message.contains("background") && message["background"].is_string()) {
        if (const auto color = ParseHexColor(message["background"].get<std::string>())) {
            state.background = *color;
            // Кисть класса — то, чем окно закрашено до первого кадра WebView2.
            // Старая кисть не удаляется: она принадлежала классу с запуска,
            // а утечка одной кисти за сеанс дешевле гонки с перерисовкой.
            SetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND,
                             reinterpret_cast<LONG_PTR>(CreateSolidBrush(*color)));
            ComPtr<ICoreWebView2Controller2> controller2;
            if (state.controller &&
                SUCCEEDED(state.controller->QueryInterface(IID_PPV_ARGS(&controller2)))) {
                controller2->put_DefaultBackgroundColor(
                    {255, GetRValue(*color), GetGValue(*color), GetBValue(*color)});
            }
        }
    }
    if (const auto key = number("toggleKey")) {
        const UINT vk = static_cast<UINT>(*key);
        if (IsAllowedToggleKey(vk)) {
            g_toggle_key.store(vk);
        } else {
            Log(std::format("клавиша {} для панели не разрешена, остаётся прежняя", vk));
        }
    }

    Log(std::format("настройки окна от интерфейса: {}x{}, доля экрана {:.2f}", state.width,
                    state.height, state.max_screen_share));

    // Панель уже на экране — переставить сразу, а не при следующем показе.
    if (state.visible) {
        const RECT place = Placement(state);
        SetWindowPos(hwnd, HWND_TOPMOST, place.left, place.top, place.right - place.left,
                     place.bottom - place.top, SWP_NOACTIVATE);
    }
}

// "http://127.0.0.1:8777/" -> "http://127.0.0.1:8777". Origin — это схема,
// хост и порт; путь и всё после него к источнику сообщения не относятся.
std::wstring OriginOf(const std::wstring& url) {
    const std::size_t scheme = url.find(L"://");
    if (scheme == std::wstring::npos) {
        return url;
    }
    const std::size_t path = url.find(L'/', scheme + 3);
    return path == std::wstring::npos ? url : url.substr(0, path);
}

// Показать или спрятать панель.
//
// Показанная панель забирает фокус целиком: клавиатура и мышь принадлежат
// ей, а не игре. Это осознанный выбор вместо сквозных кликов — с панелью
// работают, когда она открыта, и прячут, когда она мешает. Полумеры
// («видна, но не кликается») только путают.
//
// put_IsVisible обязателен, и это не перестраховка: контроллер WebView2,
// созданный при скрытом окне, остаётся невидимым сам по себе и не следит
// за ShowWindow. Без этой строки окно появляется пустым — виден только
// сплошной фон окна.
void ApplyVisibility(HWND hwnd, OverlayState& state, bool visible) {
    if (!visible) {
        ShowWindow(hwnd, SW_HIDE);
        if (state.controller) {
            state.controller->put_IsVisible(FALSE);
        }
        NotifyPage(state, "overlay/hidden");
        Log("панель спрятана");
        return;
    }

    // Положение считается при КАЖДОМ показе, а не один раз при создании:
    // разрешение меняется при выходе из игры и обратно.
    const RECT place = Placement(state);
    const int x = place.left;
    const int y = place.top;
    const int width = place.right - place.left;
    const int height = place.bottom - place.top;

    // Поверх полноэкранной игры одного ShowWindow мало. SetForegroundWindow
    // Windows выполняет только у процесса, которому и так принадлежит
    // передний план; у всех остальных он молча проваливается — это защита
    // от выскакивающих окон. Обход штатный: на миг присоединить свой поток
    // ввода к потоку активного окна, тогда права переднего плана общие.
    SetWindowPos(hwnd, HWND_TOPMOST, x, y, width, height, SWP_SHOWWINDOW);

    const HWND foreground = GetForegroundWindow();
    const DWORD foreground_thread = GetWindowThreadProcessId(foreground, nullptr);
    const DWORD this_thread = GetCurrentThreadId();

    if (foreground_thread != this_thread) {
        AttachThreadInput(foreground_thread, this_thread, TRUE);
    }
    const BOOL brought = SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    SetFocus(hwnd);
    if (foreground_thread != this_thread) {
        AttachThreadInput(foreground_thread, this_thread, FALSE);
    }

    Log(std::format("панель показана: SetForegroundWindow={} передний план был у потока {}",
                    brought ? "ок" : "ОТКАЗ", foreground_thread));

    if (!state.controller) {
        Log("контроллер WebView2 ещё не создан — окно будет пустым");
        return;
    }
    state.controller->put_IsVisible(TRUE);
    RECT bounds{};
    GetClientRect(hwnd, &bounds);
    state.controller->put_Bounds(bounds);
    // Фокус клавиатуры внутрь страницы: без этого Escape и клики
    // по кнопкам до интерфейса не доходят.
    state.controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
    NotifyPage(state, "overlay/shown");
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    OverlayState* state = StateOf(hwnd);

    switch (message) {
        case WM_SIZE:
            if (state && state->controller) {
                RECT bounds{};
                GetClientRect(hwnd, &bounds);
                state->controller->put_Bounds(bounds);
            }
            return 0;



        // Оба входа ведут в одно действие: хук клавиатуры (основной путь,
        // работает в игре) и системная горячая клавиша (запасной, если хук
        // не установился).
        case WM_APP_TOGGLE_PANEL:
        case WM_HOTKEY:
            Log(message == WM_HOTKEY ? "PgDn: системная горячая клавиша"
                                     : "PgDn: хук клавиатуры");
            if (state) {
                state->visible = !state->visible;
                ApplyVisibility(hwnd, *state, state->visible);
            }
            return 0;

        // Закрытие окна не завершает программу: сервер данных должен
        // продолжать работать, чтобы PgDn открыл панель мгновенно.
        case WM_CLOSE:
            if (state) {
                state->visible = false;
                ApplyVisibility(hwnd, *state, false);
            }
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

}  // namespace

int RunOverlay(const OverlayOptions& options) {
    // WebView2 — это COM.
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    OverlayState state;
    state.width = options.width;
    state.height = options.height;
    state.allowed_origin = OriginOf(options.url);

    const HINSTANCE instance = GetModuleHandleW(nullptr);

    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = CreateSolidBrush(kPanelBackground);
    window_class.lpszClassName = L"SintenceOverlay";
    if (RegisterClassExW(&window_class) == 0) {
        std::println("RegisterClassExW не прошёл: код {}", GetLastError());
        return 1;
    }

    // Окно НЕ layered: у layered-окна с цветовым ключом Windows пропускает
    // мышь сквозь пиксели ключа, и клик по прозрачному участку уходит
    // в игру. Обойти это нельзя — hit-test считается по видимой области
    // слоя раньше, чем окно получает WM_NCHITTEST. Панель непрозрачна
    // целиком, поэтому все клики достаются ей.
    //
    // WS_EX_TOPMOST    — поверх игры;
    // WS_EX_TOOLWINDOW — нет в Alt+Tab и на панели задач.
    //
    // WS_EX_NOACTIVATE здесь НЕТ намеренно: открытая панель должна получать
    // фокус и клавиатуру. Пока он стоял, окно не активировалось, и все
    // нажатия продолжали уходить в игру.
    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        window_class.lpszClassName, options.title.c_str(), WS_POPUP,
        // Координаты здесь неважны: панель центрируется при каждом показе.
        0, 0, options.width, options.height,
        nullptr, nullptr, instance, nullptr);
    if (hwnd == nullptr) {
        std::println("не удалось создать окно: код {}", GetLastError());
        return 1;
    }

    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&state));

    // Окно создаётся скрытым: ShowWindow здесь нет намеренно.
    // Первое появление — по PgDn.

    const bool elevated = IsElevated();
    Log(std::format("--- запуск, права администратора: {}", elevated ? "есть" : "НЕТ"));
    if (!elevated) {
        std::println("ВНИМАНИЕ: нет прав администратора — панель не поднимется "
                     "поверх игры");
    }

    // Основной способ поймать PgDn — хук: он работает и когда активна игра.
    g_overlay_window = hwnd;
    g_keyboard_hook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHook, instance, 0);
    if (g_keyboard_hook == nullptr) {
        Log(std::format("хук клавиатуры не встал, код {}", GetLastError()));
    } else {
        Log("хук клавиатуры установлен");
    }

    // Запасной путь на случай, если хук не встал: системная горячая клавиша.
    // В игре она молчит (RIDEV_NOHOTKEYS), но вне игры выручит.
    // MOD_NOREPEAT — иначе зажатая клавиша переключает панель десятки раз.
    if (g_keyboard_hook == nullptr &&
        !RegisterHotKey(hwnd, kHotkeyVisibilityId, MOD_NOREPEAT, VK_NEXT)) {
        Log(std::format("RegisterHotKey(PgDn) тоже не прошёл, код {}", GetLastError()));
        std::println("PgDn перехватить нечем — панель не открыть");
    }

    std::println("PgDn — показать или спрятать панель (клавишу может сменить интерфейс)");
    std::println("журнал событий: overlay.log рядом с exe");

    const std::wstring url = options.url;

    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hwnd, url](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
                if (FAILED(result) || environment == nullptr) {
                    std::println("WebView2 не поднялся: рантайм не установлен?");
                    PostMessageW(hwnd, WM_CLOSE, 0, 0);
                    return S_OK;
                }
                environment->CreateCoreWebView2Controller(
                    hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [hwnd, url](HRESULT, ICoreWebView2Controller* controller) -> HRESULT {
                            if (controller == nullptr) {
                                return S_OK;
                            }
                            OverlayState* state = StateOf(hwnd);
                            state->controller = controller;

                            // Непрозрачный фон WebView того же цвета,
                            // что и фон окна: прозрачность больше не нужна,
                            // а сквозь неё было бы видно фон окна при
                            // перерисовке.
                            ComPtr<ICoreWebView2Controller2> controller2;
                            if (SUCCEEDED(controller->QueryInterface(IID_PPV_ARGS(&controller2)))) {
                                COREWEBVIEW2_COLOR panel = {255, 0x12, 0x16, 0x1d};
                                controller2->put_DefaultBackgroundColor(panel);
                            }

                            RECT bounds{};
                            GetClientRect(hwnd, &bounds);
                            controller->put_Bounds(bounds);

                            ComPtr<ICoreWebView2> webview;
                            controller->get_CoreWebView2(&webview);
                            state->webview = webview;

                            // Сообщения интерфейса: настройки окна и «спрячь меня».
                            // Принимаются только со своего origin.
                            EventRegistrationToken token{};
                            webview->add_WebMessageReceived(
                                Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                    [hwnd](ICoreWebView2*,
                                           ICoreWebView2WebMessageReceivedEventArgs* args)
                                        -> HRESULT {
                                        OverlayState* state = StateOf(hwnd);
                                        if (state == nullptr) {
                                            return S_OK;
                                        }
                                        LPWSTR source = nullptr;
                                        args->get_Source(&source);
                                        const std::wstring from = source ? source : L"";
                                        CoTaskMemFree(source);
                                        if (!state->allowed_origin.empty() &&
                                            OriginOf(from) != state->allowed_origin) {
                                            Log("сообщение с чужого origin отброшено");
                                            return S_OK;
                                        }
                                        LPWSTR json = nullptr;
                                        if (SUCCEEDED(args->get_WebMessageAsJson(&json))) {
                                            HandleWebMessage(hwnd, *state, ToUtf8(json));
                                        }
                                        CoTaskMemFree(json);
                                        return S_OK;
                                    })
                                    .Get(),
                                &token);

                            ComPtr<ICoreWebView2Settings> settings;
                            webview->get_Settings(&settings);
                            settings->put_AreDefaultContextMenusEnabled(FALSE);
                            settings->put_IsStatusBarEnabled(FALSE);


                            webview->Navigate(url.c_str());
                            return S_OK;
                        })
                        .Get());
                return S_OK;
            })
            .Get());

    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (g_keyboard_hook != nullptr) {
        UnhookWindowsHookEx(g_keyboard_hook);
        g_keyboard_hook = nullptr;
    }
    UnregisterHotKey(hwnd, kHotkeyVisibilityId);
    g_overlay_window = nullptr;
    CoUninitialize();
    return 0;
}

}  // namespace sintence
