// Разговор с crawler_dashboard/server.py. Формы ответов — как в server.py.

export interface Job {
  id: number
  kind: 'crawl' | 'seed' | 'refetch' | 'prune' | 'pack'
  title: string
  params: Record<string, unknown>
  running: boolean
  stopping: boolean
  startedAt: number
  elapsedS: number
  exitCode: number | null
  progress: { matches?: number; rate?: number; duplicates?: string; done?: number; total?: number }
}

export interface PackFile {
  name: string
  sizeBytes: number
  modifiedAt: number
  buckets?: number
  baseBuckets?: number
  minGames?: number
}

export interface Status {
  jobs: Job[]
  external: Array<{ pid: number; command: string }>
  key: { found: boolean; source: string }
  packs: {
    index: { patch?: string; file?: string; generatedAt?: number }
    files: PackFile[]
  }
  now: number
}

export interface Family {
  threshold: number
  buckets: number
  bucketsOk: number
  share: number
  targets: Array<{ share: number; matches: number | null }>
}

export interface MainRole {
  champion: string
  championId: number
  role: string
  games: number
  share: number
  toPack: number
  toReliable: number
  matchups: number
}

export interface Coverage {
  patch: string
  matches: number
  withTimeline: number
  packMinMatches: number
  observations: number
  champions: number
  thresholds: { pack: number; reliable: number; matchup: number }
  base: Family
  baseReliable: Family
  tier: Family
  matchup: Family
  matchupReliable: Family
  tierGames: Array<{ tier: string; observations: number }>
  mains: MainRole[]
}

export interface Stats {
  empty?: boolean
  error?: string
  matches: number
  withTimeline: number
  participants: number
  players: number
  queue: number
  lastHour: number
  lastDay: number
  lastFetchedAt: number
  rate: { perHour: number; measured: boolean }
  sizeBytes: number
  patches: Array<{ patch: string; matches: number; timeline: number }>
  tiers: Array<{ tier: string; matches: number }>
  coverage: Coverage | null
  packPatch: string
}

export interface LogLine {
  n: number
  t: number
  job: number | null
  text: string
  system: boolean
}

// Идентификатор этой вкладки. Сервер останавливает сбор, когда не остаётся
// ни одной живой вкладки: закрыли браузер — сбор не идёт без присмотра.
export const clientId = crypto.randomUUID()

// Вкладка закрывается — сказать сразу, не дожидаясь, пока сервер заметит
// тишину. sendBeacon доходит и из закрывающейся страницы.
window.addEventListener('pagehide', () => {
  navigator.sendBeacon('/api/bye', JSON.stringify({ client: clientId }))
})

export async function get<T>(path: string): Promise<T> {
  const response = await fetch(path, { cache: 'no-store' })
  const data = await response.json().catch(() => ({}))
  if (!response.ok) throw new Error(data.error || `HTTP ${response.status}`)
  return data as T
}

// Действие на сервере. Заголовок X-Dashboard-Action — пропуск: без него
// сервер отвечает 403, и чужой сайт в этом же браузере сбор не запустит.
export async function post<T = unknown>(path: string, body: unknown = {}): Promise<T> {
  const response = await fetch(path, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json', 'X-Dashboard-Action': '1' },
    body: JSON.stringify(body),
  })
  const data = await response.json().catch(() => ({}))
  if (!response.ok) throw new Error(data.error || `HTTP ${response.status}`)
  return data as T
}
