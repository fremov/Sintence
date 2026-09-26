#!/usr/bin/env python3
"""
Пульт сбора матчей: запуск crawl.py и build_pack.py из браузера и сводка
по базе — сколько собрано и сколько ещё не хватает для надёжного пака.

    start-crawler.cmd                              # из корня репозитория: собрать и открыть
    python crawler_dashboard/server.py             # http://127.0.0.1:8790
    python crawler_dashboard/server.py --port 8791 --no-browser

Интерфейс — Vue + Vite + Tailwind в crawler_dashboard/web, сервер раздаёт
его сборку (web/dist). Для правки интерфейса: npm run dev в web/ —
Vite проксирует /api сюда.

Сбор идёт без срока, пока его не остановят: кнопкой, закрытием всех
вкладок пульта (страница шлёт пульс, см. BROWSER_TIMEOUT_S) или закрытием
самого сервера — в том числе аварийным: дочерние процессы сидят в задании
Windows (Job Object), и система снимает их вместе с сервером.

Только стандартная библиотека, как и остальные scripts/. Сервер слушает
127.0.0.1 и запускает ровно те команды, что описаны в JOBS, — никаких
произвольных строк из браузера. Действия (POST) принимаются только
с заголовком X-Dashboard-Action и со своего Origin: чужой сайт, открытый
в том же браузере, запустить сбор не сможет.

Ключ Riot сайт не видит и не показывает: его читает сам crawl.py.
"""

import argparse
import collections
import contextlib
import ctypes
import json
import math
import os
import pathlib
import signal
import sqlite3
import subprocess
import sys
import threading
import time
import webbrowser
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "scripts"
WEB_DIST = pathlib.Path(__file__).resolve().parent / "web" / "dist"
DB_PATH = ROOT / "project" / "data" / "base.sqlite"
PACKS_DIR = ROOT / "project" / "data" / "packs"

sys.path.insert(0, str(SCRIPTS))
import build_pack  # noqa: E402  — lane_opponents, TIER_BUCKET, pick_patch: считаем как пак

# Пороги. PACK — с какой выборки бакет вообще попадает в пак (build_pack
# --min-games); RELIABLE — с какой советам можно верить: варианты рун
# и сборок перестают прыгать от одной игры. Для матчапов (чемпион против
# конкретного противника на линии) надёжный порог ниже — их в разы больше,
# и 150 игр на пару не наберётся никогда.
PACK_MIN_GAMES = 20
RELIABLE_GAMES = 150
MATCHUP_RELIABLE = 40

# Роль чемпиона считается «основной», если на ней не меньше этой доли его
# игр. Джинкс в лесу — случайность, и нехватка данных по ней не важна.
MAIN_ROLE_SHARE = 0.15

# Темп по умолчанию, пока в базе нет свежих матчей: персональный ключ,
# 100 запросов за 2 минуты, два запроса на матч (матч + timeline).
DEFAULT_RATE_WITH_TIMELINE = 1430

LOG_LINES = 4000

# Пульс браузера. Открытая страница отмечается каждые 2 секунды; свёрнутый
# или фоновый Chrome будит таймеры страницы не чаще раза в минуту, поэтому
# порог молчания — с запасом. Закрытие вкладки страница сообщает сразу
# (/api/bye), и тогда ждём только GRACE_S: перезагрузка страницы — не выход.
BROWSER_TIMEOUT_S = 150
GRACE_S = 15

# Задачи, которые останавливаются вместе с браузером: они сами не кончаются.
LONG_JOBS = {"crawl", "refetch"}


# --- Задание Windows: дочерние процессы умирают вместе с сервером ------------

