<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref } from 'vue'
import { clientId, get, post, type Job, type LogLine, type Stats, type Status } from './api'
import ActionsPanel from './components/ActionsPanel.vue'
import CoverageCard from './components/CoverageCard.vue'
import CrawlPanel from './components/CrawlPanel.vue'
import DatabaseOverview from './components/DatabaseOverview.vue'
import LogPanel from './components/LogPanel.vue'
import MissingTable from './components/MissingTable.vue'
import { loadChampions } from './ddragon'
import { duration, fmt } from './format'

// Пульт сбора матчей. Разделы — вкладки: сбор, база, покрытие, нехватка,
// обслуживание, журнал. Раздел хранится в адресе (#coverage), чтобы
// обновление страницы не возвращало на первую вкладку.
//
// Опрос: задачи — раз в 2 с (это же пульс вкладки для сервера), журнал —
// раз в секунду, сводка по базе — раз в 10 с и сразу после конца задачи.

const TABS = [
  { id: 'crawl', label: 'Сбор', hint: 'запуск и остановка сбора матчей' },
  { id: 'database', label: 'База', hint: 'что собрано: матчи, патчи, ранги, паки' },
  { id: 'coverage', label: 'Покрытие', hint: 'насколько патча хватает для советов' },
  { id: 'missing', label: 'Чего не хватает', hint: 'чемпионы и роли с малой выборкой' },
  { id: 'maintenance', label: 'Пак и обслуживание', hint: 'сборка пака, докачка, посев, очистка' },
  { id: 'log', label: 'Журнал', hint: 'вывод скриптов' },
] as const
type TabId = (typeof TABS)[number]['id']

function tabFromHash(): TabId {
  const id = window.location.hash.slice(1)
  return (TABS.find((t) => t.id === id)?.id ?? 'crawl') as TabId
}
const tab = ref<TabId>(tabFromHash())
function openTab(id: TabId) {
  tab.value = id
  history.replaceState(null, '', `#${id}`)
}
window.addEventListener('hashchange', () => (tab.value = tabFromHash()))

const status = ref<Status | null>(null)
const stats = ref<Stats | null>(null)
const lines = ref<LogLine[]>([])
const offline = ref(false)
const patch = ref('')
const busy = ref(false)
const note = ref<{ text: string; error: boolean } | null>(null)
const unreadErrors = ref(0)

let cursor = 0
let running = new Set<number>()

async function pollStatus() {
  try {
    status.value = await get<Status>(`/api/status?client=${clientId}`)
    offline.value = false
  } catch {
    offline.value = true
    return
  }
  const now = new Set(status.value.jobs.filter((j) => j.running).map((j) => j.id))
  const finished = [...running].some((id) => !now.has(id))
  running = now
  if (finished) void pollStats()
}

async function pollStats() {
  try {
    stats.value = await get<Stats>(`/api/stats${patch.value ? `?patch=${encodeURIComponent(patch.value)}` : ''}`)
  } catch {
    // Сводка — не главное: задачи и журнал работают и без неё.
  }
}

const ERROR = /traceback|error|ошибк/i
async function pollLog() {
  try {
    const data = await get<{ lines: LogLine[]; cursor: number }>(`/api/log?since=${cursor}`)
    if (!data.lines.length) return
    cursor = data.cursor
    lines.value = [...lines.value, ...data.lines].slice(-3000)
    if (tab.value !== 'log') unreadErrors.value += data.lines.filter((l) => !l.system && ERROR.test(l.text)).length
  } catch {
    // Сервер недоступен — об этом скажет шапка.
  }
}

async function act(run: () => Promise<unknown>, done: string) {
  busy.value = true
  try {
    await run()
    note.value = { text: done, error: false }
  } catch (error) {
    note.value = { text: (error as Error).message, error: true }
  } finally {
    busy.value = false
    void pollStatus()
    const shown = note.value
    setTimeout(() => {
      if (note.value === shown) note.value = null
    }, 5000)
  }
}

