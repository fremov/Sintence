#!/usr/bin/env python3
"""
Выгрузка матчей League of Legends из Riot API в локальные файлы.

Запускается один раз, чтобы у курса появились реальные данные.
Дальше весь анализатор работает с папки data/matches и в сеть не ходит.

Требуется только Python 3.8+, сторонние библиотеки не нужны.

Ключ: https://developer.riotgames.com -> Sign in -> DEVELOPMENT API KEY.
Ключ живёт 24 часа, после этого его надо обновить на сайте.

Примеры:
    export RIOT_API_KEY=RGAPI-xxxxxxxx
    python3 fetch_matches.py --riot-id "Faker#KR1" --region asia --count 30
    python3 fetch_matches.py --riot-id "Имя#EUW" --region europe --queue 420
    python3 fetch_matches.py --riot-id "Имя#EUW" --region europe --count 500 --timeline

Повторный запуск догружает только новое: уже скачанные файлы пропускаются.
"""

import argparse
import json
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

# Региональные кластеры для account-v1 и match-v5.
# Это НЕ платформа (euw1, na1): match-v5 живёт на кластерах.
REGIONS = {
    "europe": "EUW, EUNE, RU, TR",
    "americas": "NA, BR, LAN, LAS",
    "asia": "KR, JP",
    "sea": "OCE, PH, SG, TH, TW, VN",
}

# Dev-ключ: 20 запросов в секунду и 100 запросов за 2 минуты.
# 100/120с даёт 1.2с на запрос, берём с запасом.
DELAY_SECONDS = 1.3

# Часто используемые очереди. Полный список:
# https://static.developer.riotgames.com/docs/lol/queues.json
QUEUES = {
    400: "Normal Draft",
    420: "Ranked Solo/Duo",
    440: "Ranked Flex",
    450: "ARAM",
}

# Cloudflare перед api.riotgames.com отбивает запросы с дефолтным
# User-Agent вида "Python-urllib/3.x": отвечает 403 и телом
# "error code: 1010". Это не Riot и не ключ — это фильтр по UA.
HEADERS = {
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/128.0.0.0 Safari/537.36"
    ),
    "Accept": "application/json",
    "Accept-Language": "en-US,en;q=0.9",
}


class RiotError(Exception):
    def __init__(self, message, code=None):
        super().__init__(message)
        self.code = code


