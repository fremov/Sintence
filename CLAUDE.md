# CLAUDE.md

Sintence — десктопное приложение под Windows для League of Legends: оверлей с табло
живого матча, профили игроков лобби (ранг, мастери), советы активному игроку
по сборке в его матчапе и статистика по истории матчей. Пока всё работает
локально, без телеметрии; данные со временем переедут на сервер.

Отвечай и пиши файлы по-русски. Ключевые слова C++, имена типов и библиотек
остаются английскими.

## Документы, которые важнее этого файла

- `project/ARCHITECTURE.md` — принятые решения с причинами. Если решение мешает,
  правится этот файл, а не обходится молча в коде.
- `project/POLICY.md` — ограничения Riot. Обязательны для любой функции,
  которая работает с данными игры.
- `project/CHAMP_SELECT.md` — профили и советы до матча (LCU, spectator-v5).

Интерфейс — отдельный репозиторий `../sintence-web` (Vue 3 + Vite + TypeScript + zod),
со своим README. Контракт между ними — ARCHITECTURE.md §11.

## Сборка и тесты

MSVC (Visual Studio 2026, C++23), CMake ≥ 3.20, vcpkg (cpp-httplib + OpenSSL, WebView2, SQLite),
Node.js 20+. `doctest` и `nlohmann/json` лежат в `third_party/`.

```powershell
cd ..\sintence-web; npm install; npm run build; cd ..\Portfolio
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
build\tests\Debug\sintence_tests.exe --source-file="*rate_limiter*"   # одна группа
```

- Мишени: `sintence_lib` (весь `src/` кроме `app/`), `sintence` (приложение),
  `sintence_tests` (все тесты одним бинарником). Новые `.cpp`/`.h` в `core/`, `data/`,
  `analysis/` и новые `tests/test_*.cpp` подхватываются глобами; файлы `app/`
  перечисляются в `src/CMakeLists.txt` руками.
- Debug собирается с ASan. `clang_rt.asan*.dll` копируется к exe; без неё
  процесс падает с `0xc0000135` ещё до `main`.
- `sintence.exe` — оконное приложение без консоли, живёт в трее: крестик окна
  статистики прячет его, выход — меню значка. Одна копия на порт (второй запуск
  показывает окно первой). Всё, что раньше печаталось, — в журнале
  `%LOCALAPPDATA%\Sintence\logs\sintence.log` (`sintence-<порт>.log` для
  не 8777) через `data/app_log` (`Log`, `LogError`); `std::println` в `src/`
  не использовать. Данные WebView2 — в `%LOCALAPPDATA%\Sintence\WebView2`.
- `sintence.exe` требует прав администратора (иначе окно не встанет поверх игры
  с Vanguard). Для отладки без UAC: `-DSINTENCE_REQUIRE_ADMIN=OFF` в отдельный `build-noadmin`.
- Тесты не ходят в сеть, не требуют ключа и запущенной игры: только фикстуры
  из `project/data/fixtures/`.
- Интерфейс без пересборки C++: в `../sintence-web` — `npm run dev:mock` (без игры
  и без exe, моки и переключатель сценариев) или `npm run dev` (Vite проксирует
  `/api` на `127.0.0.1:8777`).
- Переменные окружения `sintence.exe`: `SINTENCE_WEB_DIR` (где собранный интерфейс,
  по умолчанию `../sintence-web/dist`), `SINTENCE_UI_URL` (показать в окне адрес,
  например живой Vite), `SINTENCE_PORT` (порт вместо 8777 — отладочная копия рядом
  с рабочей), `SINTENCE_HISTORY_DB` (база истории матчей вместо
  `project/data/history.sqlite`), `SINTENCE_NO_PROFILE=1` (без окна профиля,
  только оверлей). Рабочий `build\...\sintence.exe` часто запущен и держит exe и порт:
  отладочную сборку делай в `build-noadmin` и запускай с `SINTENCE_PORT=8778`.

## Архитектура

```
src/core/      доменные типы, ни JSON, ни HTTP, ни файлов
src/data/      адаптеры: файлы, Match-V5, Live Client, Riot API, пак предпочтений
src/analysis/  метрики — чистые функции над типами core/
src/app/       main, HTTP-сервер 127.0.0.1:8777, окна WebView2: оверлей и профиль
scripts/       Python (только stdlib): краулер Riot API -> SQLite -> пак
```

Правила, которые проверяются на каждом изменении:

- Зависимости только вниз: `app → analysis → core`, `app → data → core`.
- **Ни один тип фреймворка не попадает в `core/` и `analysis/`.** nlohmann виден
  только в `data/` и `app/` (через `json_helpers.h` он есть и в заголовках
  `match_json.h`, `live_client_json.h`); cpp-httplib — только в `data/*.cpp`
  и `app/*.cpp`; WebView2 — только в `app/`.
