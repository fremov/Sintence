<script setup lang="ts">
import { computed, reactive } from 'vue'
import type { Job, Status } from '../api'
import { duration, fmt } from '../format'

// Сбор матчей: запуск и остановка crawl.py. Без срока — идёт, пока его
// не остановят кнопкой, не закроют все вкладки пульта или сам сервер.

const props = defineProps<{ status: Status | null; busy: boolean }>()
const emit = defineEmits<{
  start: [body: Record<string, unknown>]
  stop: [job: Job]
  stopExternal: [pid: number]
}>()

const form = reactive({ perPlayer: 20, days: 14, platform: 'ru', seed: false, noTimeline: false })

const job = computed(() => props.status?.jobs.find((j) => j.kind === 'crawl' && j.running) ?? null)
const last = computed(() => props.status?.jobs.find((j) => j.kind === 'crawl' && !j.running) ?? null)
const external = computed(() => props.status?.external ?? [])
// В базу в каждый момент пишет одна задача: сбор, посев, докачка или очистка.
const writerBusy = computed(
  () =>
    external.value.length > 0 ||
    (props.status?.jobs ?? []).some((j) => j.running && ['crawl', 'seed', 'refetch', 'prune'].includes(j.kind)),
)
</script>

<template>
  <section class="card flex flex-col gap-4">
    <div>
      <h2 class="card-title">Сбор матчей</h2>
      <p class="hint">
        Скачивает ранговые матчи соло-очереди через Riot API в базу <code>project/data/base.sqlite</code>:
        берёт игрока из очереди обхода, скачивает его последние матчи с timeline, а всех участников добавляет
        в очередь. Работает, пока вы его не остановите, не закроете все вкладки пульта или сервер пульта.
      </p>
    </div>

    <!-- Идёт сбор -->
    <div v-if="job" class="rounded-md border border-gold bg-soft p-3">
      <div class="flex items-center justify-between gap-3">
        <div>
          <div class="font-semibold text-gold">Сбор идёт · {{ duration(job.elapsedS) }}</div>
          <div class="hint mt-1">
            новых матчей <b class="text-text">{{ fmt(job.progress.matches) }}</b>
            <template v-if="job.progress.rate">
              · темп <b class="text-text">{{ fmt(job.progress.rate) }}</b>/ч · повторов {{ job.progress.duplicates }}
            </template>
            <template v-else> · первые цифры — после 25 матчей</template>
          </div>
        </div>
        <button class="btn btn-danger" :disabled="busy" @click="emit('stop', job)">
          {{ job.stopping ? 'Снять сразу' : 'Остановить' }}
        </button>
      </div>
      <p class="hint mt-2">
        <template v-if="job.stopping">Останавливается: докачивает текущий матч. «Снять сразу» — без ожидания, база не пострадает.</template>
        <template v-else>Остановка мягкая: текущий матч докачается, потом процесс выйдет.</template>
      </p>
    </div>

    <!-- Сбор из терминала -->
    <div v-for="p in external" :key="p.pid" class="rounded-md border border-warn p-3 text-warn">
      <div>crawl.py запущен из терминала (pid {{ p.pid }}) — второй сбор поверх него не запустится.</div>
      <code class="mt-1 block truncate text-xs text-muted" :title="p.command">{{ p.command }}</code>
      <div class="mt-2 flex items-center gap-3">
        <button class="btn" :disabled="busy" @click="emit('stopExternal', p.pid)">Остановить его</button>
        <span class="hint">Снимается сразу; матчи пишутся по одному, так что база не пострадает.</span>
      </div>
    </div>

    <!-- Параметры -->
    <form v-if="!job" class="grid grid-cols-1 gap-4 sm:grid-cols-3" @submit.prevent="emit('start', { ...form })">
      <label class="flex flex-col gap-1">
        <span class="text-xs text-muted">Матчей с игрока</span>
        <input v-model.number="form.perPlayer" class="field" type="number" min="1" max="100" />
        <span class="hint">
          Сколько последних матчей брать у каждого игрока. Меньше — больше разных игроков в выборке,
          больше — меньше запросов уходит на поиск игроков.
        </span>
      </label>
      <label class="flex flex-col gap-1">
        <span class="text-xs text-muted">Не старше, дней</span>
        <input v-model.number="form.days" class="field" type="number" min="1" max="60" />
        <span class="hint">Более старые матчи пропускаются: сборки прошлого патча советам не нужны. Патч длится ~14 дней.</span>
      </label>
      <label class="flex flex-col gap-1">
        <span class="text-xs text-muted">Платформа</span>
        <input v-model="form.platform" class="field" maxlength="5" />
        <span class="hint">Сервер Riot, где идёт обход: ru, euw1, eun1, na1, kr…</span>
      </label>

      <label class="flex items-start gap-2 sm:col-span-3">
        <input v-model="form.seed" type="checkbox" class="mt-0.5 accent-gold" />
        <span>
          Сначала посев с ладдера
          <span class="hint block">
            Добавить в очередь обхода игроков всех дивизионов перед сбором (~50 запросов). Нужен при первом
            запуске, на новой платформе или когда очередь игроков пуста.
          </span>
        </span>
      </label>
      <label class="flex items-start gap-2 sm:col-span-3">
        <input v-model="form.noTimeline" type="checkbox" class="mt-0.5 accent-gold" />
        <span>
          Без timeline
          <span class="hint block">
            Один запрос на матч вместо двух — вдвое быстрее, но без порядка прокачки и покупок: по таким матчам
            пак советует только руны и заклинания.
          </span>
        </span>
      </label>

      <div class="flex items-center gap-3 sm:col-span-3">
        <button type="submit" class="btn btn-primary" :disabled="busy || writerBusy || !status?.key.found">
          Запустить сбор
        </button>
        <span v-if="!status?.key.found" class="hint text-red">
          Нет ключа Riot: положите его в %LOCALAPPDATA%\Sintence\riot_key.txt.
        </span>
        <span v-else-if="writerBusy" class="hint">В базу сейчас пишет другая задача.</span>
        <span v-else-if="last" class="hint">
          Прошлый сбор: {{ duration(last.elapsedS) }}, новых матчей {{ fmt(last.progress.matches) }}.
        </span>
      </div>
    </form>
  </section>
</template>
