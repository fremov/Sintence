# ---------------------------------------------------------------------------
# sintence_lib — весь код анализатора, кроме точки входа.
# Слои по project/ARCHITECTURE.md:
#   core/     доменные типы, ничего про файлы, JSON и сеть
#   data/     адаптеры источников
#   analysis/ метрики и агрегация над типами core/
# Приложение и тесты линкуются с этой библиотекой, а не пересобирают исходники.
# ---------------------------------------------------------------------------

# Заголовки перечисляются наравне с .cpp. Компилятору они не нужны — он видит
# их через #include, — но мишень описывает состав проекта, и IDE рисует дерево
# именно по ней. Без этой строки в Rider и Visual Studio видно одни .cpp.
file(GLOB SINTENCE_CORE_SOURCES CONFIGURE_DEPENDS "core/*.cpp" "core/*.h")
file(GLOB SINTENCE_DATA_SOURCES CONFIGURE_DEPENDS "data/*.cpp" "data/*.h")
file(GLOB SINTENCE_ANALYSIS_SOURCES CONFIGURE_DEPENDS "analysis/*.cpp" "analysis/*.h")

add_library(sintence_lib STATIC
    ${SINTENCE_CORE_SOURCES}
    ${SINTENCE_DATA_SOURCES}
    ${SINTENCE_ANALYSIS_SOURCES})

# Папки в дереве IDE повторяют папки на диске, а не свалены в одну кучу.
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES
    ${SINTENCE_CORE_SOURCES}
    ${SINTENCE_DATA_SOURCES}
    ${SINTENCE_ANALYSIS_SOURCES})

target_include_directories(sintence_lib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/data"
    "${CMAKE_CURRENT_SOURCE_DIR}/analysis")