- Сервер слушает только `127.0.0.1`, никогда `0.0.0.0`.
- Ключ Riot не попадает в репозиторий, бинарник, журнал и фронтенд. Читается
  из `SINTENCE_RIOT_KEY` или `%LOCALAPPDATA%\Sintence\riot_key.txt`.
- Пак предпочтений приложение читает файлом: `project/data/packs/index.json`,
  затем пак, на который тот указывает. База краулера (`base.sqlite`) ему
  не видна.
- **Данные будут жить на сервере** (MySQL или другая СУБД), у пользователя —
  только программа. Сервера пока нет, и всё готовится к переезду: история
  матчей игроков — за интерфейсом `core/match_store.h`, сейчас это SQLite
  `project/data/history.sqlite` (`data/sqlite_match_store`, схема
  `project/data/history_schema.sql` — общее подмножество SQL). Новое
  хранилище — новый адаптер, а не правки вокруг. ARCHITECTURE.md §12.

Локальный API приложения: `/api/health` (версия контракта `kApiVersion`, включённые
функции), `/api/live` (табло), `/api/lobby` (выбор чемпиона из LCU, экран загрузки из
spectator-v5), `/api/profiles` (501 без ключа, без матча отдаёт готовые),
`/api/preferences` (советы активному игроку по табло, а до матча — по параметрам
`?champion=&role=&enemies=`; роль `NONE` заменяется самой частой по паку,
`roleSource: "pack"`), окно профиля — `/api/profile`, `/api/matches`,
`/api/matches/{id}[/timeline]` (история из `HistoryService`, 501 без ключа).

LCU (клиент League): чтение — `LobbyService`; запись — только `data/lcu_actions`
(страница рун и свои заклинания) и **только по клику** в интерфейсе, никогда
по событию (политика Riot). Эндпоинты записи `POST /api/champselect/runes|spells`
защищены `TrustedWrite`: заголовок `X-Sintence-Action`, `Host`, `Origin` — не
ослаблять. Пароль из lockfile не логируется; скрытые клиентом имена союзников
не восстанавливаются, противники в выборе чемпиона безымянны.
`SINTENCE_LOL_DIR` — каталог установки, если автопоиск не нашёл. Меняешь ответ — меняй zod-схему в `sintence-web` в том же заходе;
удаляешь поле или меняешь тип — поднимай `kApiVersion`.

Настройки окна оверлея (размер, якорь, фон, клавиша) задаёт интерфейс сообщением
`overlay/config` через `chrome.webview.postMessage`; C++ только проверяет границы.

## Граница Riot (кратко, полностью — POLICY.md)

Показываем только то, что игрок и так видит. Не показываем скрытое состояние
противника (кулдауны, таймеры, позиции), не действуем за игрока, ничего не внедряем
в процесс игры. Чужие руны и порядок прокачки Live Client не отдаёт — и на карточках
врагов их быть не должно, даже если их можно достать из истории матчей.

## Live Client Data API

`https://127.0.0.1:2999`, без ключа, только во время матча. Сертификат
самоподписанный Riot: его надо принять или отключить проверку.

| Путь | Что отдаёт |
|---|---|
| `/liveclientdata/allgamedata` | всё сразу: активный игрок, все игроки, события, данные игры |
| `/liveclientdata/activeplayer` | активный игрок целиком |
| `/liveclientdata/activeplayername` | только riotId |
| `/liveclientdata/activeplayerabilities` | Q/W/E/R/Passive и их уровни |
| `/liveclientdata/activeplayerrunes` | полная страница рун |
| `/liveclientdata/playerlist` | все 10 игроков, `?teamID=ALL\|ORDER\|CHAOS` |
| `/liveclientdata/playerscores?riotId=` | KDA, CS, wardScore |
| `/liveclientdata/playersummonerspells?riotId=` | заклинания призывателя |
| `/liveclientdata/playermainrunes?riotId=` | keystone и три дерева |
| `/liveclientdata/playeritems?riotId=` | инвентарь со стаками и ценой |
| `/liveclientdata/eventdata` | лента событий, `?eventID=` как курсор |
| `/gamestats` | gameMode, gameTime, mapName |

`LiveClientSource` ходит только в `allgamedata`: отдельные эндпоинты снимаются
в разные моменты и вместе дают противоречивый снимок. В режиме наблюдателя
и в реплее `activePlayer` приходит строкой с ошибкой вместо объекта.
`allPlayers[].runes` — ключевая руна и деревья у всех (видно по Tab),
`allPlayers[].summonerSpells` — ключ заклинания только внутри `rawDisplayName`.
В Practice Tool и пользовательских играх `position` = `"NONE"`, у ботов
Riot ID вида `Garen#BOT`, `rawChampionName` может отличаться регистром от Data
Dragon (`FiddleSticks`).

