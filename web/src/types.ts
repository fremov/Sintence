// Форма данных, которую отдаёт sintence.exe на GET /api/live.
// Поля названы так же, как в C++-структурах (core/live_game.h), только
// в camelCase — это граница между двумя языками, и держать её один в один
// проще, чем переименовывать на полпути.

export type Team = 'ORDER' | 'CHAOS'

export interface LiveItem {
  itemId: number
  name: string
  slot: number
  count: number
  price: number
}

export interface LivePlayer {
  championName: string
  riotId: string
  position: string
  team: Team
  level: number
  kills: number
  deaths: number
  assists: number
  creepScore: number
  isBot: boolean
  isDead: boolean
  items: LiveItem[]
}

export interface LiveAbility {
  slot: string
  name: string
  level: number
}

export interface LiveRunes {
  keystone: string
  primaryTree: string
  secondaryTree: string
  minorRunes: string[]
}

// Есть только про того, за кем клиент. Про остальных Live Client
// способности и руны не отдаёт — и не должен.
export interface LiveActivePlayer {
  riotId: string
  level: number
  currentGold: number
  abilities: LiveAbility[]
  runes: LiveRunes
}

export interface LiveGameStats {
  gameMode: string
  mapName: string
  gameTimeSeconds: number
}

export interface LiveGame {
  stats: LiveGameStats
  players: LivePlayer[]
  // null в режиме наблюдателя и в реплее — это рабочее состояние.
  activePlayer: LiveActivePlayer | null
}

// Профиль игрока вне матча: GET /api/profiles.
// Собирается из Riot API по ключу, приходит порциями — десять игроков
// это тридцать запросов, и лимит ключа растягивает их на полминуты.
export interface RankedStats {
  tier: string
  division: string
  leaguePoints: number
  wins: number
  losses: number
}

export interface ChampionMastery {
  championId: number
  level: number
  points: number
  lastPlayTimeMs: number
}

export interface PlayerProfile {
  riotId: string
  // null означает «не играл в соло-очереди», а не «не смогли узнать».
  solo: RankedStats | null
  masteries: ChampionMastery[]
}

export interface ProfilesProgress {
  done: number
  total: number
  running: boolean
}

export interface ProfilesResponse {
  progress: ProfilesProgress
  profiles: PlayerProfile[]
}

// Состояние опроса. Их ровно четыре, и интерфейс обязан показывать все:
//   loading — первый запрос ещё не вернулся;
//   live    — матч идёт, данные свежие;
//   idle    — матча нет (сервер ответил 503), это НЕ ошибка;
//   error   — sintence.exe не отвечает вовсе.
export type FeedState = 'loading' | 'live' | 'idle' | 'error'

// Советы по игре: GET /api/preferences.
//
// Считаются заранее из базы матчей (scripts/crawl.py + build_pack.py)
// и только ДЛЯ АКТИВНОГО ИГРОКА: чужие руны и порядок прокачки Live Client
// не отдаёт никому, и показывать их на карточке противника было бы
// подсказкой того, чего игрок видеть не должен.
export interface RunePage {
  keystone: string
  primaryTree: string
  primary: string[]    // три малые основного древа
  secondaryTree: string
  secondary: string[]  // две малые дополнительного
  shards: string[]     // атака, гибкий, защита
}

export interface PreferenceVariant {
  name: string
  games: number
  share: number
  winrate: number
  // Нижняя граница Вильсона: вариант с пятью играми не обгоняет
  // проверенный вариант с сотнями.
  winrateLow: number
  // Цепочка: повышения способностей или предметы по порядку покупки.
  steps?: string[]
  // Только у страниц рун.
  page?: RunePage
}

export interface Matchup {
  versus: string        // каноническое имя противника
  versusName: string    // как его показывает клиент
  versusRole: string
  lane: boolean         // он стоит с тобой на линии
  // false означает «по этому матчапу выборки не набралось, показана
  // статистика против всех». Врать об этом нельзя.
  exact: boolean
  opponent: string
  tier: string
  patch: string
  games: number
  winrate: number
  runePages: PreferenceVariant[]
  skillOrders: PreferenceVariant[]
  itemChains: PreferenceVariant[]
}

export interface PreferencesResponse {
  patch: string
  region: string
  you: {
    riotId: string
    champion: string
    championName: string
    role: string
  }
  matchups: Matchup[]
}