# MatchFile работает с fopen напрямую и владеет файлом сам.
# MSVC считает fopen устаревшим и требует fopen_s, которого нет в стандарте.
if(MSVC)
    target_compile_definitions(sintence_lib PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# cpp-httplib + OpenSSL — HTTP-клиент к Riot и локальный сервер.
# Ставится через vcpkg по манифесту vcpkg.json, собирать руками ничего не нужно.
# httplib::httplib тянет за собой OpenSSL и определяет CPPHTTPLIB_OPENSSL_SUPPORT.
find_package(httplib CONFIG REQUIRED)

target_link_libraries(sintence_lib PUBLIC sintence_common httplib::httplib)

# SQLite — история матчей игроков (data/sqlite_match_store) до переезда на
# сервер. PRIVATE: sqlite3.h видит только адаптер, наружу он не выходит.
find_package(unofficial-sqlite3 CONFIG REQUIRED)
target_link_libraries(sintence_lib PRIVATE unofficial::sqlite3::sqlite3)

# Схема истории встраивается в бинарник из project/data/history_schema.sql:
# .sql-файл — единственный источник, он же потом уедет на сервер.
set(SINTENCE_SCHEMA_FILE "${CMAKE_SOURCE_DIR}/project/data/history_schema.sql")
file(READ "${SINTENCE_SCHEMA_FILE}" SINTENCE_HISTORY_SCHEMA)
configure_file(data/history_schema.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/history_schema.h" @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SINTENCE_SCHEMA_FILE}")
target_include_directories(sintence_lib PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
set_target_properties(sintence_lib PROPERTIES FOLDER "sintence")
set_target_properties(sintence_lib PROPERTIES VS_GLOBAL_VcpkgEnabled false)

# ---------------------------------------------------------------------------
# sintence — само приложение.
# ---------------------------------------------------------------------------

# WebView2 — окно оверлея. Ставится тем же манифестом vcpkg.json.
find_package(unofficial-webview2 CONFIG REQUIRED)

add_executable(sintence
    app/main.cpp
    app/live_api_server.cpp
    app/live_api_server.h
    app/overlay_window.cpp
    app/overlay_window.h
    app/profile_window.cpp
    app/profile_window.h
    app/tray_icon.cpp
    app/tray_icon.h
    app/resource.h
    app/sintence.rc
    app/sintence.manifest)

# Оконное приложение: без консоли. Точка входа остаётся main —
# /ENTRY:mainCRTStartup вместо WinMain, которого требовала бы подсистема
# WINDOWS по умолчанию. Всё, что раньше печаталось в консоль, пишется
# в журнал %LOCALAPPDATA%\Sintence\logs\sintence.log (data/app_log).
set_target_properties(sintence PROPERTIES WIN32_EXECUTABLE TRUE)
if(MSVC)
    target_link_options(sintence PRIVATE "/ENTRY:mainCRTStartup")
endif()

# Манифест встраивается в exe: совместимость с версиями Windows и DPI.
# Права администратора — отдельным флагом линкера, иначе MSVC и наш манифест
# спорят об атрибуте level (manifest authoring error c1010001).
#
# Зачем права: League работает с анти-читом Vanguard от администратора,
# и Windows не пускает процесс с меньшими правами ни к горячим клавишам,
# ни поверх его окна. Выключается ради отладки без запроса UAC:
#   cmake -S . -B build-noadmin -DSINTENCE_REQUIRE_ADMIN=OFF ...
option(SINTENCE_REQUIRE_ADMIN "Требовать права администратора для sintence.exe" ON)

if(MSVC AND SINTENCE_REQUIRE_ADMIN)
    target_link_options(sintence PRIVATE
        "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'")
endif()

# dwmapi — тёмный заголовок окна профиля.
target_link_libraries(sintence PRIVATE sintence_lib unofficial::webview2::webview2 dwmapi)
set_target_properties(sintence PROPERTIES FOLDER "sintence")
set_target_properties(sintence PROPERTIES VS_GLOBAL_VcpkgEnabled false)

# Без UNICODE макросы Win32 вроде IDC_ARROW раскрываются в ANSI-вариант,
# и W-функции получают char* вместо wchar_t*.
target_compile_definitions(sintence PRIVATE UNICODE _UNICODE)

# Пути, зашитые в бинарник, чтобы запускать без аргументов:
#   SINTENCE_DEFAULT_DATA_DIR — выгрузка матчей для офлайн-режима
#   SINTENCE_WEB_DIR          — собранный интерфейс (репозиторий sintence-web,
#                               каталог dist/). По умолчанию — соседний с этим
#                               репозиторием ../sintence-web/dist; меняется
#                               при конфигурации -DSINTENCE_WEB_DIR=... или
#                               без пересборки переменной окружения того же имени
#   SINTENCE_PACKS_DIR        — паки предпочтений (scripts/build_pack.py);
#                               когда появится сервер, вместо каталога
#                               здесь будет URL, и больше ничего не изменится
get_filename_component(SINTENCE_WEB_DIR_DEFAULT
    "${CMAKE_SOURCE_DIR}/../sintence-web/dist" ABSOLUTE)
set(SINTENCE_WEB_DIR "${SINTENCE_WEB_DIR_DEFAULT}" CACHE PATH
    "Каталог собранного интерфейса (sintence-web/dist)")

target_compile_definitions(sintence PRIVATE
    SINTENCE_DEFAULT_DATA_DIR="${CMAKE_SOURCE_DIR}/project/data/matches"
    SINTENCE_WEB_DIR="${SINTENCE_WEB_DIR}"
    SINTENCE_PACKS_DIR="${CMAKE_SOURCE_DIR}/project/data/packs"
    SINTENCE_HISTORY_DB="${CMAKE_SOURCE_DIR}/project/data/history.sqlite")

if(SINTENCE_ASAN_DLLS)
    add_custom_command(TARGET sintence POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${SINTENCE_ASAN_DLLS} "$<TARGET_FILE_DIR:sintence>"
        VERBATIM)
endif()