class _KillOnClose:
    """Job Object с флагом KILL_ON_JOB_CLOSE. Хэндл задания держит только
    сервер; когда процесс сервера завершается — штатно, по Ctrl+C или
    аварийно, — Windows закрывает хэндл и снимает все процессы задания.
    Без этого crawl.py пережил бы упавший сервер и собирал бы дальше
    без присмотра."""

    def __init__(self):
        self.handle = None
        if os.name != "nt":
            return
        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        self._kernel32 = kernel32
        kernel32.CreateJobObjectW.restype = ctypes.c_void_p
        kernel32.AssignProcessToJobObject.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        kernel32.SetInformationJobObject.argtypes = [ctypes.c_void_p, ctypes.c_int,
                                                     ctypes.c_void_p, ctypes.c_uint32]
        handle = kernel32.CreateJobObjectW(None, None)
        if not handle:
            return

        class BasicLimits(ctypes.Structure):
            _fields_ = [("PerProcessUserTimeLimit", ctypes.c_int64),
                        ("PerJobUserTimeLimit", ctypes.c_int64),
                        ("LimitFlags", ctypes.c_uint32),
                        ("MinimumWorkingSetSize", ctypes.c_size_t),
                        ("MaximumWorkingSetSize", ctypes.c_size_t),
                        ("ActiveProcessLimit", ctypes.c_uint32),
                        ("Affinity", ctypes.c_size_t),
                        ("PriorityClass", ctypes.c_uint32),
                        ("SchedulingClass", ctypes.c_uint32)]

        class IoCounters(ctypes.Structure):
            _fields_ = [(name, ctypes.c_uint64) for name in (
                "ReadOperationCount", "WriteOperationCount", "OtherOperationCount",
                "ReadTransferCount", "WriteTransferCount", "OtherTransferCount")]

        class ExtendedLimits(ctypes.Structure):
            _fields_ = [("BasicLimitInformation", BasicLimits),
                        ("IoInfo", IoCounters),
                        ("ProcessMemoryLimit", ctypes.c_size_t),
                        ("JobMemoryLimit", ctypes.c_size_t),
                        ("PeakProcessMemoryUsed", ctypes.c_size_t),
                        ("PeakJobMemoryUsed", ctypes.c_size_t)]

        limits = ExtendedLimits()
        limits.BasicLimitInformation.LimitFlags = 0x2000  # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
        extended_info_class = 9  # JobObjectExtendedLimitInformation
        if kernel32.SetInformationJobObject(handle, extended_info_class, ctypes.byref(limits),
                                            ctypes.sizeof(limits)):
            self.handle = handle

    def adopt(self, process: subprocess.Popen) -> None:
        if self.handle is not None:
            self._kernel32.AssignProcessToJobObject(self.handle, int(process._handle))


KILL_ON_CLOSE = _KillOnClose()


# --- Задачи ----------------------------------------------------------------

class Job:
    """Один запущенный скрипт: процесс, его вывод и итог."""

    def __init__(self, kind: str, title: str, argv: list[str], params: dict):
        self.id = int(time.time() * 1000)
        self.kind = kind
        self.title = title
        self.argv = argv
        self.params = params
        self.started_at = time.time()
        self.finished_at: float | None = None
        self.exit_code: int | None = None
        self.stopping = False
        self.progress: dict = {}
        self.process = subprocess.Popen(
            argv,
            cwd=ROOT,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            # Своя группа процессов: только так ей можно послать Ctrl+Break
            # (мягкая остановка crawl.py), не задев сам сервер.
            creationflags=getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0),
            env={**os.environ, "PYTHONIOENCODING": "utf-8", "PYTHONUNBUFFERED": "1"},
        )
        KILL_ON_CLOSE.adopt(self.process)
        threading.Thread(target=self._pump, daemon=True).start()

    def _pump(self):
        for raw in self.process.stdout:
            line = raw.decode("utf-8", errors="replace").rstrip()
            if line:
                LOG.add(line, self.id)
                self._parse_progress(line)
        self.exit_code = self.process.wait()
        self.finished_at = time.time()
        LOG.add(f"— {self.title}: завершено, код {self.exit_code}", self.id, system=True)

    def _parse_progress(self, line: str):
        with contextlib.suppress(ValueError, IndexError):
            # «матчей 150 (1412/ч), повторов 12%, игроков обойдено 9, запросов 305»
            if line.startswith("матчей ") and "/ч)" in line:
                self.progress["matches"] = int(line.split()[1])
                self.progress["rate"] = int(line.split("(")[1].split("/")[0])
                self.progress["duplicates"] = line.split("повторов ")[1].split(",")[0]
            # «итог: новых матчей 31, повторов 1, запросов 64»
            elif line.startswith("итог: новых матчей"):
                self.progress["matches"] = int(line.split()[3].rstrip(","))
            # «перекачано 50 из 900 (1400/ч)» и «патч 16.19: перекачать timeline у 900 матчей»
            elif line.startswith("перекачано "):
                parts = line.split()
                self.progress["done"] = int(parts[1])
                self.progress["total"] = int(parts[3])
            elif "перекачать timeline у" in line:
                self.progress["done"] = 0
                self.progress["total"] = int(line.split(" у ")[1].split()[0])

    @property
    def running(self) -> bool:
        return self.finished_at is None

    def stop(self):
        if not self.running:
            return
        self.stopping = True
        if self.kind in ("crawl", "refetch", "seed") and hasattr(signal, "CTRL_BREAK_EVENT"):
            # Мягко: crawl.py доделает текущий матч и выйдет сам.
            self.process.send_signal(signal.CTRL_BREAK_EVENT)
        else:
            self.process.terminate()

    def kill(self):
        if self.running:
            self.process.kill()

    def to_json(self) -> dict:
        end = self.finished_at or time.time()
        return {
            "id": self.id,
            "kind": self.kind,
            "title": self.title,
            "params": self.params,
            "running": self.running,
            "stopping": self.stopping,
            "startedAt": self.started_at,
            "elapsedS": round(end - self.started_at),
            "exitCode": self.exit_code,
            "progress": self.progress,
        }


