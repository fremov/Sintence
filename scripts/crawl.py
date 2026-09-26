#!/usr/bin/env python3
"""
Сбор базы матчей в SQLite. Запускается часами, прерывается Ctrl+C,
продолжается с того же места.

    python scripts/crawl.py --seed              # посев по дивизионам, один раз
    python scripts/crawl.py --hours 8           # ночной прогон
    python scripts/crawl.py --hours 1 --no-timeline
    python scripts/crawl.py --stats             # что уже собрано

Потолок ключа — 50 запросов в минуту. Матч без timeline стоит один
запрос, с timeline два, поэтому за час выходит ~2900 или ~1450 матчей.
Каждый матч — это десять наблюдений: руны и сборки всех участников.

Что собирается (только ранговая соло-очередь, только текущий патч):
    matches       — шапка матча, патч, ранг посева
    participants  — десять строк на матч: руны, финальные предметы, итог
    skill_order   — порядок прокачки, первые шесть повышений (нужен timeline)
    purchases     — первые покупки по порядку (нужен timeline)
    players       — очередь обхода: кого ещё не спрашивали

Ранга участников в match-v5 больше нет, поэтому матчу приписывается ранг
ТОГО ИГРОКА, от которого краулер в него пришёл. Соло-очередь рангово
однородна, так что это приличное приближение — но именно приближение,
и в схеме оно называется seed_tier, а не tier.
"""

import argparse
import contextlib
import pathlib
import signal
import sqlite3
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from riot_client import RiotClient, RiotError, load_key  # noqa: E402

DEFAULT_DB = pathlib.Path(__file__).resolve().parents[1] / "project" / "data" / "base.sqlite"

TIERS = ["DIAMOND", "EMERALD", "PLATINUM", "GOLD", "SILVER", "BRONZE", "IRON"]
DIVISIONS = ["I", "II", "III", "IV"]
APEX = ["challenger", "grandmaster", "master"]

SKILL_SLOTS = {1: "Q", 2: "W", 3: "E", 4: "R"}

SCHEMA = """
PRAGMA journal_mode = WAL;

CREATE TABLE IF NOT EXISTS meta (
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS players (
    puuid      TEXT PRIMARY KEY,
    tier       TEXT,
    division   TEXT,
    taken_at   INTEGER,          -- NULL = ещё не спрашивали
    found_in   TEXT              -- 'seed' или match_id, откуда взялся
);
CREATE INDEX IF NOT EXISTS players_pending ON players(taken_at) WHERE taken_at IS NULL;

CREATE TABLE IF NOT EXISTS matches (
    match_id     TEXT PRIMARY KEY,
    patch        TEXT NOT NULL,          -- "15.19"
    queue_id     INTEGER NOT NULL,
    duration_s   INTEGER NOT NULL,
    started_at   INTEGER NOT NULL,
    seed_tier    TEXT,                   -- ОЦЕНКА ранга матча, не факт
    has_timeline INTEGER NOT NULL DEFAULT 0,
    fetched_at   INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS matches_patch ON matches(patch);

CREATE TABLE IF NOT EXISTS participants (
    match_id      TEXT NOT NULL,
    puuid         TEXT NOT NULL,
    champion      TEXT NOT NULL,         -- каноническое "Zed", "MonkeyKing"
    champion_id   INTEGER NOT NULL,
    role          TEXT NOT NULL,         -- teamPosition; пустой в ARAM
    win           INTEGER NOT NULL,
    kills         INTEGER NOT NULL,
    deaths        INTEGER NOT NULL,
    assists       INTEGER NOT NULL,
    cs            INTEGER NOT NULL,
    item0 INTEGER, item1 INTEGER, item2 INTEGER,
    item3 INTEGER, item4 INTEGER, item5 INTEGER, item6 INTEGER,
    keystone      INTEGER,
    primary_tree  INTEGER,
    sub_tree      INTEGER,
    -- Руны хранятся ПОСТРАНИЧНО, а не одной кучей: страница показывается
    -- целиком, и для этого нужно знать, какая руна в какой ветке стоит.
    primary_perks TEXT,                  -- три малых основной ветки, по порядку
    sub_perks     TEXT,                  -- две малых дополнительной
    stat_perks    TEXT,                  -- три осколка: атака, гибкий, защита
    spell1        INTEGER,
    spell2        INTEGER,
    PRIMARY KEY (match_id, puuid)
);
CREATE INDEX IF NOT EXISTS participants_champ ON participants(champion, role);
CREATE INDEX IF NOT EXISTS participants_puuid ON participants(puuid);

CREATE TABLE IF NOT EXISTS skill_order (
    match_id TEXT NOT NULL,
    puuid    TEXT NOT NULL,
    seq      TEXT NOT NULL,              -- "QEWQQRQEW": первые девять повышений
    PRIMARY KEY (match_id, puuid)
);

CREATE TABLE IF NOT EXISTS purchases (
    match_id TEXT NOT NULL,
    puuid    TEXT NOT NULL,
    seq      INTEGER NOT NULL,           -- порядковый номер покупки
    item_id  INTEGER NOT NULL,
    at_ms    INTEGER NOT NULL,
    PRIMARY KEY (match_id, puuid, seq)
);
"""