function startCrawl(body: Record<string, unknown>) {
  void act(() => post('/api/run/crawl', body), 'Сбор запущен')
}
function stopJob(job: Job) {
  void act(() => post(`/api/jobs/${job.id}/stop`), job.stopping ? 'Снято' : 'Останавливаю после текущего матча')
}
function stopExternal(pid: number) {
  if (!confirm(`Остановить crawl.py (pid ${pid})?`)) return
  void act(() => post('/api/external/stop', { pid }), 'Остановлен')
}
function run(kind: Job['kind'], body: Record<string, unknown>, question?: string) {
  if (question && !confirm(question)) return
  const titles = { pack: 'Сборка пака запущена', refetch: 'Докачка запущена', seed: 'Посев запущен', prune: 'Очистка запущена', crawl: 'Сбор запущен' }
  void act(() => post(`/api/run/${kind}`, body), titles[kind])
}
function pickPatch(value: string) {
  patch.value = value
  void pollStats()
  openTab('coverage')
}

const runningJobs = computed(() => status.value?.jobs.filter((j) => j.running) ?? [])
const crawlJob = computed(() => runningJobs.value.find((j) => j.kind === 'crawl') ?? null)

const timers: number[] = []
onMounted(() => {
  void loadChampions()
  void pollStatus()
  void pollStats()
  void pollLog()
  timers.push(window.setInterval(pollStatus, 2000), window.setInterval(pollLog, 1000), window.setInterval(pollStats, 10000))
})
onUnmounted(() => timers.forEach((t) => window.clearInterval(t)))

function selectTab(id: TabId) {
  openTab(id)
  if (id === 'log') unreadErrors.value = 0
}
</script>

