// Форма данных, которую отдаёт sintence.exe на GET /api/live.
// Поля названы так же, как в C++-структурах (core/live_game.h), только
// в camelCase — это граница между двумя языками, и держать её один в один
// проще, чем переименовывать на полпути.

export type Team = 'ORDER' | 'CHAOS'

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
}

export interface LiveGameStats {
  gameMode: string
  mapName: string
  gameTimeSeconds: number
}

export interface LiveGame {
  stats: LiveGameStats
  players: LivePlayer[]
}

// Состояние опроса. Их ровно четыре, и интерфейс обязан показывать все:
//   loading — первый запрос ещё не вернулся;
//   live    — матч идёт, данные свежие;
//   idle    — матча нет (сервер ответил 503), это НЕ ошибка;
//   error   — sintence.exe не отвечает вовсе.
export type FeedState = 'loading' | 'live' | 'idle' | 'error'