# Сколько покупок хранить на игрока.
#
# Было 12, и этого не хватало: стартовый набор, зелья и компоненты съедают
# первую дюжину целиком, и законченный предмет в неё попадал в лучшем случае
# один. Цепочек из двух-трёх предметов в паке не было почти ни у кого.
# Сорок покупок покрывают полную сборку в игре на 35 минут.
MAX_PURCHASES = 40

# has_timeline в matches: 0 — нет, 1 — старое извлечение с обрезанными
# покупками, 2 — полное. --refetch-timelines докачивает единицы до двоек.
TIMELINE_FULL = 2

# Сколько повышений способностей хранить. Девять — это порядок до уровня 9,
# в нём видно и первую способность, и очередь максимизации.
MAX_SKILL_LEVELS = 9

stop_requested = False


def request_stop(_signum, _frame):
    global stop_requested
    stop_requested = True
    print("\nостанавливаюсь после текущего матча...", flush=True)


def migrate(db: sqlite3.Connection) -> None:
    """Добавляет колонки, которых нет в старой базе.

    CREATE TABLE IF NOT EXISTS существующую таблицу не меняет, поэтому
    база, собранная до постраничных рун, молча осталась бы без них.
    Старые строки получают NULL — агрегатор такие страницы пропускает,
    но скиллы и предметы из них по-прежнему считаются.
    """
    existing = {row[1] for row in db.execute("PRAGMA table_info(participants)")}
    for column in ("primary_perks", "sub_perks", "stat_perks"):
        if column not in existing:
            db.execute(f"ALTER TABLE participants ADD COLUMN {column} TEXT")
    db.commit()


def open_db(path: pathlib.Path) -> sqlite3.Connection:
    path.parent.mkdir(parents=True, exist_ok=True)
    connection = sqlite3.connect(path)
    connection.executescript(SCHEMA)
    migrate(connection)
    return connection


def patch_of(game_version: str) -> str:
    parts = game_version.split(".")
    return ".".join(parts[:2]) if len(parts) >= 2 else game_version


def seed(client: RiotClient, db: sqlite3.Connection, pages: int) -> int:
    """Посев: игроки из всех дивизионов, пропорционально ладдеру."""
    added = 0
    rows = []

    for tier in APEX:
        entries = client.apex_league(tier)
        rows += [(puuid, tier.upper(), rank) for puuid, tier_name, rank in entries]
        print(f"посев {tier}: {len(entries)}", flush=True)

    for tier in TIERS:
        for division in DIVISIONS:
            for page in range(1, pages + 1):
                entries = client.league_page(tier, division, page)
                rows += entries
                print(f"посев {tier} {division} стр. {page}: {len(entries)}", flush=True)
                if len(entries) < 200:
                    break

    with db:
        for puuid, tier, division in rows:
            cursor = db.execute(
                "INSERT OR IGNORE INTO players(puuid, tier, division, found_in) "
                "VALUES (?, ?, ?, 'seed')",
                (puuid, tier, division),
            )
            added += cursor.rowcount
    return added