<template>
  <div class="flex min-h-screen flex-col">
    <!-- Шапка: видна на всех вкладках -->
    <header class="sticky top-0 z-10 border-b border-edge bg-bg/95 backdrop-blur">
      <div class="mx-auto flex max-w-[1400px] flex-wrap items-center justify-between gap-3 px-5 py-3">
        <div class="flex items-center gap-3">
          <span
            class="grid size-9 place-items-center rounded-lg border border-gold bg-gradient-to-b from-[#222a36] to-bg font-serif text-xl font-bold text-gold"
          >
            S
          </span>
          <div>
            <div class="text-base font-semibold">Сбор матчей</div>
            <div class="text-xs text-muted">пульт краулера Sintence · project/data/base.sqlite</div>
          </div>
        </div>
        <div class="flex flex-wrap items-center gap-2 text-xs">
          <span v-if="offline" class="rounded-full border border-red/50 px-2.5 py-0.5 text-red">сервер пульта не отвечает</span>
          <template v-else-if="status">
            <span
              class="rounded-full border px-2.5 py-0.5"
              :class="status.key.found ? 'border-ok/50 text-ok' : 'border-red/50 text-red'"
              :title="status.key.source"
            >
              {{ status.key.found ? 'ключ Riot найден' : 'ключа Riot нет' }}
            </span>
            <button
              v-if="crawlJob"
              class="cursor-pointer rounded-full border border-gold px-2.5 py-0.5 text-gold"
              @click="selectTab('crawl')"
            >
              сбор идёт · {{ duration(crawlJob.elapsedS) }} · +{{ fmt(crawlJob.progress.matches) }}
            </button>
            <span v-else-if="status.external.length" class="rounded-full border border-warn px-2.5 py-0.5 text-warn">
              сбор идёт из терминала
            </span>
            <span v-else class="rounded-full border border-edge px-2.5 py-0.5 text-muted">сбор не идёт</span>
            <span
              v-for="job in runningJobs.filter((j) => j.kind !== 'crawl')"
              :key="job.id"
              class="rounded-full border border-gold px-2.5 py-0.5 text-gold"
            >
              {{ job.title }}…
            </span>
          </template>
        </div>
      </div>
      <nav class="mx-auto flex max-w-[1400px] gap-1 overflow-x-auto px-5">
        <button
          v-for="t in TABS"
          :key="t.id"
          class="-mb-px cursor-pointer border-b-2 px-3.5 py-2 whitespace-nowrap"
          :class="tab === t.id ? 'border-gold text-gold' : 'border-transparent text-muted hover:text-text'"
          :title="t.hint"
          @click="selectTab(t.id)"
        >
          {{ t.label }}
          <span v-if="t.id === 'log' && unreadErrors" class="ml-1 rounded-full bg-red px-1.5 text-[11px] text-bg">
            {{ unreadErrors }}
          </span>
        </button>
      </nav>
    </header>

    <main class="mx-auto flex w-full max-w-[1400px] flex-col gap-4 px-5 py-5">
      <div
        v-if="note"
        class="rounded-md border px-3 py-2 text-sm"
        :class="note.error ? 'border-red/50 text-red' : 'border-ok/50 text-ok'"
      >
        {{ note.text }}
      </div>

      <!-- Сбор -->
      <template v-if="tab === 'crawl'">
        <CrawlPanel :status="status" :busy="busy" @start="startCrawl" @stop="stopJob" @stop-external="stopExternal" />
        <section v-if="stats && !stats.empty" class="grid grid-cols-2 gap-3 md:grid-cols-4">
          <div class="rounded-lg border border-edge bg-panel px-4 py-3">
            <div class="text-[22px] font-semibold">{{ fmt(stats.matches) }}</div>
            <div class="text-xs text-muted">матчей в базе</div>
          </div>
          <div class="rounded-lg border border-edge bg-panel px-4 py-3">
            <div class="text-[22px] font-semibold">{{ fmt(stats.lastHour) }}</div>
            <div class="text-xs text-muted">за последний час</div>
          </div>
          <div class="rounded-lg border border-edge bg-panel px-4 py-3">
            <div class="text-[22px] font-semibold">{{ fmt(stats.rate.perHour) }}/ч</div>
            <div class="text-xs text-muted">темп сбора</div>
          </div>
          <div class="rounded-lg border border-edge bg-panel px-4 py-3">
            <div class="text-[22px] font-semibold">{{ fmt(stats.queue) }}</div>
            <div class="text-xs text-muted">игроков в очереди обхода</div>
          </div>
        </section>
        <p class="hint">
          Подробности — на вкладках «База», «Покрытие» и «Чего не хватает»; вывод сборщика — в «Журнале».
        </p>
      </template>

      <!-- База -->
      <template v-else-if="tab === 'database'">
        <DatabaseOverview
          v-if="stats && !stats.empty"
          :stats="stats"
          :status="status"
          :shown-patch="stats.coverage?.patch ?? ''"
          @pick-patch="pickPatch"
        />
        <div v-else class="card hint">База пуста — запустите сбор с посевом на вкладке «Сбор».</div>
      </template>

      <!-- Покрытие -->
      <template v-else-if="tab === 'coverage'">
        <div v-if="stats?.patches.length" class="flex flex-wrap items-center gap-2">
          <span class="text-xs text-muted">Патч:</span>
          <button
            v-for="p in stats.patches"
            :key="p.patch"
            class="btn px-2.5 py-1 text-xs"
            :class="{ 'border-gold text-gold': p.patch === stats.coverage?.patch }"
            @click="pickPatch(p.patch)"
          >
            {{ p.patch }}<span v-if="p.patch === stats.packPatch" class="ml-1 text-muted">· пак</span>
          </button>
        </div>
        <CoverageCard v-if="stats?.coverage" :coverage="stats.coverage" :rate="stats.rate.perHour" />
        <div v-else class="card hint">Нет данных по патчу.</div>
      </template>

      <!-- Чего не хватает -->
      <template v-else-if="tab === 'missing'">
        <MissingTable v-if="stats?.coverage" :coverage="stats.coverage" />
        <div v-else class="card hint">Нет данных по патчу.</div>
      </template>

      <!-- Обслуживание -->
      <ActionsPanel v-else-if="tab === 'maintenance'" :status="status" :stats="stats" :busy="busy" @run="run" @stop="stopJob" />

      <!-- Журнал -->
      <LogPanel v-else-if="tab === 'log'" :lines="lines" />

      <p class="hint mt-2 text-center">
        Сбор останавливается сам, если закрыть все вкладки пульта или окно сервера — без присмотра он не идёт.
      </p>
    </main>
  </div>
</template>
