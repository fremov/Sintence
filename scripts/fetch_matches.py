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


class RiotError(Exception):
    pass


def request_json(url, api_key, attempt=1):
    """GET с обработкой троттлинга и понятными сообщениями об ошибках."""
    req = urllib.request.Request(url, headers={"X-Riot-Token": api_key})
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        if e.code == 429:
            wait = int(e.headers.get("Retry-After", 10))
            print(f"  лимит запросов, жду {wait}с", flush=True)
            time.sleep(wait + 1)
            if attempt < 4:
                return request_json(url, api_key, attempt + 1)
            raise RiotError("лимит запросов не отпускает, попробуй позже")
        if e.code in (401, 403):
            raise RiotError(
                "ключ отклонён (403). Скорее всего он истёк — dev-ключ живёт 24 часа.\n"
                "Обнови его на https://developer.riotgames.com и перезапиши RIOT_API_KEY."
            )
        if e.code == 404:
            raise RiotError("не найдено (404). Проверь Riot ID и регион.")
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
    parser.add_argument("--out", default="project/data/matches", help="куда складывать файлы")
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

        saved, skipped, failed = 0, 0, 0
        for i, match_id in enumerate(match_ids, 1):
            path = os.path.join(out_dir, f"{match_id}.json")
            if os.path.exists(path):
                skipped += 1
                print(f"[{i}/{len(match_ids)}] {match_id} — уже есть", flush=True)
                continue
            try:
                match = get_match(match_id, args.region, args.key)
            except RiotError as e:
                failed += 1
                print(f"[{i}/{len(match_ids)}] {match_id} — не вышло: {e}", flush=True)
                time.sleep(DELAY_SECONDS)
                continue
            with open(path, "w", encoding="utf-8") as f:
                json.dump(match, f, ensure_ascii=False, indent=2)
            saved += 1
            print(f"[{i}/{len(match_ids)}] {match_id} — сохранён", flush=True)
            time.sleep(DELAY_SECONDS)

        # Индекс нужен, чтобы анализатор не сканировал папку вслепую
        # и чтобы было видно, к какому аккаунту привязана выгрузка.
        index_path = os.path.join(out_dir, "index.json")
        index = {
            "riot_id": args.riot_id,
            "puuid": puuid,
            "region": args.region,
            "queue": args.queue,
            "fetched_at": time.strftime("%Y-%m-%dT%H:%M:%S"),
            "match_ids": sorted(
                f[:-5] for f in os.listdir(out_dir)
                if f.endswith(".json") and f != "index.json"
            ),
        }
        with open(index_path, "w", encoding="utf-8") as f:
            json.dump(index, f, ensure_ascii=False, indent=2)

        print(
            f"\nГотово: сохранено {saved}, пропущено {skipped}, ошибок {failed}.\n"
            f"Всего в папке: {len(index['match_ids'])} матчей.\n"
            f"Индекс: {index_path}",
            flush=True,
        )
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