def extract_match(db: sqlite3.Connection, match: dict, seed_tier: str) -> bool:
    """Шапка матча и десять участников. False — матч не годится."""
    info = match.get("info") or {}
    metadata = match.get("metadata") or {}
    match_id = metadata.get("matchId")
    participants = info.get("participants") or []
    if not match_id or len(participants) != 10:
        return False

    # Ремейки до 5 минут не отражают ни сборку, ни прокачку.
    if int(info.get("gameDuration", 0)) < 300:
        return False

    db.execute(
        "INSERT OR REPLACE INTO matches(match_id, patch, queue_id, duration_s, "
        "started_at, seed_tier, has_timeline, fetched_at) VALUES (?,?,?,?,?,?,0,?)",
        (
            match_id,
            patch_of(str(info.get("gameVersion", ""))),
            int(info.get("queueId", 0)),
            int(info.get("gameDuration", 0)),
            int(info.get("gameStartTimestamp", 0)) // 1000,
            seed_tier,
            int(time.time()),
        ),
    )

    for participant in participants:
        perks = participant.get("perks") or {}
        styles = perks.get("styles") or []
        keystone = primary_tree = sub_tree = None
        primary_minor = []
        sub_minor = []
        for style in styles:
            selections = [s.get("perk") for s in style.get("selections") or []]
            if style.get("description") == "primaryStyle":
                primary_tree = style.get("style")
                if selections:
                    keystone = selections[0]
                    primary_minor = [str(p) for p in selections[1:] if p]
            else:
                sub_tree = style.get("style")
                sub_minor = [str(p) for p in selections if p]

        # Осколки статов лежат отдельно от деревьев, тремя именованными
        # полями. Порядок фиксированный: атака, гибкий, защита — так же,
        # как они стоят в интерфейсе выбора рун.
        stats = perks.get("statPerks") or {}
        stat_perks = [str(stats.get(name, 0)) for name in ("offense", "flex", "defense")]

        db.execute(
            "INSERT OR REPLACE INTO participants(match_id, puuid, champion, champion_id, "
            "role, win, kills, deaths, assists, cs, item0, item1, item2, item3, item4, "
            "item5, item6, keystone, primary_tree, sub_tree, primary_perks, sub_perks, "
            "stat_perks, spell1, spell2) "
            "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
            (
                match_id,
                participant.get("puuid", ""),
                participant.get("championName", ""),
                int(participant.get("championId", 0)),
                participant.get("teamPosition", "") or "",
                1 if participant.get("win") else 0,
                int(participant.get("kills", 0)),
                int(participant.get("deaths", 0)),
                int(participant.get("assists", 0)),
                int(participant.get("totalMinionsKilled", 0))
                + int(participant.get("neutralMinionsKilled", 0)),
                *[int(participant.get(f"item{i}", 0)) for i in range(7)],
                keystone,
                primary_tree,
                sub_tree,
                ",".join(primary_minor),
                ",".join(sub_minor),
                ",".join(stat_perks),
                int(participant.get("summoner1Id", 0)),
                int(participant.get("summoner2Id", 0)),
            ),
        )

    # Новые игроки в очередь обхода: ранг наследуется от матча.
    for puuid in metadata.get("participants") or []:
        db.execute(
            "INSERT OR IGNORE INTO players(puuid, tier, found_in) VALUES (?, ?, ?)",
            (puuid, seed_tier, match_id),
        )
    return True


def extract_timeline(db: sqlite3.Connection, match_id: str, timeline: dict) -> None:
    """Порядок прокачки и первые покупки. Единственный их источник."""
    info = timeline.get("info") or {}
    puuids = (timeline.get("metadata") or {}).get("participants") or []
    if not puuids:
        return

    skills = {index: [] for index in range(1, len(puuids) + 1)}
    buys = {index: [] for index in range(1, len(puuids) + 1)}

    for frame in info.get("frames") or []:
        for event in frame.get("events") or []:
            kind = event.get("type")
            who = event.get("participantId")
            if not who or who not in skills:
                continue
            if kind == "SKILL_LEVEL_UP":
                slot = SKILL_SLOTS.get(event.get("skillSlot"))
                if slot and len(skills[who]) < MAX_SKILL_LEVELS:
                    skills[who].append(slot)
            elif kind == "ITEM_PURCHASED":
                if len(buys[who]) < MAX_PURCHASES:
                    buys[who].append((int(event.get("itemId", 0)),
                                      int(event.get("timestamp", 0))))
            elif kind == "ITEM_UNDO":
                # Промах мышью и откат покупки. Без этого случайно купленный
                # предмет попадёт в «первый предмет» статистики.
                undone = int(event.get("beforeId", 0))
                for position in range(len(buys[who]) - 1, -1, -1):
                    if buys[who][position][0] == undone:
                        del buys[who][position]
                        break

    # Повторное извлечение (--refetch-timelines) не должно оставлять
    # хвост старой, обрезанной версии.
    db.execute("DELETE FROM purchases WHERE match_id = ?", (match_id,))

    for index, puuid in enumerate(puuids, start=1):
        if skills[index]:
            db.execute(
                "INSERT OR REPLACE INTO skill_order(match_id, puuid, seq) VALUES (?,?,?)",
                (match_id, puuid, "".join(skills[index])),
            )
        for seq, (item_id, at_ms) in enumerate(buys[index]):
            db.execute(
                "INSERT OR REPLACE INTO purchases(match_id, puuid, seq, item_id, at_ms) "
                "VALUES (?,?,?,?,?)",
                (match_id, puuid, seq, item_id, at_ms),
            )

    db.execute("UPDATE matches SET has_timeline = ? WHERE match_id = ?",
               (TIMELINE_FULL, match_id))


