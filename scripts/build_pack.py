#!/usr/bin/env python3
"""
Агрегация базы в «пак предпочтений» — то единственное, что видит приложение.

    python scripts/build_pack.py                 # текущий патч, регион RU
    python scripts/build_pack.py --patch 15.19 --min-games 30

На выходе два файла в project/data/packs:
    ru-15.19.json  — сам пак
    index.json     — какой пак сейчас актуален, с sha256

Приложение читает индекс, сверяет хеш, при расхождении берёт пак. Сегодня
это локальный каталог, завтра — HTTP с сервера: контракт один и тот же.

Единица агрегата — (чемпион, роль, ранговая корзина). Внутри распределения
по рунам, порядку прокачки, первому предмету и ядру из трёх. Ранговая
корзина берётся из seed_tier матча: ранга участников в match-v5 нет,
и это оценка, а не факт (см. crawl.py).

Про винрейт вариантов: голая доля побед врёт. Сборку чаще доводят до конца
в выигранной игре, поэтому у «победных» сборок завышается и доля, и винрейт.
Здесь считается нижняя граница интервала Вильсона — она штрафует малые
выборки, и вариант с 5 играми не обгоняет вариант с 500.
"""

import argparse
import hashlib
import json
import math
import pathlib
import sqlite3
import sys
import time
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[1]
DEFAULT_DB = ROOT / "project" / "data" / "base.sqlite"
PACKS_DIR = ROOT / "project" / "data" / "packs"
STATIC_DIR = ROOT / "project" / "data" / "static"

DDRAGON = "https://ddragon.leagueoflegends.com"
# 2: бакеты стали матчапами (чемпион|роль|оппонент|ранг), руны отдаются
# страницей целиком, предметы — цепочкой в порядке покупки.
# 3: плюс числовые id рун и предметов (keystoneId, primaryIds, itemIds...):
# по ним интерфейс берёт иконки и названия на языке клиента. Надмножество
# второй схемы — приложение читает обе.
SCHEMA_VERSION = 3

# Ранговые корзины. Дробить по каждому тиру нельзя: выборка расползается,
# а разница между Изумрудом I и Изумрудом IV в сборках неразличима.
TIER_BUCKET = {
    "IRON": "BRONZE", "BRONZE": "BRONZE",
    "SILVER": "GOLD", "GOLD": "GOLD",
    "PLATINUM": "EMERALD", "EMERALD": "EMERALD",
    "DIAMOND": "DIAMOND", "MASTER": "DIAMOND",
    "GRANDMASTER": "DIAMOND", "CHALLENGER": "DIAMOND",
}

ROLES = ["TOP", "JUNGLE", "MIDDLE", "BOTTOM", "UTILITY"]

# Сколько вариантов показывать и с какой выборки вариант вообще существует.
TOP_VARIANTS = 3
MIN_VARIANT_GAMES = 5

# Длина цепочек. Чем длиннее ключ варианта, тем реже он повторяется:
# пять повышений и три предмета — предел, на котором выборка ещё
# набирается, а порядок уже виден.
MAX_SKILL_STEPS = 5
MAX_CHAIN = 3

# Сколько игр должно пройти через звено цепочки предметов, чтобы
# цепочка продлилась дальше него. Ниже — цепочка обрывается на том,
# в чём выборка ещё уверена, а не угадывает третий предмет по двум играм.
MIN_CHAIN_STEP_GAMES = 3

# Сколько матчей с timeline нужно новому патчу, чтобы пак собирался
# по нему, а не по прошлому. Сборки меняются с патчем, и пак старого
# патча в первые дни после выхода нового хуже, чем тонкий, но свежий.
MIN_PATCH_MATCHES = 1000

# Осколки статов: id из statPerks. В Data Dragon их нет ни в runesReforged,
# ни в item.json — это единственный справочник, который приходится держать
# руками, и проверять его надо при смене сезона.
SHARD_NAMES = {
    5001: "Health Scaling",
    5005: "Attack Speed",
    5007: "Ability Haste",
    5008: "Adaptive Force",
    5010: "Move Speed",
    5011: "Health",
    5013: "Tenacity",
}


