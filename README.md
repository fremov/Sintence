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
                                              (web/)
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
| `web/` | user interface: Vue 3 + Vite + TypeScript |
| `tests/` | the whole suite as a single `sintence_tests` binary |
| `project/` | architecture decisions, Riot constraints, data schema and fixtures |

## Build

Requires **Visual Studio 2026** (MSVC 14.51+, C++23), **CMake ≥ 3.20**,
**Node.js 20+** and **vcpkg**. The WebView2 Runtime ships with Windows 11.

C++ dependencies are installed automatically from `vcpkg.json` at configure
time: cpp-httplib (HTTP client and server), OpenSSL, WebView2 SDK.
`doctest` and `nlohmann/json` are header-only files in `third_party/`.

```powershell
# interface
cd web
npm install
npm run build
cd ..

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

Editing CSS does not require rebuilding the C++ side:

```powershell
build\src\Debug\sintence.exe   # serves /api/live on port 8777
cd web && npm run dev          # http://localhost:5173, hot reload
```

Vite proxies `/api` to the application, so the page in the browser shows real
data from the running match. Details in `web/README.md`.

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

The key is only needed for match history; the overlay works without it.

The key **never enters the repository or the compiled binary**: it lives in a
file in the user's profile directory and is read at startup. Match dumps are
produced by `scripts/fetch_matches.py` (Python standard library only, no pip, no
dependencies).

The Riot constraints the application honours are listed in `project/POLICY.md`.

## Status

Under active development. Done: the overlay on live data, Match-V5 and Live
Client Data parsing, per-champion aggregation. In progress: Riot API requests
from the application itself, caching and rate limiting, history of the players
in the current match.