def refetch_timelines(client: RiotClient, db: sqlite3.Connection, patch: str,
                      hours: float) -> None:
    """Перекачать timeline матчей, извлечённых со старым потолком покупок.

    Один запрос на матч вместо двух: сам матч уже в базе. Порядок
    случайный не нужен — идём по старшинству, прерывание безопасно,
    повторный запуск продолжит с того же места.
    """
    deadline = time.monotonic() + hours * 3600 if hours > 0 else float("inf")
    pending = [row[0] for row in db.execute(
        "SELECT match_id FROM matches WHERE patch = ? AND has_timeline = 1 "
        "ORDER BY started_at DESC", (patch,))]
    print(f"патч {patch}: перекачать timeline у {len(pending)} матчей", flush=True)

    done = 0
    started = time.monotonic()
    for match_id in pending:
        if stop_requested or time.monotonic() >= deadline:
            break
        events = client.timeline(match_id)
        if events:
            with db:
                extract_timeline(db, match_id, events)
        done += 1
        if done % 50 == 0:
            elapsed = (time.monotonic() - started) / 3600
            rate = done / elapsed if elapsed > 0 else 0
            print(f"перекачано {done} из {len(pending)} ({rate:.0f}/ч)", flush=True)

    print(f"итог: перекачано {done} из {len(pending)}, запросов {client.requests}",
          flush=True)


def next_player(db: sqlite3.Connection):
    row = db.execute(
        "SELECT puuid, tier FROM players WHERE taken_at IS NULL "
        "ORDER BY RANDOM() LIMIT 1"
    ).fetchone()
    return row


def crawl(client: RiotClient, db: sqlite3.Connection, hours: float,
          per_player: int, days: int, timeline: bool) -> None:
    deadline = time.monotonic() + hours * 3600
    start_time = int(time.time()) - days * 86400

    started = time.monotonic()
    new_matches = 0
    duplicates = 0
    players_done = 0

    while not stop_requested and time.monotonic() < deadline:
        row = next_player(db)
        if row is None:
            print("очередь обхода пуста — нужен --seed", flush=True)
            return
        puuid, tier = row
        db.execute("UPDATE players SET taken_at = ? WHERE puuid = ?",
                   (int(time.time()), puuid))
        db.commit()
        players_done += 1

        for match_id in client.match_ids(puuid, per_player, start_time):
            if stop_requested or time.monotonic() >= deadline:
                break
            known = db.execute("SELECT 1 FROM matches WHERE match_id = ?",
                               (match_id,)).fetchone()
            if known:
                duplicates += 1
                continue

            match = client.match(match_id)
            if not match:
                continue

            with db:
                if not extract_match(db, match, tier):
                    continue

            if timeline:
                events = client.timeline(match_id)
                if events:
                    with db:
                        extract_timeline(db, match_id, events)

            new_matches += 1
            if new_matches % 25 == 0:
                elapsed = (time.monotonic() - started) / 3600
                rate = new_matches / elapsed if elapsed > 0 else 0
                total = duplicates + new_matches
                share = 100 * duplicates / total if total else 0
                print(
                    f"матчей {new_matches} ({rate:.0f}/ч), повторов {share:.0f}%, "
                    f"игроков обойдено {players_done}, запросов {client.requests}",
                    flush=True,
                )

    print(f"итог: новых матчей {new_matches}, повторов {duplicates}, "
          f"запросов {client.requests}", flush=True)