def fetch_static(name: str, version: str) -> dict:
    """Data Dragon с кешем на диске: статика без ключа и без лимитов."""
    STATIC_DIR.mkdir(parents=True, exist_ok=True)
    path = STATIC_DIR / f"{version}-{name}"
    if path.is_file():
        return json.loads(path.read_text(encoding="utf-8"))

    url = f"{DDRAGON}/cdn/{version}/data/en_US/{name}"
    with urllib.request.urlopen(url, timeout=30) as response:
        data = json.loads(response.read().decode("utf-8"))
    path.write_text(json.dumps(data), encoding="utf-8")
    return data


def latest_version() -> str:
    with urllib.request.urlopen(f"{DDRAGON}/api/versions.json", timeout=30) as response:
        return json.loads(response.read().decode("utf-8"))[0]


def load_runes(version: str) -> dict:
    """id руны -> название. runesReforged лежит списком деревьев."""
    STATIC_DIR.mkdir(parents=True, exist_ok=True)
    path = STATIC_DIR / f"{version}-runesReforged.json"
    if path.is_file():
        trees = json.loads(path.read_text(encoding="utf-8"))
    else:
        url = f"{DDRAGON}/cdn/{version}/data/en_US/runesReforged.json"
        with urllib.request.urlopen(url, timeout=30) as response:
            trees = json.loads(response.read().decode("utf-8"))
        path.write_text(json.dumps(trees), encoding="utf-8")

    names = {}
    for tree in trees:
        names[tree["id"]] = tree["name"]
        for slot in tree.get("slots", []):
            for rune in slot.get("runes", []):
                names[rune["id"]] = rune["name"]
    return names


def load_items(version: str):
    """id предмета -> (название, цена, теги, законченный ли)."""
    data = fetch_static("item.json", version).get("data", {})
    items = {}
    for raw_id, item in data.items():
        items[int(raw_id)] = (
            item.get("name", f"#{raw_id}"),
            int((item.get("gold") or {}).get("total", 0)),
            set(item.get("tags") or []),
            not item.get("into"),  # «into» пуст только у законченных предметов
        )
    return items


def is_core_item(item_id: int, items: dict) -> bool:
    """Ядро — законченный предмет, а не компонент по дороге к нему.

    Цены мало: Hearthbound Axe стоит 1100 и в ядро не входит, это
    полуфабрикат. Признак законченности — пустое поле "into" в Data Dragon:
    из этого предмета больше ничего не собирается.
    """
    entry = items.get(item_id)
    if not entry:
        return False
    _, price, tags, finished = entry
    if not finished:
        return False
    if tags & {"Consumable", "Trinket", "Boots", "Jungle", "Lane"}:
        return False
    return price >= 1600


def wilson_lower(wins: int, games: int, z: float = 1.96) -> float:
    """Нижняя граница доли побед. Пять игр из пяти не обгонят 500 из 900."""
    if games == 0:
        return 0.0
    p = wins / games
    denominator = 1 + z * z / games
    centre = p + z * z / (2 * games)
    spread = z * math.sqrt((p * (1 - p) + z * z / (4 * games)) / games)
    return max(0.0, (centre - spread) / denominator)


class Distribution:
    """Счётчик вариантов: сколько раз встретился и сколько из них выиграл."""

    def __init__(self):
        self.games = {}
        self.wins = {}

    def add(self, key, win: int) -> None:
        if key is None or key == "":
            return
        self.games[key] = self.games.get(key, 0) + 1
        self.wins[key] = self.wins.get(key, 0) + win

    def top(self, total: int, label=lambda key: key):
        """Топ вариантов. label возвращает либо строку-название, либо
        готовый словарь с полями — так страница рун и цепочка предметов
        приезжают структурой, а не склеенным текстом."""
        rows = []
        for key, games in sorted(self.games.items(), key=lambda kv: -kv[1]):
            if games < MIN_VARIANT_GAMES:
                continue
            wins = self.wins[key]
            described = label(key)
            row = described if isinstance(described, dict) else {"name": described}
            row |= {
                "games": games,
                "share": round(games / total, 4) if total else 0.0,
                "winrate": round(wins / games, 4),
                "winrateLow": round(wilson_lower(wins, games), 4),
            }
            rows.append(row)
            if len(rows) >= TOP_VARIANTS:
                break
        return rows