class Log:
    """Кольцевой журнал вывода задач: браузер забирает строки после курсора."""

    def __init__(self):
        self._lines = collections.deque(maxlen=LOG_LINES)
        self._next = 0
        self._lock = threading.Lock()

    def add(self, text: str, job_id: int | None = None, system: bool = False):
        with self._lock:
            self._lines.append({
                "n": self._next, "t": time.time(), "job": job_id, "text": text, "system": system,
            })
            self._next += 1

    def since(self, cursor: int) -> tuple[list[dict], int]:
        with self._lock:
            return [line for line in self._lines if line["n"] >= cursor], self._next


LOG = Log()
JOBS_LOCK = threading.Lock()
JOBS: list[Job] = []

# Задачи, которые пишут в базу, идут по одной. Сборка пака только читает —
# её можно запускать и во время сбора (SQLite в режиме WAL).
WRITERS = {"crawl", "seed", "refetch", "prune"}


def python(*args: str) -> list[str]:
    return [sys.executable, "-u", *args]


def make_job(kind: str, body: dict) -> tuple[str, list[str], dict]:
    """Проверенные параметры -> команда. Строки из браузера в команду
    попадают только после проверки формата."""

    def number(name, default, low, high, cast=float):
        value = body.get(name, default)
        try:
            value = cast(value)
        except (TypeError, ValueError):
            raise ValueError(f"{name}: не число")
        if not low <= value <= high:
            raise ValueError(f"{name}: от {low} до {high}")
        return value

    def patch(name):
        value = str(body.get(name, "")).strip()
        parts = value.split(".")
        if len(parts) != 2 or not all(part.isdigit() for part in parts):
            raise ValueError(f"{name}: нужен патч вида 16.19")
        return value

    def platform():
        value = str(body.get("platform", "ru")).strip().lower()
        if not value.isalnum() or len(value) > 5:
            raise ValueError("platform: ru, euw1, eun1, na1...")
        return value

    crawl = str(SCRIPTS / "crawl.py")
    if kind == "crawl":
        per_player = number("perPlayer", 20, 1, 100, int)
        days = number("days", 14, 1, 60, int)
        argv = python(crawl, "--until-stopped", "--per-player", str(per_player),
                      "--days", str(days), "--platform", platform())
        if body.get("seed"):
            argv.append("--seed")
        if body.get("noTimeline"):
            argv.append("--no-timeline")
        params = {"perPlayer": per_player, "days": days,
                  "seed": bool(body.get("seed")), "noTimeline": bool(body.get("noTimeline"))}
        return "Сбор матчей", argv, params
    if kind == "seed":
        pages = number("pages", 1, 1, 10, int)
        return ("Посев игроков с ладдера",
                python(crawl, "--seed", "--seed-pages", str(pages), "--platform", platform()),
                {"pages": pages})
    if kind == "refetch":
        value = patch("patch")
        # Без --hours: до конца списка или до остановки.
        return (f"Докачка timeline {value}", python(crawl, "--refetch-timelines", value),
                {"patch": value})
    if kind == "prune":
        value = patch("patch")
        return f"Оставить только {value}", python(crawl, "--prune", value), {"patch": value}
    if kind == "pack":
        argv = python(str(SCRIPTS / "build_pack.py"))
        params = {}
        if body.get("patch"):
            params["patch"] = patch("patch")
            argv += ["--patch", params["patch"]]
        min_games = number("minGames", PACK_MIN_GAMES, 5, 500, int)
        argv += ["--min-games", str(min_games)]
        params["minGames"] = min_games
        return "Сборка пака", argv, params
    raise ValueError(f"неизвестная задача {kind}")


