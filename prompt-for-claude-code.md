# Prompt for Claude Code

---

You are setting up a personal C++ learning environment for me, and then mentoring me through it. Do the whole setup in one pass.

**Always talk to me in Russian.** Every response and every file you write — theory, task comments, error explanations, commit messages. C++ keywords, type names, and library names stay in English inside Russian sentences. This prompt is in English; your output is not.

## Who I am

I'm 22, working a desk job, studying alongside the Yandex Practicum course "Разработка на C++ (расширенный)". I've finished sprint 1: loops, conditionals, `std::vector`. My next topic is classes. The end goal is that I write, by myself, a League of Legends match statistics analyzer with a real interface and real API requests: parse match data, aggregate by champion and role, compute KDA, winrate, CS/min.

I own two books, and the theory you write points into them:
- Robert Martin, *Clean Code* (Russian edition, «Чистый код»)
- Aditya Bhargava, *Grokking Algorithms*, 2nd edition (Russian edition, «Грокаем алгоритмы»)

## Two files I'm giving you

**`SKILL.md`** — install it at `.claude/skills/cpp-mentor/SKILL.md` unchanged. It holds the whole teaching contract: file ownership, hint levels, how theory is delivered, the task rubric, pacing. Read it before you build anything, because the setup has to match it. It replaces slash commands — it triggers on what I actually say, so I don't have to remember a command vocabulary.

**`fetch_matches.py`** — install it at `scripts/fetch_matches.py` unchanged. Standard library only. It resolves a Riot ID to a PUUID via ACCOUNT-V1, pulls match IDs via MATCH-V5, saves one JSON per match into `project/data/matches/`, writes an `index.json`, throttles to the development key's limits, and resumes if interrupted. Read it before you write anything around it.

## The core rule

**You never write my implementations.** Theory, declarations with `// TODO` bodies, and tests are yours. The bodies are mine. `SKILL.md` has the exact file ownership table — follow it literally, including the part that says you *do* own `tests.cpp` and can fix your own bugs in it.

## What to create

### 1. Folder layout

```
.
├── CLAUDE.md                 # short: points at the skill, doesn't duplicate it
├── PROGRESS.md               # topic map, checkboxes, weak spots, session log
├── README.md                 # build & run
├── CMakeLists.txt            # root, auto-discovers tasks
├── .claude/skills/cpp-mentor/SKILL.md
├── scripts/fetch_matches.py
├── third_party/doctest.h     # fetch from raw.githubusercontent.com/doctest/doctest
├── topics/
│   ├── 00_build_model/
│   └── 01_classes/
│       ├── THEORY.md         # one-page reference card, not a textbook
│       └── tasks/
│           └── t01_champion/
│               ├── champion.h        # declarations, acceptance criteria, TODO
│               ├── champion.cpp      # empty bodies — mine
│               ├── tests.cpp         # yours
│               └── CMakeLists.txt
└── project/
    └── data/
```

C++17, CMake ≥ 3.16, doctest (single header, no network at build time). The root `CMakeLists.txt` discovers every `topics/*/tasks/*/CMakeLists.txt` via `file(GLOB_RECURSE ...)` so new tasks appear without edits. `enable_testing()` plus `add_test` per task so `ctest` runs everything.

Debug builds get `-fsanitize=address,undefined -g -O0` (MSVC: `/fsanitize=address /Zi`) from day one. Undefined behavior doesn't crash — it works until it doesn't.

Detect my OS first, verify a compiler, CMake, and Python 3.8+. If something's missing, print the install commands for my system and stop until I've run them.

### 2. No documentation site

I considered one and dropped it. Theory in three layers, no duplication — `SKILL.md` has the full reasoning, and the short version is: conversation is the adaptive layer, `THEORY.md` is a one-page reference card per topic, and the books plus Practicum carry the depth. A site would rot into a third stale copy of what the books already say better.

So `THEORY.md` is a card: the topic's signatures and idioms, three to five topic-specific mistakes each paired with the compiler error it produces, the «Читать в книгах» block, and where the topic shows up in the analyzer. If it starts explaining the language from first principles, it's too long.

### 3. Data: local files first

This is the biggest way to lose a month to setup. Development keys expire every 24 hours, there are rate limits, and HTTP from C++ means libcurl and build configuration. If that lands during the classes topic, I drown in infrastructure instead of learning the language. So the course runs on local files fetched by the Python script, and C++ networking waits until topic 14.

Around the script, write:

- `project/data/README.md` in Russian: getting a development key at developer.riotgames.com, the 24-hour expiry, setting `RIOT_API_KEY` on my OS, a ready-to-paste command for my region, what to do when the key expires mid-topic.
- `project/data/sample_match.json` — one realistic match with the full Riot structure, hand-written, as a schema reference I can read without downloading anything.
- Three synthetic fixtures in `project/data/fixtures/`, so tests run before I've fetched anything. Make one deliberately malformed — a missing field, a null where a number belongs — because topic 8 needs it.
- `project/data/matches/` in `.gitignore`. Match data doesn't belong in the repo.

**Tell me to fetch 200–500 matches, not 25.** Winrate over 30 games is noise; the analyzer would be computing statistics that don't mean anything. That's several runs of the script across different days, and more than one account is fine. The analyzer itself should say when a sample is too small to support a conclusion — that's both a real feature and a lesson about the domain.

Document the field subset the course actually uses: `metadata.matchId`, `info.gameDuration`, `info.queueId`, and per participant `puuid`, `championName`, `teamPosition`, `kills`, `deaths`, `assists`, `win`, `totalMinionsKilled`, `neutralMinionsKilled`. The real payload has hundreds of fields; naming the ten that matter is what keeps topic 8 from becoming a parsing marathon.