class ChainTree:
    """Цепочки законченных предметов как дерево префиксов.

    Точное совпадение трёх предметов по порядку повторяется редко: на
    сотне игр чемпиона одна и та же тройка встречается два-три раза, и
    вариантов с порогом в пять игр почти не остаётся — именно так в паке
    16.18 у 548 бакетов из 656 предметов не было вовсе.

    Поэтому цепочка строится жадно: самый частый первый предмет, среди
    тех, кто его собрал, — самый частый второй, и так далее, пока через
    звено проходит хотя бы MIN_CHAIN_STEP_GAMES игр. Вариантов столько,
    сколько популярных первых предметов, и у каждого честное число игр —
    тех, кто прошёл весь путь целиком.
    """

    def __init__(self):
        self.chains = []  # (кортеж id, win)

    def add(self, chain, win: int) -> None:
        if chain:
            self.chains.append((tuple(chain[:MAX_CHAIN]), win))

    def top(self, total: int, label):
        firsts = {}
        for chain, _win in self.chains:
            firsts[chain[0]] = firsts.get(chain[0], 0) + 1

        rows = []
        for first, count in sorted(firsts.items(), key=lambda kv: -kv[1]):
            if count < MIN_VARIANT_GAMES or len(rows) >= TOP_VARIANTS:
                break
            path = [first]
            while len(path) < MAX_CHAIN:
                following = {}
                for chain, _win in self.chains:
                    if len(chain) > len(path) and list(chain[:len(path)]) == path:
                        following[chain[len(path)]] = following.get(chain[len(path)], 0) + 1
                if not following:
                    break
                best, best_count = max(following.items(), key=lambda kv: kv[1])
                if best_count < MIN_CHAIN_STEP_GAMES:
                    break
                path.append(best)

            matching = [win for chain, win in self.chains
                        if list(chain[:len(path)]) == path]
            games = len(matching)
            wins = sum(matching)
            row = label(tuple(path))
            row |= {
                "games": games,
                "share": round(games / total, 4) if total else 0.0,
                "winrate": round(wins / games, 4) if games else 0.0,
                "winrateLow": round(wilson_lower(wins, games), 4),
            }
            rows.append(row)
        return rows


def lane_opponents(rows) -> dict:
    """(match_id, puuid) -> чемпион противника по линии.

    Оппонента в match-v5 нет отдельным полем: он выводится из того, что
    в матче ровно два игрока с одинаковым teamPosition и разными командами.
    Роль пустая (ARAM, нераспознанная линия) — пары нет, и такая строка
    в матчапы не идёт.
    """
    by_slot = {}
    for match_id, puuid, champion, role, team in rows:
        if role in ROLES:
            by_slot.setdefault((match_id, role), []).append((puuid, champion, team))

    opponents = {}
    for (match_id, _role), players in by_slot.items():
        if len(players) != 2 or players[0][2] == players[1][2]:
            continue
        first, second = players
        opponents[(match_id, first[0])] = second[1]
        opponents[(match_id, second[0])] = first[1]
    return opponents


def rune_page(row, runes: dict):
    """Страница рун целиком или None, если строка из старой базы.

    Показывать половину страницы нельзя: игрок не сможет её повторить,
    а полуправда в рекомендации хуже её отсутствия.
    """
    keystone, primary_tree, sub_tree, primary_perks, sub_perks, stat_perks = row
    if not keystone or not primary_tree or not sub_tree:
        return None
    if not primary_perks or not sub_perks:
        return None

    def names(csv: str):
        return tuple(int(value) for value in csv.split(",") if value.strip().isdigit())

    primary = names(primary_perks)
    secondary = names(sub_perks)
    shards = names(stat_perks or "")
    if len(primary) != 3 or len(secondary) != 2:
        return None
    return (keystone, primary_tree, primary, sub_tree, secondary, shards)


