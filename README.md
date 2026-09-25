# Sintence — статистика League of Legends на своём компьютере

Десктопное приложение для Windows: показывает табло идущего матча оверлеем
поверх игры и считает статистику по собственной истории матчей.

Всё работает локально. Данные идущего матча берутся у игрового клиента
(Live Client Data API, `127.0.0.1:2999`, ключ не нужен), история матчей —
из Riot API. Ни серверов, ни аккаунтов, ни телеметрии: файлы лежат на диске
пользователя, ключ Riot не покидает машину.

> Sintence isn't endorsed by Riot Games and doesn't reflect the views or
> opinions of Riot Games or anyone officially involved in producing or managing
> Riot Games properties. Riot Games and all associated properties are trademarks
> or registered trademarks of Riot Games, Inc.

**Личный проект, не open source.** Исходный код открыт для просмотра, но
использование, копирование, изменение и распространение требуют письменного
разрешения — подробности в `LICENSE`.

## Что уже работает

**Оверлей идущего матча.** `PgDn` показывает и прячет панель по центру экрана:
время матча, режим, карта, обе команды карточками — чемпион, Riot ID, роль,
уровень, K/D/A, фарм и CS в минуту. Обновляется раз в секунду. Повторное
нажатие `PgDn` убирает панель, выход из программы — `Ctrl+C` в консоли.

**Разбор истории матчей.** Match-V5 из локальной выгрузки: агрегация по
чемпионам, winrate, KDA, CS/min, топы и фильтры по объёму выборки.

Показывается только то, что игрок и так видит на экране. Скрытого состояния
противника — кулдаунов, позиций на карте — Live Client Data API не отдаёт,
и приложение на нём ничего не строит (`project/POLICY.md`).

## Как устроено

```
League (127.0.0.1:2999)
        |  HTTPS, без ключа
        v
  LiveClientSource  ──►  LiveGame  ──►  HTTP-сервер 127.0.0.1:8777
  (src/data)             (src/core)     (src/app)
                                              |  JSON
                                              v
                                    Vue 3 в окне WebView2
                                              (web/)
```

Слои и зависимости строго вниз: `app/ → analysis/ → core/`, `app/ → data/ → core/`.
Ни один тип сторонней библиотеки (nlohmann, cpp-httplib, WebView2) не попадает
в `core/` и `analysis/` — они видны только внутри `data/*.cpp` и `app/*.cpp`.
Поэтому источник данных меняется без правки анализатора: за интерфейсом
`MatchSource` одинаково живут файлы, Riot API и фикстуры тестов.

| Каталог | Что внутри |
|---|---|
| `src/core/` | доменные типы: `Champion`, `ChampionReport`, `ChampionIndex`, `LiveGame` |
| `src/data/` | адаптеры: чтение файлов, разбор JSON, Live Client Data API |
| `src/analysis/` | метрики и агрегация над типами `core/` |
| `src/app/` | точка входа, локальный HTTP-сервер, окно оверлея |
| `web/` | интерфейс: Vue 3 + Vite + TypeScript |
| `tests/` | весь набор тестов одним бинарником `sintence_tests` |
| `project/` | решения по архитектуре, ограничения Riot, схема данных и фикстуры |

## Сборка

Нужны: **Visual Studio 2026** (MSVC 14.51+, C++23), **CMake ≥ 3.20**,
**Node.js 20+**, **vcpkg**. WebView2 Runtime входит в Windows 11.

Зависимости C++ ставятся сами по `vcpkg.json` при конфигурации:
cpp-httplib (HTTP-клиент и сервер), OpenSSL, WebView2 SDK.
`doctest` и `nlohmann/json` лежат заголовками в `third_party/`.

```powershell
# интерфейс
cd web
npm install
npm run build
cd ..

# приложение
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<путь к vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Debug
```

Запуск — **от имени администратора**: без повышения прав Windows не поднимает
окно поверх игры с анти-читом.

```powershell
build\src\Debug\sintence.exe
```

В игре должен стоять режим окна **«Без рамки»**: поверх эксклюзивного
полноэкранного режима не отрисовывается ни одно обычное окно.

Собрать без запроса UAC (для отладки, оверлей поверх игры при этом не работает):

```powershell
cmake -S . -B build -DSINTENCE_REQUIRE_ADMIN=OFF ...
```

## Разработка интерфейса

Пересобирать C++ ради правки CSS не нужно:

```powershell
build\src\Debug\sintence.exe   # отдаёт /api/live на порту 8777
cd web && npm run dev          # http://localhost:5173, горячая перезагрузка
```

Vite проксирует `/api` в приложение, поэтому страница в браузере показывает
настоящие данные идущего матча. Подробности — в `web/README.md`.

## Тесты

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

Отдельная группа — флагами doctest, без пересборки:

```powershell
build\tests\Debug\sintence_tests.exe --source-file="*live_client*"
build\tests\Debug\sintence_tests.exe --test-case="*winrate*"
build\tests\Debug\sintence_tests.exe --list-test-cases
```

Тесты не требуют ни сети, ни ключа, ни запущенной игры: они работают на
фикстурах из `project/data/fixtures/`. Debug собирается с address sanitizer.

## Ключ Riot

Ключ нужен только для истории матчей; оверлей работает без него.

Ключ **никогда не попадает в репозиторий и в собранный бинарник**: он живёт
в файле профиля пользователя и читается при запуске. Выгрузка матчей в локальные
файлы делается скриптом `scripts/fetch_matches.py` (только стандартная библиотека
Python, ни pip, ни зависимостей).

Ограничения Riot, которые соблюдает приложение, — в `project/POLICY.md`.

## Статус

Приложение в активной разработке. Готово: оверлей на живых данных, разбор
Match-V5 и Live Client Data, агрегация по чемпионам. В работе: запросы к Riot API
из самого приложения, кеш и соблюдение лимитов, история игроков текущего матча.