def request_json(url, api_key, attempt=1):
    """GET с обработкой троттлинга и понятными сообщениями об ошибках."""
    headers = dict(HEADERS)
    headers["X-Riot-Token"] = api_key
    req = urllib.request.Request(url, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        body = ""
        try:
            body = e.read().decode("utf-8", "replace")
        except Exception:
            pass
        if "error code: 1010" in body:
            raise RiotError(
                "запрос отбил Cloudflare (error code: 1010), до Riot он не дошёл.\n"
                "Причина — фильтр по User-Agent. Проверь, что в запрос уходит HEADERS.",
                e.code,
            )
        if e.code == 429:
            wait = int(e.headers.get("Retry-After", 10))
            print(f"  лимит запросов, жду {wait}с", flush=True)
            time.sleep(wait + 1)
            if attempt < 4:
                return request_json(url, api_key, attempt + 1)
            raise RiotError("лимит запросов не отпускает, попробуй позже", 429)
        if e.code in (401, 403):
            raise RiotError(
                f"ключ отклонён ({e.code}). Скорее всего он истёк — dev-ключ живёт 24 часа.\n"
                f"Ответ Riot: {body.strip() or '(пусто)'}\n"
                "Обнови ключ на https://developer.riotgames.com и перезапиши RIOT_API_KEY.",
                e.code,
            )
        if e.code == 404:
            raise RiotError("не найдено (404). Проверь Riot ID и регион.", 404)
        if e.code >= 500:
            if attempt < 4:
                print(f"  сервер Riot вернул {e.code}, повтор через 5с", flush=True)
                time.sleep(5)
                return request_json(url, api_key, attempt + 1)
            raise RiotError(f"сервер Riot недоступен ({e.code})")
        raise RiotError(f"HTTP {e.code}: {e.reason}")
    except urllib.error.URLError as e:
        raise RiotError(f"сеть недоступна: {e.reason}")


def get_puuid(riot_id, region, api_key):
    if "#" not in riot_id:
        raise RiotError('Riot ID должен быть в формате "Имя#TAG", например "Faker#KR1"')
    game_name, tag_line = riot_id.split("#", 1)
    url = (
        f"https://{region}.api.riotgames.com/riot/account/v1/accounts/by-riot-id/"
        f"{urllib.parse.quote(game_name)}/{urllib.parse.quote(tag_line)}"
    )
    account = request_json(url, api_key)
    return account["puuid"]


def get_match_ids(puuid, region, api_key, count, queue):
    """match-v5 отдаёт максимум 100 id за запрос, поэтому идём страницами."""
    ids = []
    start = 0
    while len(ids) < count:
        batch_size = min(100, count - len(ids))
        params = {"start": start, "count": batch_size}
        if queue:
            params["queue"] = queue
        url = (
            f"https://{region}.api.riotgames.com/lol/match/v5/matches/by-puuid/"
            f"{puuid}/ids?{urllib.parse.urlencode(params)}"
        )
        batch = request_json(url, api_key)
        if not batch:
            break
        ids.extend(batch)
        start += len(batch)
        time.sleep(DELAY_SECONDS)
    return ids[:count]


def get_match(match_id, region, api_key):
    url = f"https://{region}.api.riotgames.com/lol/match/v5/matches/{match_id}"
    return request_json(url, api_key)


def get_timeline(match_id, region, api_key):
    url = (
        f"https://{region}.api.riotgames.com/lol/match/v5/matches/"
        f"{match_id}/timeline"
    )
    return request_json(url, api_key)


def save_json(path, data, pretty=False):
    """Компактно по умолчанию: отступы раздувают файл примерно вдвое."""
    with open(path, "w", encoding="utf-8") as f:
        if pretty:
            json.dump(data, f, ensure_ascii=False, indent=2)
        else:
            json.dump(data, f, ensure_ascii=False, separators=(",", ":"))


def main():
    parser = argparse.ArgumentParser(
        description="Скачивает матчи League of Legends в локальные JSON-файлы.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="Регионы: " + "; ".join(f"{k} ({v})" for k, v in REGIONS.items()),
    )
    parser.add_argument("--riot-id", required=True, help='Riot ID в формате "Имя#TAG"')
    parser.add_argument("--region", required=True, choices=sorted(REGIONS))
    parser.add_argument("--count", type=int, default=25, help="сколько матчей (по умолчанию 25)")
    parser.add_argument(
        "--queue",
        type=int,
        default=None,
        help="фильтр по очереди: " + ", ".join(f"{k}={v}" for k, v in QUEUES.items()),
    )
    parser.add_argument("--out", default="project/data/matches", help="куда складывать матчи")
    parser.add_argument(
        "--timeline",
        action="store_true",
        help="качать ещё и таймлайны: вдвое больше запросов и примерно в 5 раз больше места, "
             "но без них не посчитать смерти по минутам и удержание золота",
    )
    parser.add_argument(
        "--timeline-out",
        default=None,
        help="куда складывать таймлайны (по умолчанию папка timelines рядом с матчами)",
    )
    parser.add_argument(
        "--pretty",
        action="store_true",
        help="сохранять с отступами — читаемо глазами, но файлы вдвое больше",
    )
    parser.add_argument("--key", default=os.environ.get("RIOT_API_KEY"))
    args = parser.parse_args()

    if not args.key:
        sys.exit(
            "Нет ключа. Получи dev-ключ на https://developer.riotgames.com и задай его:\n"
            "  export RIOT_API_KEY=RGAPI-...        (Linux/macOS)\n"
            "  $env:RIOT_API_KEY='RGAPI-...'        (PowerShell)\n"
            "или передай через --key"
        )

    out_dir = os.path.abspath(args.out)
    os.makedirs(out_dir, exist_ok=True)

    tl_dir = None
    if args.timeline:
        tl_dir = os.path.abspath(
            args.timeline_out
            or os.path.join(os.path.dirname(out_dir), "timelines")
        )
        os.makedirs(tl_dir, exist_ok=True)

    try:
        print(f"Ищу аккаунт {args.riot_id} в кластере {args.region}...", flush=True)
        puuid = get_puuid(args.riot_id, args.region, args.key)
        time.sleep(DELAY_SECONDS)

        queue_note = f", очередь {args.queue}" if args.queue else ""
        print(f"Запрашиваю список матчей (до {args.count}{queue_note})...", flush=True)
        match_ids = get_match_ids(puuid, args.region, args.key, args.count, args.queue)

        if not match_ids:
            sys.exit("Матчей не найдено. Попробуй без --queue или другой аккаунт.")

        print(f"Нашёл {len(match_ids)}. Скачиваю в {out_dir}", flush=True)

        saved, skipped, failed, tl_saved, no_timeline = 0, 0, 0, 0, 0
        for i, match_id in enumerate(match_ids, 1):
            done = []
            path = os.path.join(out_dir, f"{match_id}.json")

            if os.path.exists(path):
                skipped += 1
                done.append("матч уже есть")
            else:
                try:
                    match = get_match(match_id, args.region, args.key)
                    save_json(path, match, args.pretty)
                    saved += 1
                    done.append("матч сохранён")
                except RiotError as e:
                    failed += 1
                    done.append(f"матч не вышел: {e}")
                time.sleep(DELAY_SECONDS)

            # Таймлайн качается отдельно: если матч уже лежал с прошлого
            # запуска без --timeline, доберём только недостающее.
            if tl_dir:
                tl_path = os.path.join(tl_dir, f"{match_id}.json")
                if os.path.exists(tl_path):
                    done.append("таймлайн уже есть")
                else:
                    try:
                        timeline = get_timeline(match_id, args.region, args.key)
                        save_json(tl_path, timeline, args.pretty)
                        tl_saved += 1
                        done.append("таймлайн сохранён")
                    except RiotError as e:
                        # Матчи Riot хранит 2 года, таймлайны только 1 год.
                        # Для старой игры 404 — это норма, а не поломка.
                        if e.code == 404:
                            no_timeline += 1
                            done.append("таймлайна нет: игра старше года")
                        else:
                            failed += 1
                            done.append(f"таймлайн не вышел: {e}")
                    time.sleep(DELAY_SECONDS)

            print(f"[{i}/{len(match_ids)}] {match_id} — {', '.join(done)}", flush=True)

        # Индекс нужен, чтобы анализатор не сканировал папку вслепую
        # и чтобы было видно, к какому аккаунту привязана выгрузка.
        index_path = os.path.join(out_dir, "index.json")
        all_ids = sorted(
            f[:-5] for f in os.listdir(out_dir)
            if f.endswith(".json") and f != "index.json"
        )
        with_tl = sorted(
            f[:-5] for f in os.listdir(tl_dir) if f.endswith(".json")
        ) if tl_dir else []

        index = {
            "riot_id": args.riot_id,
            "puuid": puuid,
            "region": args.region,
            "queue": args.queue,
            "fetched_at": time.strftime("%Y-%m-%dT%H:%M:%S"),
            "compact": not args.pretty,
            "match_count": len(all_ids),
            "timeline_count": len(with_tl),
            "match_ids": all_ids,
        }
        # Индекс маленький, его читают глазами — тут отступы уместны.
        with open(index_path, "w", encoding="utf-8") as f:
            json.dump(index, f, ensure_ascii=False, indent=2)

        def folder_mb(d):
            return sum(
                os.path.getsize(os.path.join(d, f)) for f in os.listdir(d)
            ) / 1024 / 1024

        report = (
            f"\nГотово: матчей сохранено {saved}, пропущено {skipped}, ошибок {failed}."
        )
        if tl_dir:
            report += f" Таймлайнов сохранено {tl_saved}."
            if no_timeline:
                report += (
                    f"\nУ {no_timeline} игр таймлайна нет вообще: Riot хранит "
                    f"матчи 2 года, а таймлайны только 1 год."
                )
        report += (
            f"\nВ папке матчей: {len(all_ids)} шт., {folder_mb(out_dir):.0f} МБ."
        )
        if tl_dir:
            report += (
                f"\nВ папке таймлайнов: {len(with_tl)} шт., {folder_mb(tl_dir):.0f} МБ."
            )
        if tl_dir and len(with_tl) < len(all_ids):
            report += (
                f"\nУ {len(all_ids) - len(with_tl)} матчей таймлайна нет — "
                f"перезапусти с --timeline, чтобы добрать."
            )
        report += f"\nИндекс: {index_path}"
        print(report, flush=True)
        print(
            "\nPUUID твоего аккаунта — он нужен, чтобы находить себя среди 10 участников:\n"
            f"  {puuid}",
            flush=True,
        )

    except RiotError as e:
        sys.exit(f"Ошибка: {e}")
    except KeyboardInterrupt:
        sys.exit("\nПрервано. Уже скачанные матчи на месте, перезапуск продолжит с того же места.")


if __name__ == "__main__":
    main()