def build(db: sqlite3.Connection, patch: str, min_games: int,
          runes: dict, items: dict) -> dict:
    buckets = {}

    # Прокачка и покупки лежат отдельными таблицами — читаем их в память
    # одним проходом: на патч это десятки тысяч строк, не миллионы.
    skills = dict(db.execute(
        "SELECT s.match_id || '|' || s.puuid, s.seq FROM skill_order s "
        "JOIN matches m ON m.match_id = s.match_id WHERE m.patch = ?", (patch,)))

    purchases = {}
    for match_id, puuid, item_id in db.execute(
        "SELECT p.match_id, p.puuid, p.item_id FROM purchases p "
        "JOIN matches m ON m.match_id = p.match_id WHERE m.patch = ? "
        "ORDER BY p.match_id, p.puuid, p.seq", (patch,)
    ):
        purchases.setdefault(f"{match_id}|{puuid}", []).append(item_id)

    opponents = lane_opponents(db.execute(
        "SELECT p.match_id, p.puuid, p.champion, p.role, p.win FROM participants p "
        "JOIN matches m ON m.match_id = p.match_id WHERE m.patch = ?", (patch,)))
    # win здесь служит меткой команды: победители и проигравшие — разные
    # стороны, и этого достаточно, чтобы отличить союзника от противника.

    query = """
        SELECT p.champion, p.role, m.seed_tier, p.win, p.match_id, p.puuid,
               p.keystone, p.primary_tree, p.sub_tree,
               p.primary_perks, p.sub_perks, p.stat_perks
        FROM participants p
        JOIN matches m ON m.match_id = p.match_id
        WHERE m.patch = ? AND p.role <> ''
    """

    for row in db.execute(query, (patch,)):
        champion, role, seed_tier, win, match_id, puuid = row[:6]
        if role not in ROLES:
            continue

        tier_bucket = TIER_BUCKET.get(seed_tier or "", None)
        opponent = opponents.get((match_id, puuid))

        core = [item for item in purchases.get(f"{match_id}|{puuid}", [])
                if is_core_item(item, items)]
        skill_seq = skills.get(f"{match_id}|{puuid}", "")
        page = rune_page(row[6:], runes)

        # Каждое наблюдение попадает в четыре бакета: против конкретного
        # оппонента и против всех, в своей ранговой корзине и в общей.
        # Это и есть иерархия отката: чем тоньше выборка по матчапу,
        # тем выше по этой лестнице поднимется интерфейс.
        opponent_keys = ["ANY"] + ([opponent] if opponent else [])
        tier_keys = ["ALL"] + ([tier_bucket] if tier_bucket else [])

        for opponent_key in opponent_keys:
            for tier_key in tier_keys:
                bucket = buckets.setdefault((champion, role, opponent_key, tier_key), {
                    "games": 0,
                    "wins": 0,
                    "runePages": Distribution(),
                    "skills": Distribution(),
                    "chains": ChainTree(),
                })
                bucket["games"] += 1
                bucket["wins"] += win
                if page:
                    bucket["runePages"].add(page, win)
                if len(skill_seq) >= 3:
                    bucket["skills"].add(skill_seq[:MAX_SKILL_STEPS], win)
                if core:
                    bucket["chains"].add(core, win)

    return render(buckets, patch, min_games, runes, items)


def render(buckets: dict, patch: str, min_games: int, runes: dict, items: dict) -> dict:
    rune_name = lambda key: runes.get(key, f"#{key}")
    item_name = lambda key: items.get(key, (f"#{key}", 0, set(), False))[0]

    def page_variant(key):
        keystone, primary_tree, primary, sub_tree, secondary, shards = key
        return {
            "name": rune_name(keystone),
            "page": {
                "keystone": rune_name(keystone),
                "keystoneId": keystone,
                "primaryTree": rune_name(primary_tree),
                "primaryTreeId": primary_tree,
                "primary": [rune_name(perk) for perk in primary],
                "primaryIds": list(primary),
                "secondaryTree": rune_name(sub_tree),
                "secondaryTreeId": sub_tree,
                "secondary": [rune_name(perk) for perk in secondary],
                "secondaryIds": list(secondary),
                "shards": [SHARD_NAMES.get(perk, f"#{perk}") for perk in shards],
                "shardIds": list(shards),
            },
        }

    def chain_variant(key):
        names = [item_name(item) for item in key]
        return {"name": " → ".join(names), "steps": names, "itemIds": list(key)}

    def skill_variant(key):
        return {"name": ">".join(key), "steps": list(key)}

    out = {}
    for (champion, role, opponent, tier_key), bucket in buckets.items():
        total = bucket["games"]
        if total < min_games:
            continue
        out[f"{champion}|{role}|{opponent}|{tier_key}"] = {
            "champion": champion,
            "role": role,
            "opponent": opponent,
            "tier": tier_key,
            "patch": patch,
            "games": total,
            "winrate": round(bucket["wins"] / total, 4),
            "runePages": bucket["runePages"].top(total, page_variant),
            "skillOrders": bucket["skills"].top(total, skill_variant),
            "itemChains": bucket["chains"].top(total, chain_variant),
        }
    return out


