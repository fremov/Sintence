#ifndef SINTENCE_APP_PROFILE_WINDOW_H
#define SINTENCE_APP_PROFILE_WINDOW_H

#ifndef NOMINMAX
#define NOMINMAX  // см. overlay_window.cpp: иначе std::min ломается о макрос
#endif
#include <windows.h>

#include <WebView2.h>

#include <functional>
#include <string>

// app/profile_window — обычное окно приложения: профиль игрока, последние
// игры, роли и чемпионы. В отличие от оверлея оно не поверх игры,
// с рамкой, на панели задач и в Alt+Tab, двигается и меняет размер мышью.
//
// Окно — второе в том же процессе и на том же потоке, что и оверлей:
// общий цикл сообщений, общий сервер данных, общее окружение WebView2
// (один браузерный процесс на оба окна). Страница та же, что у оверлея,
// с маршрутом #/profile.
//
// Крестик окно не закрывает, а прячет в трей: PgDn продолжает открывать
// оверлей, окно возвращается щелчком по значку. Приложение завершается
// только пунктом «Выход» в меню значка (app/tray_icon).
//
// Сообщения от страницы окно не принимает: управлять им интерфейсу нечем,
// а overlay/config от этой страницы оверлей двигать не должен.

namespace sintence {

struct ProfileWindowOptions {
    std::wstring url;  // http://127.0.0.1:8777/#/profile
    std::wstring title = L"Sintence";
    int width = 1360;
    int height = 900;
    HICON icon = nullptr;        // заголовок окна и панель задач
    HICON small_icon = nullptr;
    // Окно спрятали крестиком — владелец трея может сказать об этом.
    std::function<void()> on_hide;
};

// Создаёт окно и показывает его по центру экрана (размер обрезается
// по экрану). nullptr — окно не создалось, причина в журнале.
HWND CreateProfileWindow(HINSTANCE instance, const ProfileWindowOptions& options);

// Поднимает WebView2 внутри окна на готовом окружении и открывает url.
void AttachProfileWebView(HWND hwnd, ICoreWebView2Environment* environment);

// Вернуть окно из трея: показать, развернуть, вывести на передний план.
void ShowProfileWindow(HWND hwnd);

}  // namespace sintence

#endif  // SINTENCE_APP_PROFILE_WINDOW_H
