# Sintence — League of Legends stats on your own machine

A Windows desktop application: a live match scoreboard as an overlay on top of
the game, plus statistics computed from your own match history.

Everything runs locally. Live match data comes from the game client
(Live Client Data API, `127.0.0.1:2999`, no key required); match history comes
from the Riot API. No servers, no accounts, no telemetry: files stay on the
user's disk and the Riot API key never leaves the machine.

> Sintence isn't endorsed by Riot Games and doesn't reflect the views or
> opinions of Riot Games or anyone officially involved in producing or managing
> Riot Games properties. Riot Games and all associated properties are trademarks
> or registered trademarks of Riot Games, Inc.

**Personal project, not open source.** The source is public for review only;
use, copying, modification and redistribution require written permission — see
`LICENSE`.

## What works today

**Live match overlay.** `PgDn` shows and hides a panel centred on the screen:
game time, mode, map, and both teams as cards — champion, Riot ID, role, level,
K/D/A, creep score and CS per minute. Refreshed once per second. Pressing `PgDn`
again hides the panel; `Ctrl+C` in the console quits the program.

**Match history analysis.** Match-V5 data from a local dump: per-champion
aggregation, win rate, KDA, CS/min, top lists and filters by sample size.

Only information the player already sees on screen is displayed. The Live Client
Data API does not expose hidden enemy state — ability cooldowns, map positions —
and the application builds nothing on top of it (`project/POLICY.md`).

## How it fits together

```
League (127.0.0.1:2999)
        |  HTTPS, no key
        v
  LiveClientSource  ──►  LiveGame  ──►  HTTP server 127.0.0.1:8777
  (src/data)             (src/core)     (src/app)
                                              |  JSON
                                              v
                                    Vue 3 inside a WebView2 window
                                    (separate repository: sintence-web)
```

Layers depend strictly downwards: `app/ → analysis/ → core/`,
`app/ → data/ → core/`. No third-party type (nlohmann, cpp-httplib, WebView2)
reaches `core/` or `analysis/` — they are visible only inside `data/*.cpp` and
`app/*.cpp`. That is why swapping the data source requires no change to the
analysis code: files, the Riot API and test fixtures all live behind the
`MatchSource` interface.

| Directory | Contents |
|---|---|
| `src/core/` | domain types: `Champion`, `ChampionReport`, `ChampionIndex`, `LiveGame` |
| `src/data/` | adapters: file reading, JSON parsing, Live Client Data API |
| `src/analysis/` | metrics and aggregation over `core/` types |
| `src/app/` | entry point, local HTTP server, overlay window |
| `tests/` | the whole suite as a single `sintence_tests` binary |
| `project/` | architecture decisions, Riot constraints, data schema and fixtures |

## Build

Requires **Visual Studio 2026** (MSVC 14.51+, C++23), **CMake ≥ 3.20**,
**Node.js 20+** and **vcpkg**. The WebView2 Runtime ships with Windows 11.