def patch_key(patch: str):
    """'16.19' -> (16, 19): патчи сравниваются как числа, '16.9' < '16.10'."""
    return tuple(int(part) for part in patch.split(".") if part.isdigit())


def pick_patch(db: sqlite3.Connection) -> str:
    """Самый новый патч, по которому набралось MIN_PATCH_MATCHES матчей
    с timeline; если такого нет — патч с наибольшим числом матчей.

    Раньше брался только второй вариант, и в первые дни после выхода
    патча пак собирался по прошлому: 2247 матчей 16.18 перевешивали
    1679 матчей 16.19, хотя играли уже на 16.19.
    """
    rows = db.execute(
        "SELECT patch, COUNT(*), SUM(has_timeline >= 1) FROM matches GROUP BY patch"
    ).fetchall()
    if not rows:
        raise SystemExit("база пуста — сначала python scripts/crawl.py --seed --hours N")
    fresh = [patch for patch, _total, timelines in rows if (timelines or 0) >= MIN_PATCH_MATCHES]
    if fresh:
        return max(fresh, key=patch_key)
    return max(rows, key=lambda row: row[1])[0]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--db", type=pathlib.Path, default=DEFAULT_DB)
    parser.add_argument("--region", default="ru")
    parser.add_argument("--patch", help="по умолчанию — самый новый патч с достаточной выборкой")
    parser.add_argument("--min-games", type=int, default=20,
                        help="ниже этого бакет не попадает в пак вовсе")
    parser.add_argument("--out", type=pathlib.Path, default=PACKS_DIR)
    args = parser.parse_args()

    if not args.db.is_file():
        print(f"нет базы {args.db}", file=sys.stderr)
        return 1

    db = sqlite3.connect(args.db)
    patch = args.patch or pick_patch(db)

    version = latest_version()
    runes = load_runes(version)
    items = load_items(version)

    buckets = build(db, patch, args.min_games, runes, items)
    if not buckets:
        print(f"на патче {patch} нет ни одного бакета с {args.min_games}+ играми",
              file=sys.stderr)
        return 1

    pack = {
        "schemaVersion": SCHEMA_VERSION,
        "region": args.region.upper(),
        "patch": patch,
        "generatedAt": int(time.time()),
        "ddragon": version,
        "minGames": args.min_games,
        "buckets": buckets,
    }

    args.out.mkdir(parents=True, exist_ok=True)
    name = f"{args.region.lower()}-{patch}.json"
    path = args.out / name
    body = json.dumps(pack, ensure_ascii=False, separators=(",", ":"))
    path.write_text(body, encoding="utf-8")

    index = {
        "schemaVersion": SCHEMA_VERSION,
        "region": args.region.upper(),
        "patch": patch,
        "file": name,
        "sha256": hashlib.sha256(body.encode("utf-8")).hexdigest(),
        "bytes": len(body.encode("utf-8")),
        "generatedAt": pack["generatedAt"],
    }
    (args.out / "index.json").write_text(
        json.dumps(index, ensure_ascii=False, indent=1), encoding="utf-8")

    print(f"пак {path.name}: бакетов {len(buckets)}, {len(body) / 1024:.0f} КБ")
    print(f"патч {patch}, порог {args.min_games} игр на бакет")
    return 0


if __name__ == "__main__":
    sys.exit(main())