def start_job(kind: str, body: dict) -> Job:
    title, argv, params = make_job(kind, body)
    with JOBS_LOCK:
        running = [job for job in JOBS if job.running]
        if any(job.kind == kind for job in running):
            raise RuntimeError("такая задача уже идёт")
        if kind in WRITERS and any(job.kind in WRITERS for job in running):
            raise RuntimeError("в базу уже пишет другая задача — дождитесь её или остановите")
        external = external_crawlers()
        if kind in WRITERS and external:
            raise RuntimeError(f"crawl.py уже запущен вне сайта (pid {external[0]['pid']})")
        shown = " ".join(pathlib.Path(a).name if os.path.isabs(a) else a for a in argv)
        LOG.add(f"— {title}: {shown}", system=True)
        job = Job(kind, title, argv, params)
        JOBS.append(job)
        del JOBS[:-20]
        return job


# --- Пульс браузера ------------------------------------------------------------

CLIENTS_LOCK = threading.Lock()
CLIENTS: dict[str, float] = {}      # id вкладки -> когда отмечалась
LEFT: dict[str, float] = {}         # закрытые вкладки -> когда ушли


def touch_client(client: str) -> None:
    if client:
        with CLIENTS_LOCK:
            CLIENTS[client[:64]] = time.time()
            LEFT.pop(client[:64], None)


def client_left(client: str) -> None:
    with CLIENTS_LOCK:
        if CLIENTS.pop(client[:64], None) is not None:
            LEFT[client[:64]] = time.time()


def browser_watchdog() -> None:
    """Раз в 5 секунд: если открытых вкладок пульта не осталось, долгие
    задачи останавливаются мягко — сбор не должен идти без присмотра."""
    while True:
        time.sleep(5)
        now = time.time()
        with CLIENTS_LOCK:
            for client, seen in list(CLIENTS.items()):
                if now - seen > BROWSER_TIMEOUT_S:
                    del CLIENTS[client]
                    LEFT[client] = now - GRACE_S  # пропал без «до свидания» — ждать нечего
            alive = bool(CLIENTS)
            recently_left = any(now - at < GRACE_S for at in LEFT.values())
        if alive or recently_left:
            continue
        with JOBS_LOCK:
            orphaned = [job for job in JOBS
                        if job.running and not job.stopping and job.kind in LONG_JOBS]
        for job in orphaned:
            LOG.add(f"— {job.title}: браузер закрыт — останавливаю", job.id, system=True)
            job.stop()


# --- crawl.py, запущенный руками из терминала -------------------------------

_external_cache = {"at": 0.0, "value": []}


def external_crawlers() -> list[dict]:
    """crawl.py, запущенные не этим сервером (из терминала). Раз в 5 секунд:
    опрос процессов через PowerShell стоит около секунды."""
    if time.time() - _external_cache["at"] < 5:
        return _external_cache["value"]
    own = {job.process.pid for job in JOBS if job.running}
    found = []
    if os.name == "nt":
        with contextlib.suppress(Exception):
            output = subprocess.run(
                ["powershell", "-NoProfile", "-Command",
                 "Get-CimInstance Win32_Process -Filter \"Name='python.exe'\" | "
                 "ForEach-Object { \"$($_.ProcessId)`t$($_.CommandLine)\" }"],
                capture_output=True, text=True, timeout=10,
                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
            ).stdout
            for line in output.splitlines():
                pid, _, command = line.partition("\t")
                if "crawl.py" in command and pid.strip().isdigit() and int(pid) not in own:
                    found.append({"pid": int(pid), "command": command.strip()})
    _external_cache.update(at=time.time(), value=found)
    return found


