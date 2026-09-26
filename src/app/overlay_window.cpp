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
#include <chrono>
#include <format>
#include <fstream>
#include <print>
#include <string>

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace sintence {

namespace {

// Фон окна. Совпадает с --panel из web/src/styles.css: пока WebView2
// не отрисовал первый кадр, видно именно его, и другой цвет дал бы
// вспышку при каждом показе панели.
constexpr COLORREF kPanelBackground = RGB(0x12, 0x16, 0x1d);
constexpr int kHotkeyVisibilityId = 1;

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
        if (key->vkCode == VK_NEXT && g_overlay_window != nullptr) {
            PostMessageW(g_overlay_window, WM_APP_TOGGLE_PANEL, 0, 0);
            // Клавиша съедается: игра и другие программы её не увидят,
            // иначе PgDn заодно пролистывал бы всё, что под панелью.
            return 1;
        }
    }
    return CallNextHookEx(g_keyboard_hook, code, wparam, lparam);
}

struct OverlayState {
    ComPtr<ICoreWebView2Controller> controller;
    // Окно стартует скрытым: оверлей нужен по требованию, а не постоянно.
    bool visible = false;
    // Перекрывается значениями из OverlayOptions при запуске.
    int width = 2240;
    int height = 1280;
};

OverlayState* StateOf(HWND hwnd) {
    return reinterpret_cast<OverlayState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
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
        Log("панель спрятана");
        return;
    }

    // Панель всегда открывается по центру экрана — положение не запоминается
    // и не перетаскивается. Считаем центр при КАЖДОМ показе, а не один раз
    // при создании: разрешение меняется при выходе из игры и обратно.
    const int screen_width = GetSystemMetrics(SM_CXSCREEN);
    const int screen_height = GetSystemMetrics(SM_CYSCREEN);

    // Запрошенный размер обрезается по экрану: на 1920x1080 панель
    // в 2240 точек шириной уехала бы за край, и половину карточек
    // стало бы не видно вовсе. Девяносто процентов — чтобы панель
    // читалась как окно поверх игры, а не как второй полный экран.
    const int width = std::min(state.width, screen_width * 9 / 10);
    const int height = std::min(state.height, screen_height * 9 / 10);
    const int x = (screen_width - width) / 2;
    const int y = (screen_height - height) / 2;

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

    std::println("PgDn — показать или спрятать панель, Esc — спрятать");
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