## Riot API

**Связывает лимит ключа, а не лимиты методов.** Personal-ключ: 20 запросов в секунду
и 100 за 2 минуты на все эндпоинты вместе. Скользящие окна, на 429 — пауза по
`Retry-After` и один повтор. C++: `data/rate_limiter` (`RiotPersonalKey()`),
Python: `scripts/riot_client.py` (с запасом 18/1 с и 95/120 с).

**Два типа хостов — главный источник 404 на правильный запрос.**
- Региональный (`europe`, `americas`, `asia`): `account-v1`, `match-v5`.
- Платформенный (`ru`, `euw1`, `na1`…): всё остальное.

Платформа игрока не зашивается: определяется по puuid через
`account-v1/region/by-game` и кешируется. Запрос к чужой платформе отвечает
пустым массивом или 404 — выглядит как «ранга нет», а не как ошибка.

Доступные эндпоинты. Лимиты метода указаны там, где они ниже или сравнимы
с лимитом ключа; у остальных они недостижимы.

| Эндпоинт | Хост | Лимит метода |
|---|---|---|
| `GET /riot/account/v1/accounts/by-riot-id/{gameName}/{tagLine}` | рег. | 1000/мин |
| `GET /riot/account/v1/accounts/by-puuid/{puuid}` | рег. | 1000/мин |
| `GET /riot/account/v1/region/by-game/{game}/by-puuid/{puuid}` | рег. | — |
| `GET /lol/match/v5/matches/by-puuid/{puuid}/ids` | рег. | 2000/10 с |
| `GET /lol/match/v5/matches/{matchId}` | рег. | 2000/10 с |
| `GET /lol/match/v5/matches/{matchId}/timeline` | рег. | 2000/10 с |
| `GET /lol/match/v5/matches/by-puuid/{puuid}/replays` | рег. | — |
| `GET /lol/summoner/v4/summoners/by-puuid/{encryptedPUUID}` | плат. | 1600/мин |
| `GET /lol/league/v4/entries/by-puuid/{encryptedPUUID}` | плат. | — |
| `GET /lol/league/v4/entries/{queue}/{tier}/{division}` | плат. | 50/10 с |
| `GET /lol/league-exp/v4/entries/{queue}/{tier}/{division}` | плат. | 50/10 с |
| `GET /lol/league/v4/{challenger,grandmaster,master}leagues/by-queue/{queue}` | плат. | **30/10 с, 500/10 мин** |
| `GET /lol/champion-mastery/v4/champion-masteries/by-puuid/{puuid}[/top\|/by-champion/{id}]` | плат. | — |
| `GET /lol/champion-mastery/v4/scores/by-puuid/{puuid}` | плат. | — |
| `GET /lol/spectator/v5/active-games/by-summoner/{encryptedPUUID}` | плат. | 3000/10 с |
| `GET /lol/platform/v3/champion-rotations` | плат. | — |
| `GET /lol/status/v4/platform-data` | плат. | — |
| `GET /lol/challenges/v1/...` (config, percentiles, leaderboards, player-data) | плат. | — |
| `GET /lol/clash/v1/tournaments`, `.../tournaments/{id}` | плат. | **10/мин** |
| `GET /lol/clash/v1/teams/{teamId}`, `.../tournaments/by-team/{teamId}` | плат. | 200/мин |
| `GET /lol/clash/v1/players/by-puuid/{puuid}` | плат. | — |
| `tournament-stub-v5` (codes, lobby-events, providers, tournaments) | рег. | — |

Цены типовых операций: профиль игрока лобби — 3 запроса (puuid, ранг, мастери),
лобби — 30, около сорока секунд. Матч для краулера — 2 запроса (матч + timeline).

## Сбор данных для пака предпочтений

```powershell
python scripts/crawl.py --seed          # посев фронтира с ладдера
python scripts/crawl.py --hours 8       # выгрузка матчей, Ctrl+C безопасен
python scripts/crawl.py --stats         # что лежит в базе
python scripts/build_pack.py            # SQLite -> project/data/packs/<регион>-<патч>.json + index.json
python scripts/crawl.py --prune 16.19   # оставить только текущий патч
python scripts/crawl.py --refetch-timelines 16.19   # докачать покупки у старых матчей
```

Покупок на игрока хранится до 40 (`MAX_PURCHASES`); матчи со старым потолком
в 12 помечены `has_timeline = 1`. `build_pack.py` без `--patch` берёт самый новый
патч с 1000+ матчами с timeline. Пак — схема 3 (id рун и предметов), C++ читает 2 и 3.

Платформа по умолчанию — `ru` (`--platform`). База `project/data/base.sqlite`
и выгрузки матчей в git не попадают, паки — попадают.