def show_stats(db: sqlite3.Connection) -> None:
    def scalar(sql, *args):
        row = db.execute(sql, args).fetchone()
        return row[0] if row else 0

    print(f"матчей:        {scalar('SELECT COUNT(*) FROM matches')}")
    print(f"  с timeline:  {scalar('SELECT COUNT(*) FROM matches WHERE has_timeline>=1')}")
    print(f"  полных:      {scalar('SELECT COUNT(*) FROM matches WHERE has_timeline=?', TIMELINE_FULL)}")
    print(f"участников:    {scalar('SELECT COUNT(*) FROM participants')}")
    print(f"прокачек:      {scalar('SELECT COUNT(*) FROM skill_order')}")
    print(f"покупок:       {scalar('SELECT COUNT(*) FROM purchases')}")
    print(f"игроков всего: {scalar('SELECT COUNT(*) FROM players')}")
    print(f"  в очереди:   {scalar('SELECT COUNT(*) FROM players WHERE taken_at IS NULL')}")

    print("\nпо патчам:")
    for patch, count in db.execute(
        "SELECT patch, COUNT(*) FROM matches GROUP BY patch ORDER BY 2 DESC"
    ):
        print(f"  {patch}: {count}")

    print("\nпо рангу посева:")
    for tier, count in db.execute(
        "SELECT COALESCE(seed_tier,'?'), COUNT(*) FROM matches GROUP BY 1 ORDER BY 2 DESC"
    ):
        print(f"  {tier}: {count}")


def prune(db: sqlite3.Connection, keep_patch: str) -> None:
    """Удалить всё, кроме указанного патча. Агрегаты живут в паках отдельно."""
    with db:
        db.execute(
            "DELETE FROM skill_order WHERE match_id IN "
            "(SELECT match_id FROM matches WHERE patch <> ?)", (keep_patch,))
        db.execute(
            "DELETE FROM purchases WHERE match_id IN "
            "(SELECT match_id FROM matches WHERE patch <> ?)", (keep_patch,))
        db.execute(
            "DELETE FROM participants WHERE match_id IN "
            "(SELECT match_id FROM matches WHERE patch <> ?)", (keep_patch,))
        db.execute("DELETE FROM matches WHERE patch <> ?", (keep_patch,))
    db.execute("VACUUM")
    print(f"оставлен патч {keep_patch}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--db", type=pathlib.Path, default=DEFAULT_DB)
    parser.add_argument("--platform", default="ru", help="ru, euw1, eun1, na1...")
    parser.add_argument("--seed", action="store_true", help="посев по дивизионам")
    parser.add_argument("--seed-pages", type=int, default=1)
    parser.add_argument("--hours", type=float, default=0.0, help="сколько собирать")
    parser.add_argument("--per-player", type=int, default=20,
                        help="матчей с одного игрока")
    parser.add_argument("--days", type=int, default=14,
                        help="брать матчи не старше N дней (обычно длина патча)")
    parser.add_argument("--no-timeline", action="store_true",
                        help="вдвое быстрее, но без скиллов и порядка покупок")
    parser.add_argument("--stats", action="store_true")
    parser.add_argument("--prune", metavar="PATCH", help="удалить все патчи, кроме этого")
    parser.add_argument("--refetch-timelines", metavar="PATCH",
                        help="перекачать timeline матчей патча с обрезанными покупками")
    args = parser.parse_args()

    db = open_db(args.db)

    if args.stats:
        show_stats(db)
        return 0
    if args.prune:
        prune(db, args.prune)
        return 0

    try:
        client = RiotClient(load_key(), platform=args.platform)
    except RiotError as error:
        print(error, file=sys.stderr)
        return 1

    signal.signal(signal.SIGINT, request_stop)

    try:
        if args.refetch_timelines:
            refetch_timelines(client, db, args.refetch_timelines, args.hours)
        if args.seed:
            print(f"новых игроков в очереди: {seed(client, db, args.seed_pages)}")
        if args.hours > 0:
            crawl(client, db, args.hours, args.per_player, args.days,
                  timeline=not args.no_timeline)
    except RiotError as error:
        print(error, file=sys.stderr)
        return 1
    finally:
        with contextlib.suppress(Exception):
            db.commit()
            db.close()

    return 0


if __name__ == "__main__":
    sys.exit(main())