The user interface lives in its own repository,
[sintence-web](https://github.com/fremov/sintence-web), cloned next to this one:

```
D:\Dev\...\Portfolio        this repository
D:\Dev\...\sintence-web     the interface; its dist/ is what the window shows
```

C++ dependencies are installed automatically from `vcpkg.json` at configure
time: cpp-httplib (HTTP client and server), OpenSSL, WebView2 SDK.
`doctest` and `nlohmann/json` are header-only files in `third_party/`.

```powershell
# interface (sibling repository)
cd ..\sintence-web
npm install
npm run build
cd ..\Portfolio

# application
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<path to vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Debug
```

Run it **as administrator**: without elevation Windows will not raise the window
above a game protected by anti-cheat.

```powershell
build\src\Debug\sintence.exe
```

The game must run in **Borderless** window mode: nothing draws on top of
exclusive fullscreen.

Build without the UAC prompt (for debugging; the overlay will not appear above
the game in this mode):

```powershell
cmake -S . -B build -DSINTENCE_REQUIRE_ADMIN=OFF ...
```

## Interface development

The interface talks to the application only through the JSON contract
(`/api/health`, `/api/live`, `/api/profiles`, `/api/preferences`); its side
of the contract is a set of zod schemas in `sintence-web/src/api/schemas.ts`.
Editing the interface never requires rebuilding the C++ side:

```powershell
cd ..\sintence-web
npm run dev:mock      # no game, no sintence.exe: mock data and a scenario switcher
npm run dev           # live data: /api is proxied to a running sintence.exe
```

Environment variables of `sintence.exe` that connect the two:

| Variable | Default | Purpose |
|---|---|---|
| `SINTENCE_WEB_DIR` | `../sintence-web/dist` | where the built interface is |
| `SINTENCE_UI_URL` | — | show this URL in the overlay window instead, e.g. `http://localhost:5173` for live Vite |
| `SINTENCE_PORT` | `8777` | local API port; lets a debug build run next to the normal one |

The overlay window's design — size, screen share, anchor, background, hotkey —
is set in `sintence-web/src/overlay/config.ts` and sent to the window over
`chrome.webview.postMessage`; the C++ side only validates and applies it.

## Champion preferences: collecting the data

The scoreboard shows what the majority builds on each champion — keystone,
skill order, first item — for the rank bracket of the player in question.
That is computed offline, not in game: it takes hundreds of thousands of
observations, and a personal key allows 50 requests per minute.

```powershell
python scripts/crawl.py --seed            # seed the frontier from the ladder
python scripts/crawl.py --hours 8         # overnight collection, Ctrl+C safe
python scripts/crawl.py --stats           # what is in the database
python scripts/build_pack.py              # SQLite -> project/data/packs
python scripts/crawl.py --prune 16.19     # drop everything but the live patch
```

Measured on an RU personal key: **~1430 matches per hour** with timelines
(skill order and purchase order), ~2900 without. Every match is ten
observations, so one night is roughly 100 000 of them.

The application never touches the database. It reads `index.json`, then the
pack it points at — a few megabytes that load into memory whole. The same
contract works over HTTP once the collector moves to a server, which is also
where the Riot key belongs the moment the app is distributed to anyone else.

## Tests

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

A single group can be run with doctest flags, without rebuilding:

```powershell
build\tests\Debug\sintence_tests.exe --source-file="*live_client*"
build\tests\Debug\sintence_tests.exe --test-case="*winrate*"
build\tests\Debug\sintence_tests.exe --list-test-cases
```

Tests need no network, no API key and no running game: they work on the fixtures
in `project/data/fixtures/`. Debug builds enable the address sanitizer.

## Riot API key

The key powers the player profiles shown on each scoreboard card — solo queue
rank, LP, win/loss and the top three champions by mastery. The overlay works
without it: `/api/profiles` answers 501 and the cards simply omit that row.

The key **never enters the repository or the compiled binary**. It is read at
startup from the `SINTENCE_RIOT_KEY` environment variable, or from
`%LOCALAPPDATA%\Sintence\riot_key.txt` (first line, whitespace trimmed).

A personal key is limited to **20 requests per second and 100 per 2 minutes**,
across every endpoint. A ten-player lobby costs 30 requests, so profiles arrive
progressively over roughly forty seconds; the UI shows the progress. Requests go
through a sliding-window rate limiter and `puuid` values are cached, so a rematch
against the same people costs nothing. Match dumps are produced by
`scripts/fetch_matches.py` (Python standard library only, no pip, no
dependencies).

The Riot constraints the application honours are listed in `project/POLICY.md`.

## Status

Under active development. Done: the overlay on live data, Match-V5 and Live
Client Data parsing, per-champion aggregation, Riot API requests from the
application itself with rate limiting and caching, lobby profiles (rank and
mastery). In progress: per-player match history on demand.
