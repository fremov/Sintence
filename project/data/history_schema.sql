-- История матчей игроков: то, что показывает окно профиля.
--
-- Сейчас это SQLite (project/data/history.sqlite), потом — база на сервере
-- (MySQL или другая). Поэтому схема пишется на общем подмножестве SQL:
--   - только INTEGER, BIGINT, VARCHAR(n), TEXT; никаких AUTOINCREMENT,
--     WITHOUT ROWID, JSON1 и прочих особенностей SQLite;
--   - ключи естественные (match_id, puuid) — их даёт Riot, и они одинаковы
--     на любом хранилище;
--   - время — BIGINT миллисекунд Unix, а не DATETIME: у SQLite и MySQL
--     разные представления дат, число одинаково везде.
-- Переезд на MySQL: TEXT -> MEDIUMTEXT у raw_json (матч весит 50-150 КБ,
-- TEXT в MySQL ограничен 64 КБ), остальное как есть.
--
-- Сырой ответ Riot хранится целиком (raw_json) рядом с разобранными
-- колонками. Понадобится поле, которое сейчас не извлекается, — оно
-- достаётся из базы, а не перекачивается из Riot.
--
-- Эту схему читает C++ (data/sqlite_match_store) — она встраивается в
-- бинарник при сборке. Меняешь схему — поднимай schema_version.

CREATE TABLE IF NOT EXISTS meta (
    meta_key   VARCHAR(64) NOT NULL PRIMARY KEY,
    meta_value VARCHAR(255) NOT NULL
);

-- Матч: шапка и сырой ответ match-v5.
CREATE TABLE IF NOT EXISTS matches (
    match_id     VARCHAR(32) NOT NULL PRIMARY KEY,  -- "RU_528252891"
    platform     VARCHAR(8)  NOT NULL,              -- "RU"
    queue_id     INTEGER     NOT NULL,              -- 420 — одиночная
    game_mode    VARCHAR(32) NOT NULL,              -- "CLASSIC", "ARAM"
    game_version VARCHAR(32) NOT NULL,              -- "15.12.688.6522"
    patch        VARCHAR(8)  NOT NULL,              -- "15.12"
    started_at   BIGINT      NOT NULL,              -- мс Unix, gameStartTimestamp
    duration_s   INTEGER     NOT NULL,
    fetched_at   BIGINT      NOT NULL,              -- когда скачан
    raw_json     TEXT        NOT NULL
);

-- Десять строк на матч. Всё, что нужно списку игр и таблице матча,
-- без разбора raw_json.
CREATE TABLE IF NOT EXISTS participants (
    match_id      VARCHAR(32) NOT NULL,
    puuid         VARCHAR(80) NOT NULL,
    riot_id       VARCHAR(64) NOT NULL,  -- "Имя#TAG", как было в этом матче
    team_id       INTEGER     NOT NULL,  -- 100 синие, 200 красные
    win           INTEGER     NOT NULL,  -- 0/1
    champion_id   INTEGER     NOT NULL,
    champion      VARCHAR(32) NOT NULL,  -- "Vladimir"
    role          VARCHAR(16) NOT NULL,  -- teamPosition: "MIDDLE" или ""
    champ_level   INTEGER     NOT NULL,
    kills         INTEGER     NOT NULL,
    deaths        INTEGER     NOT NULL,
    assists       INTEGER     NOT NULL,
    cs            INTEGER     NOT NULL,  -- миньоны + нейтральные
    gold          INTEGER     NOT NULL,
    damage_dealt  INTEGER     NOT NULL,  -- по чемпионам
    damage_taken  INTEGER     NOT NULL,
    vision_score  INTEGER     NOT NULL,
    wards_placed  INTEGER     NOT NULL,
    wards_killed  INTEGER     NOT NULL,
    item0 INTEGER NOT NULL, item1 INTEGER NOT NULL, item2 INTEGER NOT NULL,
    item3 INTEGER NOT NULL, item4 INTEGER NOT NULL, item5 INTEGER NOT NULL,
    item6 INTEGER NOT NULL,              -- тринкет
    spell1        INTEGER     NOT NULL,
    spell2        INTEGER     NOT NULL,
    keystone      INTEGER     NOT NULL,
    primary_style INTEGER     NOT NULL,
    sub_style     INTEGER     NOT NULL,
    PRIMARY KEY (match_id, puuid)
);

-- «Последние игры игрока» — главный запрос окна профиля.
CREATE INDEX IF NOT EXISTS participants_by_puuid ON participants (puuid, match_id);

-- Timeline матча: порядок покупок, прокачки и золото по минутам. Второй
-- запрос к Riot на матч — качается только когда открыли подробности.
CREATE TABLE IF NOT EXISTS timelines (
    match_id   VARCHAR(32) NOT NULL PRIMARY KEY,
    fetched_at BIGINT      NOT NULL,
    raw_json   TEXT        NOT NULL
);
