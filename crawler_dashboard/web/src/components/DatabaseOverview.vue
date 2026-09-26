<script setup lang="ts">
import { computed } from 'vue'
import type { Stats, Status } from '../api'
import { TIER, ago, bytes, fmt } from '../format'
import ProgressBar from './ProgressBar.vue'

// Что лежит в базе: показатели, патчи, ранги посева, паки.

const props = defineProps<{ stats: Stats; status: Status | null; shownPatch: string }>()
const emit = defineEmits<{ pickPatch: [patch: string] }>()

const kpis = computed(() => {
  const s = props.stats
  return [
    { value: fmt(s.matches), label: 'матчей в базе', hint: `с timeline ${fmt(s.withTimeline)}` },
    { value: fmt(s.participants), label: 'наблюдений', hint: 'чемпион в матче — десять на матч' },
    { value: fmt(s.lastHour), label: 'за последний час', hint: `за сутки ${fmt(s.lastDay)}` },
    {
      value: `${fmt(s.rate.perHour)}/ч`,
      label: 'темп сбора',
      hint: s.rate.measured ? 'по последним 300 матчам' : 'оценка для персонального ключа',
    },
    { value: fmt(s.queue), label: 'игроков в очереди', hint: `всего известно ${fmt(s.players)}` },
    { value: bytes(s.sizeBytes), label: 'размер базы', hint: `последний матч ${ago(s.lastFetchedAt)}` },
  ]
})

const maxPatch = computed(() => Math.max(1, ...props.stats.patches.map((p) => p.matches)))
const packMin = computed(() => props.stats.coverage?.packMinMatches ?? 1000)
const maxTier = computed(() => Math.max(1, ...props.stats.tiers.map((t) => t.matches)))

const packs = computed(() => props.status?.packs ?? null)
const current = computed(() => packs.value?.files.find((f) => f.name === packs.value?.index.file) ?? null)
</script>

<template>
  <section class="grid grid-cols-2 gap-3 md:grid-cols-3 xl:grid-cols-6">
    <div v-for="k in kpis" :key="k.label" class="rounded-lg border border-edge bg-panel px-4 py-3">
      <div class="text-[22px] font-semibold">{{ k.value }}</div>
      <div class="text-xs text-muted">{{ k.label }}</div>
      <div class="text-[11px] text-muted/80">{{ k.hint }}</div>
    </div>
  </section>

  <section class="grid gap-4 lg:grid-cols-2">
    <div class="card">
      <h2 class="card-title">Патчи</h2>
      <p class="hint mb-3">
        Пак собирается по самому новому патчу, где набралось {{ fmt(packMin) }}+ матчей с timeline (риска на полосе).
        Щелчок по строке — показать ниже покрытие этого патча.
      </p>
      <table class="w-full">
        <thead>
          <tr class="text-left text-[11px] text-muted">
            <th class="border-b border-edge px-2 py-1.5 font-medium">Патч</th>
            <th class="border-b border-edge px-2 py-1.5" />
            <th class="border-b border-edge px-2 py-1.5 text-right font-medium">Матчей</th>
            <th class="border-b border-edge px-2 py-1.5 text-right font-medium">С timeline</th>
          </tr>
        </thead>
        <tbody>
          <tr
            v-for="p in stats.patches"
            :key="p.patch"
            class="cursor-pointer hover:bg-soft"
            :class="{ 'bg-soft': p.patch === shownPatch }"
            @click="emit('pickPatch', p.patch)"
          >
            <td class="border-b border-[#1a2029] px-2 py-1.5">
              {{ p.patch }}<span v-if="p.patch === stats.packPatch" class="tag">пак</span>
            </td>
            <td class="w-[45%] border-b border-[#1a2029] px-2 py-1.5">
              <ProgressBar :value="p.matches" :max="Math.max(maxPatch, packMin)" :tone="p.timeline >= packMin ? 'ok' : 'gold'" :mark="packMin" />
            </td>
            <td class="border-b border-[#1a2029] px-2 py-1.5 text-right">{{ fmt(p.matches) }}</td>
            <td class="border-b border-[#1a2029] px-2 py-1.5 text-right">{{ fmt(p.timeline) }}</td>
          </tr>
        </tbody>
      </table>

      <h3 class="mt-5 mb-2 text-xs font-semibold tracking-wide text-muted uppercase">Ранг посева матчей</h3>
      <p class="hint mb-2">
        Ранга участников в match-v5 нет, поэтому матчу приписан ранг игрока, от которого сборщик к нему пришёл.
      </p>
      <div class="grid gap-1.5">
        <div v-for="t in stats.tiers" :key="t.tier" class="grid grid-cols-[110px_1fr_60px] items-center gap-3 text-xs">
          <span>{{ TIER[t.tier] ?? t.tier }}</span>
          <ProgressBar :value="t.matches" :max="maxTier" />
          <span class="text-right">{{ fmt(t.matches) }}</span>
        </div>
      </div>
    </div>

    <div class="card">
      <h2 class="card-title">Паки</h2>
      <p v-if="packs?.index.file" class="mb-3">
        Sintence читает <b>{{ packs.index.file }}</b> — патч {{ packs.index.patch }}, собран
        {{ ago(packs.index.generatedAt) }}<template v-if="current">
          · сочетаний в паке {{ fmt(current.buckets) }}, из них «чемпион + роль против всех» {{ fmt(current.baseBuckets) }},
          порог {{ current.minGames }} игр</template>.
      </p>
      <p v-else class="hint mb-3">Пака ещё нет — соберите его кнопкой «Собрать пак».</p>
      <table class="w-full">
        <thead>
          <tr class="text-left text-[11px] text-muted">
            <th class="border-b border-edge px-2 py-1.5 font-medium">Файл</th>
            <th class="border-b border-edge px-2 py-1.5 text-right font-medium">Сочетаний</th>
            <th class="border-b border-edge px-2 py-1.5 text-right font-medium">Чемп. + роль</th>
            <th class="border-b border-edge px-2 py-1.5 text-right font-medium">Размер</th>
            <th class="border-b border-edge px-2 py-1.5 text-right font-medium">Собран</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="f in packs?.files ?? []" :key="f.name" :class="{ 'bg-soft': f.name === packs?.index.file }">
            <td class="border-b border-[#1a2029] px-2 py-1.5">
              {{ f.name }}<span v-if="f.name === packs?.index.file" class="tag">в приложении</span>
            </td>
            <td class="border-b border-[#1a2029] px-2 py-1.5 text-right">{{ fmt(f.buckets) }}</td>
            <td class="border-b border-[#1a2029] px-2 py-1.5 text-right">{{ fmt(f.baseBuckets) }}</td>
            <td class="border-b border-[#1a2029] px-2 py-1.5 text-right">{{ bytes(f.sizeBytes) }}</td>
            <td class="border-b border-[#1a2029] px-2 py-1.5 text-right">{{ ago(f.modifiedAt) }}</td>
          </tr>
        </tbody>
      </table>
      <p class="hint mt-3">
        Сочетание — чемпион + роль, отдельно по ранговой корзине и против конкретного противника на линии.
        В пак попадает сочетание, набравшее порог игр; для остальных Sintence откатывается к более общему.
      </p>
    </div>
  </section>
</template>
