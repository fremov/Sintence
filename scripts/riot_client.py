#!/usr/bin/env python3
"""
Клиент Riot API для офлайн-сбора: ключ, лимитер, повторы.

Только стандартная библиотека. Используется crawl.py и build_pack.py.

Лимиты personal-ключа общие на все эндпоинты: 20 запросов в секунду
и 100 за 2 минуты. Второе и есть потолок: 50 запросов в минуту,
3000 в час. Лимитер держит оба окна скользящими — Riot считает именно
так, и фиксированный интервал позволил бы отправить 200 запросов
на стыке двух окон.
"""

import collections
import json
import os
import pathlib
import random
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

# Cloudflare перед api.riotgames.com отбивает дефолтный User-Agent
# "Python-urllib/3.x": отвечает 403 с телом "error code: 1010".
# Это не Riot и не ключ — это фильтр по UA.
HEADERS = {
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/128.0.0.0 Safari/537.36"
    ),
    "Accept": "application/json",
}

# Платформа -> надрегиональный кластер для match-v5 и account-v1.
PLATFORM_CLUSTER = {
    "ru": "europe",
    "euw1": "europe",
    "eun1": "europe",
    "tr1": "europe",
    "na1": "americas",
    "br1": "americas",
    "la1": "americas",
    "la2": "americas",
    "kr": "asia",
    "jp1": "asia",
    "oc1": "sea",
}


class RiotError(RuntimeError):
    """Ошибка, после которой продолжать бессмысленно: нет ключа, 401, 403."""


def load_key() -> str:
    """Ключ из SINTENCE_RIOT_KEY или %LOCALAPPDATA%\\Sintence\\riot_key.txt."""
    key = os.environ.get("SINTENCE_RIOT_KEY", "").strip()
    if key:
        return key

    local = os.environ.get("LOCALAPPDATA")
    if local:
        path = pathlib.Path(local) / "Sintence" / "riot_key.txt"
        if path.is_file():
            key = path.read_text(encoding="utf-8-sig").splitlines()[0].strip()
            if key:
                return key

    raise RiotError(
        "ключ не найден: задай SINTENCE_RIOT_KEY или положи его "
        "в %LOCALAPPDATA%\\Sintence\\riot_key.txt"
    )


class RateLimiter:
    """Скользящие окна: (лимит, период в секундах).

    Лимиты ключа — 20/1с и 100/2мин, но берутся 18 и 95. Запас нужен
    потому, что окна считает Riot по своим часам и по времени ПРИЁМА
    запроса, а мы отмечаем по времени отправки: на границе окна разница
    в сотни миллисекунд даёт 429 и штрафную паузу до полутора минут.
    Три процента пропускной способности дешевле одной такой паузы.
    """

    def __init__(self, windows=((18, 1.0), (95, 120.0))):
        self._windows = [(limit, period, collections.deque()) for limit, period in windows]
        self._blocked_until = 0.0

    def delay(self, now: float) -> float:
        wait = max(0.0, self._blocked_until - now)
        for limit, period, hits in self._windows:
            edge = now - period
            while hits and hits[0] <= edge:
                hits.popleft()
            if len(hits) >= limit:
                wait = max(wait, hits[0] + period - now)
        return wait

    def record(self, now: float) -> None:
        for _, _, hits in self._windows:
            hits.append(now)

    def block_for(self, seconds: float) -> None:
        self._blocked_until = max(self._blocked_until, time.monotonic() + seconds)

    def used(self):
        return [len(hits) for _, _, hits in self._windows]

    def acquire(self) -> None:
        wait = self.delay(time.monotonic())
        if wait > 0:
            time.sleep(wait)
        self.record(time.monotonic())


class RiotClient:
    """Синхронный клиент: один поток, один лимитер, повторы на 429 и 5xx."""

    def __init__(self, key: str, platform: str = "ru", timeout: float = 15.0):
        self.key = key
        self.platform = platform
        self.cluster = PLATFORM_CLUSTER.get(platform, "europe")
        self.timeout = timeout
        self.limiter = RateLimiter()
        self.requests = 0
        self.errors = 0

    # --- низкий уровень -------------------------------------------------

    def _get(self, host: str, path: str, params=None):
        url = f"https://{host}{path}"
        if params:
            url += "?" + urllib.parse.urlencode(params)

        request = urllib.request.Request(url, headers={**HEADERS, "X-Riot-Token": self.key})

        for attempt in range(5):
            self.limiter.acquire()
            self.requests += 1
            try:
                with urllib.request.urlopen(request, timeout=self.timeout) as response:
                    return json.loads(response.read().decode("utf-8"))
            except urllib.error.HTTPError as error:
                if error.code in (401, 403):
                    raise RiotError(
                        f"{error.code}: ключ не принят Riot (ключ в журнал не пишется)"
                    ) from error
                if error.code == 404:
                    return None
                if error.code == 429:
                    retry_after = float(error.headers.get("Retry-After", "1") or 1)
                    print(f"  429: пауза {retry_after:.0f} с", flush=True)
                    self.limiter.block_for(retry_after)
                    continue
                if 500 <= error.code < 600:
                    pause = 2 ** attempt + random.random()
                    print(f"  {error.code} от Riot: повтор через {pause:.1f} с", flush=True)
                    time.sleep(pause)
                    continue
                self.errors += 1
                print(f"  код {error.code} на {path}", file=sys.stderr, flush=True)
                return None
            except (urllib.error.URLError, TimeoutError, json.JSONDecodeError) as error:
                pause = 2 ** attempt + random.random()
                print(f"  сеть: {error}; повтор через {pause:.1f} с", flush=True)
                time.sleep(pause)

        self.errors += 1
        return None

    def platform_get(self, path: str, params=None):
        return self._get(f"{self.platform}.api.riotgames.com", path, params)

    def cluster_get(self, path: str, params=None):
        return self._get(f"{self.cluster}.api.riotgames.com", path, params)

    # --- эндпоинты, которые нужны сбору ---------------------------------

    def apex_league(self, tier: str, queue: str = "RANKED_SOLO_5x5"):
        """challenger / grandmaster / master — отдельные эндпоинты, без страниц."""
        path = f"/lol/league/v4/{tier}leagues/by-queue/{queue}"
        data = self.platform_get(path)
        if not data:
            return []
        return [(entry["puuid"], tier.upper(), entry.get("rank", "I"))
                for entry in data.get("entries", []) if entry.get("puuid")]

    def league_page(self, tier: str, division: str, page: int,
                    queue: str = "RANKED_SOLO_5x5"):
        """Дивизион постранично: до 205 записей с puuid и рангом."""
        path = f"/lol/league/v4/entries/{queue}/{tier}/{division}"
        data = self.platform_get(path, {"page": page})
        if not data:
            return []
        return [(entry["puuid"], tier, entry.get("rank", division))
                for entry in data if entry.get("puuid")]

    def match_ids(self, puuid: str, count: int, start_time: int, queue: int = 420):
        path = f"/lol/match/v5/matches/by-puuid/{puuid}/ids"
        params = {"queue": queue, "start": 0, "count": count, "startTime": start_time}
        return self.cluster_get(path, params) or []

    def match(self, match_id: str):
        return self.cluster_get(f"/lol/match/v5/matches/{match_id}")

    def timeline(self, match_id: str):
        return self.cluster_get(f"/lol/match/v5/matches/{match_id}/timeline")
