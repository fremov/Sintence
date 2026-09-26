#include "tray_icon.h"

#include <shellapi.h>

#include <cwchar>

#include "app_log.h"

namespace sintence {

namespace {

// Строку в фиксированный буфер NOTIFYICONDATAW, с обрезкой.
template <std::size_t N>
void Copy(wchar_t (&target)[N], const std::wstring& text) {
    wcsncpy_s(target, N, text.c_str(), _TRUNCATE);
}

}  // namespace

TrayIcon::~TrayIcon() {
    Remove();
}

bool TrayIcon::Add(HWND owner, UINT callback_message, HICON icon, const std::wstring& tooltip) {
    data_ = {};
    data_.cbSize = sizeof(data_);
    data_.hWnd = owner;
    data_.uID = 1;
    data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    data_.uCallbackMessage = callback_message;
    data_.hIcon = icon;
    Copy(data_.szTip, tooltip);

    // После перезапуска проводника старого значка уже нет — удаление
    // просто ничего не найдёт.
    Shell_NotifyIconW(NIM_DELETE, &data_);
    added_ = Shell_NotifyIconW(NIM_ADD, &data_) != FALSE;
    if (!added_) {
        LogError("трей: значок не поставился, код {}", GetLastError());
        return false;
    }
    data_.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data_);
    return true;
}

void TrayIcon::Remove() {
    if (added_) {
        Shell_NotifyIconW(NIM_DELETE, &data_);
        added_ = false;
    }
}

void TrayIcon::Notify(const std::wstring& title, const std::wstring& text) {
    if (!added_) {
        return;
    }
    NOTIFYICONDATAW info = data_;
    info.uFlags = NIF_INFO;
    info.dwInfoFlags = NIIF_USER | NIIF_LARGE_ICON;
    info.hBalloonIcon = data_.hIcon;
    Copy(info.szInfoTitle, title);
    Copy(info.szInfo, text);
    Shell_NotifyIconW(NIM_MODIFY, &info);
}

void TrayIcon::ShowMenu(bool has_profile, bool overlay_visible, bool overlay_available) {
    HMENU menu = CreatePopupMenu();
    if (menu == nullptr) {
        return;
    }
    if (has_profile) {
        AppendMenuW(menu, MF_STRING, kTrayShowProfile, L"Статистика");
    }
    if (overlay_visible) {
        AppendMenuW(menu, MF_STRING, kTrayToggleOverlay, L"Спрятать оверлей\tPgDn");
    } else if (overlay_available) {
        AppendMenuW(menu, MF_STRING, kTrayToggleOverlay, L"Показать оверлей\tPgDn");
    } else {
        AppendMenuW(menu, MF_STRING | MF_GRAYED, kTrayToggleOverlay,
                    L"Оверлей — с выбора чемпиона");
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kTrayExit, L"Выход");
    SetMenuDefaultItem(menu, has_profile ? kTrayShowProfile : kTrayToggleOverlay, FALSE);

    // Без SetForegroundWindow меню не закрывается щелчком мимо него,
    // а WM_NULL после — известная поправка из документации Shell_NotifyIcon.
    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(data_.hWnd);
    TrackPopupMenuEx(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, cursor.x, cursor.y, data_.hWnd,
                     nullptr);
    PostMessageW(data_.hWnd, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

}  // namespace sintence
