#include "profile_window.h"

#include <dwmapi.h>
#include <wrl.h>

#include <algorithm>

#include "app_log.h"

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace sintence {

namespace {

// Тот же фон, что у интерфейса (--bg в sintence-web): окно не мигает
// белым, пока WebView2 рисует первый кадр. Для WebView2 он повторён
// байтами ниже: GetRValue на constexpr даёт предупреждение C4310.
constexpr COLORREF kBackground = RGB(0x12, 0x16, 0x1d);

// Тёмный заголовок окна. Константа есть в dwmapi.h только в новых SDK,
// значение стабильно с Windows 10 20H1.
constexpr DWORD kDwmUseImmersiveDarkMode = 20;

struct ProfileState {
    std::wstring url;
    std::function<void()> on_hide;
    ComPtr<ICoreWebView2Controller> controller;
};

ProfileState* StateOf(HWND hwnd) {
    return reinterpret_cast<ProfileState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

void FitWebView(HWND hwnd) {
    const ProfileState* state = StateOf(hwnd);
    if (state == nullptr || !state->controller) {
        return;
    }
    RECT bounds{};
    GetClientRect(hwnd, &bounds);
    state->controller->put_Bounds(bounds);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_SIZE:
            FitWebView(hwnd);
            if (ProfileState* state = StateOf(hwnd); state && state->controller) {
                // Свёрнутое окно — WebView2 можно не рисовать.
                state->controller->put_IsVisible(wparam != SIZE_MINIMIZED);
            }
            return 0;

        // Не меньше, чем влезает список игр рядом со сводкой.
        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
            info->ptMinTrackSize = {960, 640};
            return 0;
        }

        case WM_SETFOCUS:
            if (ProfileState* state = StateOf(hwnd); state && state->controller) {
                state->controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
            }
            return 0;

        // Крестик — в трей. WebView2 перестаёт рисовать, страница видит
        // visibilityState = hidden и реже опрашивает сервер.
        case WM_CLOSE:
            ShowWindow(hwnd, SW_HIDE);
            if (ProfileState* state = StateOf(hwnd)) {
                if (state->controller) {
                    state->controller->put_IsVisible(FALSE);
                }
                if (state->on_hide) {
                    state->on_hide();
                }
            }
            Log("окно статистики спрятано в трей");
            return 0;

        // Сюда доходит только выход из меню трея (DestroyWindow): цикл
        // сообщений в RunOverlay выходит по WM_QUIT.
        case WM_DESTROY:
            if (ProfileState* state = StateOf(hwnd)) {
                if (state->controller) {
                    state->controller->Close();
                }
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                delete state;
            }
            PostQuitMessage(0);
            return 0;

        default:
            break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

}  // namespace

HWND CreateProfileWindow(HINSTANCE instance, const ProfileWindowOptions& options) {
    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = CreateSolidBrush(kBackground);
    window_class.hIcon = options.icon;
    window_class.hIconSm = options.small_icon;
    window_class.lpszClassName = L"SintenceProfile";
    if (RegisterClassExW(&window_class) == 0) {
        LogError("окно профиля: RegisterClassExW не прошёл, код {}", GetLastError());
        return nullptr;
    }

    // По центру рабочей области (без панели задач), не больше 90% от неё.
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int work_width = work.right - work.left;
    const int work_height = work.bottom - work.top;
    const int width = std::min(options.width, work_width * 9 / 10);
    const int height = std::min(options.height, work_height * 9 / 10);
    const int x = work.left + (work_width - width) / 2;
    const int y = work.top + (work_height - height) / 2;

    HWND hwnd = CreateWindowExW(0, window_class.lpszClassName, options.title.c_str(),
                                WS_OVERLAPPEDWINDOW, x, y, width, height, nullptr, nullptr,
                                instance, nullptr);
    if (hwnd == nullptr) {
        LogError("окно профиля не создалось: код {}", GetLastError());
        return nullptr;
    }
    SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(new ProfileState{options.url, options.on_hide, nullptr}));

    const BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, kDwmUseImmersiveDarkMode, &dark, sizeof(dark));

    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);
    return hwnd;
}

void AttachProfileWebView(HWND hwnd, ICoreWebView2Environment* environment) {
    if (hwnd == nullptr || environment == nullptr) {
        return;
    }
    environment->CreateCoreWebView2Controller(
        hwnd, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                  [hwnd](HRESULT, ICoreWebView2Controller* controller) -> HRESULT {
                      ProfileState* state = StateOf(hwnd);
                      // Окно успели закрыть, пока WebView2 поднимался.
                      if (controller == nullptr || state == nullptr) {
                          return S_OK;
                      }
                      state->controller = controller;

                      ComPtr<ICoreWebView2Controller2> controller2;
                      if (SUCCEEDED(controller->QueryInterface(IID_PPV_ARGS(&controller2)))) {
                          controller2->put_DefaultBackgroundColor({255, 0x12, 0x16, 0x1d});
                      }
                      FitWebView(hwnd);

                      ComPtr<ICoreWebView2> webview;
                      controller->get_CoreWebView2(&webview);
                      ComPtr<ICoreWebView2Settings> settings;
                      webview->get_Settings(&settings);
                      settings->put_AreDefaultContextMenusEnabled(FALSE);
                      settings->put_IsStatusBarEnabled(FALSE);

                      webview->Navigate(state->url.c_str());
                      if (GetForegroundWindow() == hwnd) {
                          controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
                      }
                      return S_OK;
                  })
                  .Get());
}

void ShowProfileWindow(HWND hwnd) {
    if (hwnd == nullptr) {
        return;
    }
    ShowWindow(hwnd, IsIconic(hwnd) ? SW_RESTORE : SW_SHOW);
    if (ProfileState* state = StateOf(hwnd); state && state->controller) {
        state->controller->put_IsVisible(TRUE);
        FitWebView(hwnd);
    }
    SetForegroundWindow(hwnd);
    Log("окно статистики показано");
}

}  // namespace sintence