def stop_external(pid: int) -> None:
    """Снять crawl.py из терминала. Мягко нельзя — он в чужой консоли;
    но crawl.py пишет каждый матч отдельной транзакцией, и снятие ничего
    не портит: недокачанный матч просто скачается в следующий раз."""
    if pid not in {item["pid"] for item in external_crawlers()}:
        raise ValueError("такого процесса сбора нет")
    subprocess.run(["taskkill", "/PID", str(pid), "/F"], capture_output=True,
                   creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    _external_cache["at"] = 0
    LOG.add(f"— внешний crawl.py (pid {pid}) остановлен", system=True)


# --- Сводка по базе -----------------------------------------------------------

def open_db() -> sqlite3.Connection | None:
    if not DB_PATH.exists():
        return None
    # Только чтение: сайт базу не меняет, меняют скрипты.
    connection = sqlite3.connect(f"file:{DB_PATH.as_posix()}?mode=ro", uri=True, timeout=10)
    return connection


def scalar(db, sql, *args):
    row = db.execute(sql, args).fetchone()
    return row[0] if row and row[0] is not None else 0


def overview(db: sqlite3.Connection) -> dict:
    size = sum(path.stat().st_size for path in DB_PATH.parent.glob("base.sqlite*"))
    patches = [
        {"patch": patch, "matches": total, "timeline": timeline or 0}
        for patch, total, timeline in db.execute(
            "SELECT patch, COUNT(*), SUM(has_timeline >= 1) FROM matches GROUP BY patch")
    ]
    patches.sort(key=lambda row: build_pack.patch_key(row["patch"]), reverse=True)
    tiers = [
        {"tier": tier, "matches": count}
        for tier, count in db.execute(
            "SELECT COALESCE(seed_tier, '?'), COUNT(*) FROM matches GROUP BY 1 ORDER BY 2 DESC")
    ]
    now = int(time.time())
    return {
        "matches": scalar(db, "SELECT COUNT(*) FROM matches"),
        "withTimeline": scalar(db, "SELECT COUNT(*) FROM matches WHERE has_timeline >= 1"),
        "participants": scalar(db, "SELECT COUNT(*) FROM participants"),
        "players": scalar(db, "SELECT COUNT(*) FROM players"),
        "queue": scalar(db, "SELECT COUNT(*) FROM players WHERE taken_at IS NULL"),
        "lastHour": scalar(db, "SELECT COUNT(*) FROM matches WHERE fetched_at >= ?", now - 3600),
        "lastDay": scalar(db, "SELECT COUNT(*) FROM matches WHERE fetched_at >= ?", now - 86400),
        "lastFetchedAt": scalar(db, "SELECT MAX(fetched_at) FROM matches"),
        "rate": crawl_rate(db),
        "sizeBytes": size,
        "patches": patches,
        "tiers": tiers,
    }


def crawl_rate(db: sqlite3.Connection) -> dict:
    """Темп сбора по последним 300 матчам: сколько в час, пока шёл сбор.
    Перерывы больше 10 минут не считаются — иначе ночь простоя делила бы
    темп на ноль."""
    stamps = [row[0] for row in db.execute(
        "SELECT fetched_at FROM matches ORDER BY fetched_at DESC LIMIT 300")]
    stamps.reverse()
    busy = sum(min(later - earlier, 600) for earlier, later in zip(stamps, stamps[1:])
               if later - earlier <= 600)
    if len(stamps) < 20 or busy <= 0:
        return {"perHour": DEFAULT_RATE_WITH_TIMELINE, "measured": False}
    return {"perHour": round((len(stamps) - 1) / busy * 3600), "measured": True}


def needed_matches(counts: list[int], threshold: int, share: float, matches_now: int) -> int | None:
    """Сколько матчей нужно, чтобы доля наблюдений в бакетах с threshold+
    играми дошла до share. Экстраполяция: бакет растёт пропорционально
    числу матчей, распределение чемпионов и ролей не меняется."""
    total = sum(counts)
    if not total or not matches_now:
        return None
    covered = 0
    for n in sorted(counts, reverse=True):  # самые большие бакеты доходят первыми
        covered += n
        if covered / total >= share:
            return max(matches_now, math.ceil(matches_now * threshold / n))
    return None


def family(counter: dict, threshold: int, matches_now: int, targets=(0.8, 0.9)) -> dict:
    counts = [n for n in counter.values() if n > 0]
    total = sum(counts)
    ok = [n for n in counts if n >= threshold]
    return {
        "threshold": threshold,
        "buckets": len(counts),
        "bucketsOk": len(ok),
        "share": round(sum(ok) / total, 4) if total else 0,
        "targets": [
            {"share": share, "matches": needed_matches(counts, threshold, share, matches_now)}
            for share in targets
        ],
    }


_coverage_cache: dict = {}


def coverage(db: sqlite3.Connection, patch: str) -> dict:
    """Насколько патч покрыт выборкой — теми же бакетами, что строит пак:
    чемпион + роль, в ранговой корзине, против оппонента на линии."""
    stamp = (patch, scalar(db, "SELECT COUNT(*) FROM matches WHERE patch = ?", patch))
    if _coverage_cache.get("stamp") == stamp:
        return _coverage_cache["value"]

    matches_now = stamp[1]
    timeline_now = scalar(db, "SELECT COUNT(*) FROM matches WHERE patch = ? AND has_timeline >= 1",
                          patch)
    rows = db.execute(
        "SELECT p.match_id, p.puuid, p.champion, p.role, p.win, m.seed_tier, p.champion_id "
        "FROM participants p JOIN matches m ON m.match_id = p.match_id "
        "WHERE m.patch = ? AND p.role <> ''", (patch,)).fetchall()
    opponents = build_pack.lane_opponents(row[:5] for row in rows)

    base = collections.Counter()
    tiers = collections.Counter()
    matchups = collections.Counter()
    champion_ids = {}
    for match_id, puuid, champion, role, _win, seed_tier, champion_id in rows:
        if role not in build_pack.ROLES:
            continue
        champion_ids[champion] = champion_id
        base[(champion, role)] += 1
        tier = build_pack.TIER_BUCKET.get(seed_tier or "")
        if tier:
            tiers[(champion, role, tier)] += 1
        opponent = opponents.get((match_id, puuid))
        if opponent:
            matchups[(champion, role, opponent)] += 1

    # Основные роли чемпионов и что по ним не хватает.
    per_champion = collections.Counter()
    for (champion, _role), n in base.items():
        per_champion[champion] += n
    mains = []
    for (champion, role), n in base.items():
        if n < MAIN_ROLE_SHARE * per_champion[champion]:
            continue
        mains.append({
            "champion": champion,
            "championId": champion_ids.get(champion, 0),
            "role": role,
            "games": n,
            "share": round(n / per_champion[champion], 3),
            "toPack": max(0, PACK_MIN_GAMES - n),
            "toReliable": max(0, RELIABLE_GAMES - n),
            "matchups": sum(1 for (c, r, _o), m in matchups.items()
                            if c == champion and r == role and m >= PACK_MIN_GAMES),
        })
    mains.sort(key=lambda item: item["games"])

    tier_totals = collections.Counter()
    for (_c, _r, tier), n in tiers.items():
        tier_totals[tier] += n

    value = {
        "patch": patch,
        "matches": matches_now,
        "withTimeline": timeline_now,
        "packMinMatches": build_pack.MIN_PATCH_MATCHES,
        "observations": sum(base.values()),
        "champions": len(per_champion),
        "thresholds": {"pack": PACK_MIN_GAMES, "reliable": RELIABLE_GAMES,
                       "matchup": MATCHUP_RELIABLE},
        "base": family(base, PACK_MIN_GAMES, matches_now),
        "baseReliable": family(base, RELIABLE_GAMES, matches_now),
        "tier": family(tiers, PACK_MIN_GAMES, matches_now),
        "matchup": family(matchups, PACK_MIN_GAMES, matches_now, targets=(0.5, 0.8)),
        "matchupReliable": family(matchups, MATCHUP_RELIABLE, matches_now, targets=(0.5, 0.8)),
        "tierGames": [{"tier": tier, "observations": n} for tier, n in tier_totals.most_common()],
        "mains": mains,
    }
    _coverage_cache.update(stamp=stamp, value=value)
    return value


_pack_cache: dict = {}


def pack_summary(path: pathlib.Path) -> dict:
    """Бакетов в паке и порог — из самого файла (2 МБ, читается раз на версию)."""
    stamp = (path.name, path.stat().st_mtime)
    if stamp not in _pack_cache:
        summary = {}
        with contextlib.suppress(OSError, ValueError):
            data = json.loads(path.read_text(encoding="utf-8"))
            buckets = data.get("buckets", {})
            summary = {
                "buckets": len(buckets),
                "minGames": data.get("minGames"),
                # Бакеты «чемпион + роль против всех, все ранги» — основа советов.
                "baseBuckets": sum(1 for key in buckets if key.endswith("|ANY|ALL")),
            }
        _pack_cache[stamp] = summary
    return _pack_cache[stamp]


def packs() -> dict:
    index = {}
    with contextlib.suppress(OSError, ValueError):
        index = json.loads((PACKS_DIR / "index.json").read_text(encoding="utf-8"))
    files = []
    for path in sorted(PACKS_DIR.glob("*.json")):
        if path.name == "index.json":
            continue
        files.append({"name": path.name, "sizeBytes": path.stat().st_size,
                      "modifiedAt": path.stat().st_mtime, **pack_summary(path)})
    return {"index": index, "files": files}


def key_status() -> dict:
    """Есть ли ключ — без самого ключа."""
    if os.environ.get("SINTENCE_RIOT_KEY", "").strip():
        return {"found": True, "source": "переменная SINTENCE_RIOT_KEY"}
    local = os.environ.get("LOCALAPPDATA")
    if local and (pathlib.Path(local) / "Sintence" / "riot_key.txt").is_file():
        return {"found": True, "source": "%LOCALAPPDATA%\\Sintence\\riot_key.txt"}
    return {"found": False, "source": ""}


# --- HTTP ---------------------------------------------------------------------

CONTENT_TYPES = {".html": "text/html; charset=utf-8", ".js": "text/javascript; charset=utf-8",
                 ".css": "text/css; charset=utf-8", ".svg": "image/svg+xml",
                 ".png": "image/png", ".ico": "image/x-icon", ".woff2": "font/woff2"}


class Handler(BaseHTTPRequestHandler):
    server_version = "SintenceCrawler/1"

    def log_message(self, fmt, *args):  # запросы браузера в консоль не нужны
        pass

    def send_json(self, value, status=HTTPStatus.OK):
        body = json.dumps(value, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        url = urlparse(self.path)
        query = parse_qs(url.query)
        try:
            if url.path == "/api/status":
                touch_client(query.get("client", [""])[0])
                with JOBS_LOCK:
                    jobs = [job.to_json() for job in reversed(JOBS)]
                self.send_json({"jobs": jobs, "external": external_crawlers(),
                                "key": key_status(), "packs": packs(), "now": time.time()})
            elif url.path == "/api/log":
                lines, cursor = LOG.since(int(query.get("since", ["0"])[0] or 0))
                self.send_json({"lines": lines, "cursor": cursor})
            elif url.path == "/api/stats":
                db = open_db()
                if db is None:
                    self.send_json({"empty": True})
                    return
                with contextlib.closing(db):
                    info = overview(db)
                    patch = query.get("patch", [""])[0] or (
                        build_pack.pick_patch(db) if info["matches"] else "")
                    info["coverage"] = coverage(db, patch) if patch else None
                    info["packPatch"] = build_pack.pick_patch(db) if info["matches"] else ""
                self.send_json(info)
            else:
                self.send_static(url.path)
        except Exception as error:  # сводка не должна ронять сервер
            self.send_json({"error": str(error)}, HTTPStatus.INTERNAL_SERVER_ERROR)

    def send_static(self, path: str):
        if not (WEB_DIST / "index.html").is_file():
            body = ("Интерфейс пульта не собран. Запустите start-crawler.cmd из корня "
                    "репозитория или npm install && npm run build в crawler_dashboard/web.")
            self.send_response(HTTPStatus.SERVICE_UNAVAILABLE)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.end_headers()
            self.wfile.write(body.encode("utf-8"))
            return
        name = "index.html" if path in ("", "/") else path.lstrip("/")
        target = (WEB_DIST / name).resolve()
        if WEB_DIST.resolve() not in target.parents or not target.is_file():
            self.send_error(HTTPStatus.NOT_FOUND)
            return
        body = target.read_bytes()
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", CONTENT_TYPES.get(target.suffix, "application/octet-stream"))
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def trusted(self) -> bool:
        """Действие — только со своей страницы. Заголовок X-Dashboard-Action
        чужой сайт без CORS не пошлёт, а Host защищает от DNS-rebinding."""
        port = self.server.server_address[1]
        allowed = {f"127.0.0.1:{port}", f"localhost:{port}"}
        origin = self.headers.get("Origin")
        return (self.headers.get("X-Dashboard-Action") == "1"
                and self.headers.get("Host") in allowed
                and (origin is None or origin.removeprefix("http://") in allowed))

    def do_POST(self):
        url = urlparse(self.path)
        # Вкладка закрывается. sendBeacon своих заголовков не шлёт, поэтому
        # здесь проверка только Origin и Host: чужой сайт максимум
        # «закроет» чужой id, которого не знает.
        if url.path == "/api/bye":
            port = self.server.server_address[1]
            allowed = {f"127.0.0.1:{port}", f"localhost:{port}"}
            origin = (self.headers.get("Origin") or "").removeprefix("http://")
            length = int(self.headers.get("Content-Length") or 0)
            raw = self.rfile.read(min(length, 4096)) if length else b""
            if self.headers.get("Host") in allowed and origin in allowed:
                with contextlib.suppress(ValueError, AttributeError):
                    client_left(str(json.loads(raw or b"{}").get("client", "")))
            self.send_response(HTTPStatus.NO_CONTENT)
            self.end_headers()
            return
        if not self.trusted():
            self.send_json({"error": "forbidden"}, HTTPStatus.FORBIDDEN)
            return
        length = int(self.headers.get("Content-Length") or 0)
        try:
            body = json.loads(self.rfile.read(length) or b"{}") if length else {}
        except ValueError:
            self.send_json({"error": "тело не JSON"}, HTTPStatus.BAD_REQUEST)
            return
        try:
            if url.path.startswith("/api/jobs/") and url.path.endswith("/stop"):
                job_id = int(url.path.split("/")[3])
                with JOBS_LOCK:
                    job = next((j for j in JOBS if j.id == job_id), None)
                if job is None:
                    raise ValueError("задачи нет")
                if job.stopping:
                    job.kill()  # вторая «Остановить» — уже без церемоний
                    LOG.add(f"— {job.title}: снято принудительно", job.id, system=True)
                else:
                    job.stop()
                    LOG.add(f"— {job.title}: останавливаю после текущего матча", job.id, system=True)
                self.send_json(job.to_json())
            elif url.path == "/api/external/stop":
                stop_external(int(body.get("pid", 0)))
                self.send_json({"ok": True})
            elif url.path.startswith("/api/run/"):
                job = start_job(url.path.removeprefix("/api/run/"), body)
                self.send_json(job.to_json())
            else:
                self.send_json({"error": "нет такого действия"}, HTTPStatus.NOT_FOUND)
        except (ValueError, RuntimeError) as error:
            self.send_json({"error": str(error)}, HTTPStatus.CONFLICT)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", type=int, default=8790)
    parser.add_argument("--no-browser", action="store_true")
    args = parser.parse_args()

    server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    url = f"http://127.0.0.1:{args.port}/"
    print(f"пульт сбора: {url}  (Ctrl+C — выход; идущий сбор остановится мягко)", flush=True)
    LOG.add("— пульт запущен", system=True)
    threading.Thread(target=browser_watchdog, daemon=True).start()
    if not args.no_browser:
        threading.Timer(0.5, webbrowser.open, (url,)).start()
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        with JOBS_LOCK:
            running = [job for job in JOBS if job.running]
        for job in running:
            job.stop()
        for job in running:
            with contextlib.suppress(subprocess.TimeoutExpired):
                job.process.wait(timeout=60)
            job.kill()
        server.server_close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
