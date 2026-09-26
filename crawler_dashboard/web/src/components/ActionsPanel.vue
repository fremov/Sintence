<script setup lang="ts">
import { computed, reactive, watch } from 'vue'
import type { Job, Stats, Status } from '../api'
import { ago, duration, fmt } from '../format'

// Остальные действия с базой и паком. Каждое — отдельный скрипт из
// scripts/, с тем же смыслом, что его ключ в командной строке.

const props = defineProps<{ status: Status | null; stats: Stats | null; busy: boolean }>()
const emit = defineEmits<{
  run: [kind: Job['kind'], body: Record<string, unknown>, confirm?: string]
  stop: [job: Job]
}>()

const pack = reactive({ patch: '', minGames: 20 })
const refetch = reactive({ patch: '' })
const seed = reactive({ pages: 1, platform: 'ru' })
const prune = reactive({ patch: '' })

const patches = computed(() => props.stats?.patches.map((p) => p.patch) ?? [])
watch(
  () => props.stats?.packPatch,
  (patch) => {
    if (!patch) return
    if (!refetch.patch) refetch.patch = patch
    if (!prune.patch) prune.patch = patch
  },
  { immediate: true },
)

const jobs = computed(() => props.status?.jobs ?? [])
const running = (kind: Job['kind']) => jobs.value.find((j) => j.kind === kind && j.running) ?? null
const lastDone = (kind: Job['kind']) => jobs.value.find((j) => j.kind === kind && !j.running) ?? null
const writerBusy = computed(
  () =>
    (props.status?.external.length ?? 0) > 0 ||
    jobs.value.some((j) => j.running && ['crawl', 'seed', 'refetch', 'prune'].includes(j.kind)),
)

function result(job: Job | null) {
  if (!job) return ''
  return `${job.exitCode === 0 ? 'готово' : `ошибка, код ${job.exitCode}`} · ${duration(job.elapsedS)} · ${ago(
    job.startedAt + job.elapsedS,
  )}`
}
</script>

<template>
  <section class="card flex flex-col gap-5">
    <h2 class="card-title">Другие действия</h2>

    <!-- Пак -->
    <div class="flex flex-col gap-2">
      <div class="flex flex-wrap items-center gap-2">
        <select v-model="pack.patch" class="field">
          <option value="">патч: авто</option>
          <option v-for="p in patches" :key="p" :value="p">{{ p }}</option>
        </select>
        <input v-model.number="pack.minGames" class="field w-20" type="number" min="5" max="500" title="порог игр" />
        <button class="btn" :disabled="busy || !!running('pack')" @click="emit('run', 'pack', { ...pack })">
          {{ running('pack') ? 'Собирается…' : 'Собрать пак' }}
        </button>
        <span v-if="lastDone('pack')" class="hint">{{ result(lastDone('pack')) }}</span>
      </div>
      <p class="hint">
        <b class="text-text">Собрать пак</b> — <code>build_pack.py</code>: считает по базе советы (страницы рун,
        порядок прокачки, цепочки предметов, заклинания) для каждого чемпиона и роли, отдельно по рангам
        и против каждого противника на линии, и пишет файл в <code>project/data/packs</code> — его читает
        Sintence (новый пак — после перезапуска приложения). «Авто» — самый новый патч, где есть
        {{ fmt(stats?.coverage?.packMinMatches ?? 1000) }}+ матчей с timeline. Число — порог: сколько игр нужно
        сочетанию, чтобы попасть в пак. Можно запускать во время сбора.
      </p>
    </div>

    <!-- Докачка timeline -->
    <div class="flex flex-col gap-2">
      <div class="flex flex-wrap items-center gap-2">
        <select v-model="refetch.patch" class="field">
          <option v-for="p in patches" :key="p" :value="p">{{ p }}</option>
        </select>
        <button
          v-if="!running('refetch')"
          class="btn"
          :disabled="busy || writerBusy || !refetch.patch"
          @click="emit('run', 'refetch', { ...refetch })"
        >
          Докачать timeline
        </button>
        <template v-else>
          <span class="text-gold">
            идёт · {{ fmt(running('refetch')!.progress.done) }} из {{ fmt(running('refetch')!.progress.total) }}
          </span>
          <button class="btn btn-danger" :disabled="busy" @click="emit('stop', running('refetch')!)">
            {{ running('refetch')!.stopping ? 'Снять сразу' : 'Остановить' }}
          </button>
        </template>
        <span v-if="lastDone('refetch') && !running('refetch')" class="hint">{{ result(lastDone('refetch')) }}</span>
      </div>
      <p class="hint">
        <b class="text-text">Докачать timeline</b> — у матчей, скачанных старой версией сборщика, хранилось
        только 12 первых покупок, и в паке не хватало длинных цепочек предметов. Скачивает timeline таких
        матчей заново (один запрос на матч) — до конца списка или до остановки. Уже докачанные не трогает.
      </p>
    </div>

    <!-- Посев -->
    <div class="flex flex-col gap-2">
      <div class="flex flex-wrap items-center gap-2">
        <input v-model.number="seed.pages" class="field w-20" type="number" min="1" max="10" title="страниц на дивизион" />
        <input v-model="seed.platform" class="field w-20" maxlength="5" title="платформа" />
        <button class="btn" :disabled="busy || writerBusy" @click="emit('run', 'seed', { ...seed })">
          {{ running('seed') ? 'Идёт посев…' : 'Посев игроков' }}
        </button>
        <span v-if="lastDone('seed')" class="hint">{{ result(lastDone('seed')) }}</span>
      </div>
      <p class="hint">
        <b class="text-text">Посев игроков</b> — только пополняет очередь обхода, матчи не качает: берёт
        игроков с ладдера каждого дивизиона от Железа до Претендента, чтобы выборка была по всем рангам.
        Первое число — страниц на дивизион (страница — около 200 игроков), второе — платформа. Нужен, если
        очередь игроков пуста; сейчас в ней {{ fmt(stats?.queue) }}.
      </p>
    </div>

    <!-- Очистка -->
    <div class="flex flex-col gap-2">
      <div class="flex flex-wrap items-center gap-2">
        <select v-model="prune.patch" class="field">
          <option v-for="p in patches" :key="p" :value="p">{{ p }}</option>
        </select>
        <button
          class="btn btn-danger"
          :disabled="busy || writerBusy || !prune.patch"
          @click="
            emit('run', 'prune', { ...prune }, `Удалить из базы все матчи, кроме патча ${prune.patch}? Это необратимо.`)
          "
        >
          Оставить только этот патч
        </button>
        <span v-if="lastDone('prune')" class="hint">{{ result(lastDone('prune')) }}</span>
      </div>
      <p class="hint">
        <b class="text-text">Оставить только этот патч</b> — удаляет из базы все матчи других патчей и сжимает
        файл. Необратимо. Уже собранные паки не трогает: пак прошлого патча останется в
        <code>project/data/packs</code>. Имеет смысл, когда база разрослась, а старый патч больше не нужен.
      </p>
    </div>
  </section>
</template>