The script prints my PUUID. Tell me to save it — finding myself among ten participants is the analyzer's first real filtering problem.

### 4. Task map

Generate topics 0 and 1 fully. Describe the rest in `PROGRESS.md` as a plan and generate each one when I reach it, following the task rubric in `SKILL.md`.

| # | Topic | Analyzer increment | Clean Code | Grokking |
|---|---|---|---|---|
| 0 | Translation units, headers, compiler vs linker | project skeleton that builds | 5 | — |
| 1 | Classes, encapsulation, const-correctness | `Champion`, `PlayerStats`, KDA | 2, 6, 10 | — |
| 2 | Constructors, RAII, rule of 0/3/5, move | ownership of match data | 10 | — |
| 3 | Inheritance, polymorphism, interfaces | data sources behind an interface | 6, 11 | — |
| 4 | STL containers | match index by champion | — | 2, 5, 7, 8 |
| 5 | STL algorithms, lambdas, sorting | top champions, filters | — | 1, 2, 4, 10 |
| 6 | Strings and file I/O | reading the data directory | 8 | — |
| 7 | Errors and exceptions | malformed and partial matches | 7 | — |
| 8 | JSON parsing | real Riot payloads into my types | 8 | — |
| 9 | Unit tests, pure functions | I take over writing tests | 3, 9 | — |
| 10 | Templates and generic programming | generic metric aggregators | — | — |
| 11 | Module boundaries, architecture | core / I/O / reporting split | 8, 11, 12 | — |
| 12 | Complexity and optimization | faster aggregation | 17 | 1, 11, 12 |
| 13 | External dependencies | nlohmann/json and libcurl wired in | 8 | — |
| 14 | HTTP client, real Riot API requests | the app fetches matches itself | 7, 8 | — |
| 15 | Caching, rate limiting, resilience | offline mode, retry with backoff | 7, 17 | 5 |
| 16 | Threads and async | fetching without freezing the interface | 13 | — |
| 17 | UI: from CLI to Dear ImGui | dashboard with tables, filters, charts | 6, 11 | — |
| 18 | Final assembly and release | packaging, config, documentation | 14, 17 | 13 |

**Topic 0 is not filler.** My first task is `champion.h` plus `champion.cpp`, and I will hit `undefined reference` within the hour without even knowing that's a category of error distinct from a compile error. Half an hour on translation units, what the compiler does versus what the linker does, include guards, and how to read both kinds of error message saves weeks of poking at things.

Verify these chapter numbers against my copies the first time you cite them — Russian translations sometimes renumber. If they differ, fix the table and every `THEORY.md`.

### 5. Notes on the late topics (13–18)

These carry the most ways to waste a month on setup instead of learning. Constrain them.

**Dependencies (13).** CMake `FetchContent` first — no extra tooling, repo stays self-contained. vcpkg only if FetchContent fails on my machine. Pin exact versions, never a moving branch. The point isn't the libraries, it's the boundary: a thin adapter of mine wraps each one so the analyzer core never includes a third-party header. Clean Code chapter 8 is the whole argument.

**HTTP (14).** libcurl by default; `cpp-httplib` with OpenSSL as fallback if libcurl fights my toolchain. The Python script is the reference specification — I port its behavior to C++ and diff my output against it. The key comes from the environment, never from source, never from git. Requests get timeouts. Status codes are domain events: 403 means the key expired, 429 means slow down, 5xx means retry.

**Caching (15).** Live requests don't replace local files, they populate them. Fetch once, write to `data/matches/`, read from disk afterwards. Offline mode keeps working with the network unplugged. Hard rule: **no test ever touches the network.** Network code is tested through a fake implementation of my own HTTP interface — which is why the boundary in topic 13 had to exist.

**Threads (16).** Introduce this only because the UI needs it. A blocking fetch on the render thread freezes the window, and I should feel that freeze before you fix it. Prefer a worker thread handing results back through a queue over mutexes everywhere. Turn on `-fsanitize=thread` for this topic.

**UI (17).** Dear ImGui with a GLFW + OpenGL3 backend, plus ImPlot for charts. If graphics setup fails on my machine, fall back to FTXUI in the terminal rather than burning days on drivers. Before starting, build a proper CLI: argument parsing, formatted table output.

The UI is the exam for topic 11. If wiring up the interface requires changing a single header in the analyzer core, the architecture was wrong — say so plainly and fix the boundary before writing any UI code. The core computes, the UI displays, neither knows the other's types.

**Release (18).** Config file for region and Riot ID instead of hardcoded values, a README someone else could follow, a build that produces a runnable artifact on my OS.

### 6. PROGRESS.md

Four sections, kept current:

- **Topic map** with checkboxes and dates.
- **Weak spots** — what I didn't understand, still open. Pull from this when generating tasks.
- **Session log** — one line per session: date, what I did. This is what makes a return after a break possible.
- **Pace** — rough plan of five to seven hours a week, and an honest note that nineteen topics is six to nine months at that rate. Say so up front; a course that pretends to be shorter than it is gets abandoned in month two.

### 7. CLAUDE.md

Keep it short and pointed at the skill rather than duplicating it. It should say: speak Russian, always consult `cpp-mentor` when working in this repository, never write implementations, and Practicum takes priority — during a sprint crunch this course pauses, and you say so before I have to ask.

One rule gets duplicated here rather than left to the skill, because it has to hold even in a session where the skill doesn't fire: **three to five sentences by default, no preamble, no restating my question.** A lookup question — syntax, what an error message means, what's in a library — gets a straight answer immediately, not a leading question.

## Finishing setup

Build the project, confirm topic 0 builds clean and topic 1's tests fail specifically because the tasks are unimplemented rather than because the build is broken. Then show me three lines: the build command, the test command, and the path to the first file I open. Nothing else. I'll tell you when to start.
