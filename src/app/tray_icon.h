#ifndef SINTENCE_APP_TRAY_ICON_H
#define SINTENCE_APP_TRAY_ICON_H

#ifndef NOMINMAX
#define NOMINMAX  // см. overlay_window.cpp: иначе std::min ломается о макрос
#endif
#include <windows.h>

#include <string>

// app/tray_icon — значок Sintence в области уведомлений.
//
// Приложение живёт в трее: крестик окна статистики только прячет его,
// PgDn продолжает открывать оверлей. Полный выход — пункт «Выход» в меню
// значка (или диспетчер задач).
//
// События значка приходят окну-владельцу сообщением callback_message:
// LOWORD(lparam) — событие (NIN_SELECT — щелчок, WM_CONTEXTMENU — меню),
// так ведёт себя NOTIFYICON_VERSION_4.

namespace sintence {

// Команды меню значка: приходят владельцу как WM_COMMAND.
enum TrayCommand : UINT {
    kTrayShowProfile = 1,
    kTrayToggleOverlay = 2,
    kTrayExit = 3,
};

class TrayIcon {
public:
    TrayIcon() = default;
    ~TrayIcon();

    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    // Поставить значок. Повторный вызов (после перезапуска проводника,
    // сообщение TaskbarCreated) ставит его заново.
    bool Add(HWND owner, UINT callback_message, HICON icon, const std::wstring& tooltip);
    void Remove();

    // Всплывающее уведомление у значка (в Windows 11 — обычное уведомление).
    void Notify(const std::wstring& title, const std::wstring& text);

    // Меню у курсора. Команда приходит владельцу через WM_COMMAND.
    // has_profile — есть ли окно статистики (SINTENCE_NO_PROFILE его убирает).
    void ShowMenu(bool has_profile, bool overlay_visible);

private:
    NOTIFYICONDATAW data_{};
    bool added_ = false;
};

}  // namespace sintence

#endif  // SINTENCE_APP_TRAY_ICON_H
