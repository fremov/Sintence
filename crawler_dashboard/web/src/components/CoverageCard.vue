<script setup lang="ts">
import { computed } from 'vue'
import type { Coverage, Family } from '../api'
import { fmt, hours, pct } from '../format'
import ProgressBar from './ProgressBar.vue'

// Насколько патч покрыт выборкой — теми же сочетаниями, что строит пак.

const props = defineProps<{ coverage: Coverage; rate: number }>()

type Key = 'base' | 'baseReliable' | 'tier' | 'matchup' | 'matchupReliable'
const FAMILIES: Array<{ key: Key; title: string; describe: (t: number) => string }> = [
  { key: 'base', title: 'Чемпион + роль в паке', describe: (t) => `сочетание попадает в пак с ${t}+ играми` },
  { key: 'baseReliable', title: 'Чемпион + роль надёжно', describe: (t) => `${t}+ игр: варианты рун и сборок перестают прыгать` },
  { key: 'tier', title: 'По ранговым корзинам', describe: (t) => `совет для своего ранга, ${t}+ игр в корзине` },
  { key: 'matchup', title: 'Матчапы в паке', describe: (t) => `против конкретного противника на линии, ${t}+ игр` },
  { key: 'matchupReliable', title: 'Матчапы надёжно', describe: (t) => `против конкретного противника, ${t}+ игр` },
]

function tone(f: Family) {
  if (f.share >= 0.9) return 'ok'
  if (f.share >= 0.7) return 'gold'
  if (f.share >= 0.4) return 'warn'
  return 'bad'
}

function target(t: Family['targets'][number]) {
  if (t.matches === null) return { done: false, text: `${pct(t.share)} игр — оценить нельзя` }
  const more = Math.max(0, t.matches - props.coverage.matches)
  if (!more) return { done: true, text: `${pct(t.share)} игр — уже есть` }
  return {
    done: false,
    text: `${pct(t.share)} игр — нужно ≈ ${fmt(t.matches)} матчей: ещё ${fmt(more)}, ≈ ${hours(more / props.rate)}`,
  }
}

const cards = computed(() => FAMILIES.map((f) => ({ ...f, family: props.coverage[f.key] })))
</script>

<template>
  <section class="card">
    <div class="mb-3 flex flex-wrap items-baseline justify-between gap-3">
      <h2 class="card-title">Покрытие патча {{ coverage.patch }}</h2>
      <span class="hint">
        {{ fmt(coverage.matches) }} матчей (с timeline {{ fmt(coverage.withTimeline) }}) ·
        {{ fmt(coverage.observations) }} наблюдений · чемпионов {{ coverage.champions }}
      </span>
    </div>
    <div class="grid gap-3 sm:grid-cols-2 xl:grid-cols-5">
      <div v-for="c in cards" :key="c.key" class="rounded-lg border border-edge bg-soft p-3">
        <div class="font-semibold">{{ c.title }}</div>
        <div class="hint mb-2 min-h-8">{{ c.describe(c.family.threshold) }}</div>
        <div class="mb-1.5 flex items-baseline justify-between">
          <b class="text-xl">{{ pct(c.family.share) }}</b>
          <span class="hint">сочетаний {{ fmt(c.family.bucketsOk) }} из {{ fmt(c.family.buckets) }}</span>
        </div>
        <ProgressBar :value="c.family.share" :max="1" :tone="tone(c.family)" />
        <div class="mt-2.5 grid gap-1 text-xs">
          <div v-for="t in c.family.targets" :key="t.share" :class="target(t).done ? 'text-ok' : ''">
            {{ target(t).text }}
          </div>
        </div>
      </div>
    </div>
    <p class="hint mt-3">
      Процент — доля игр (чемпион в матче), попавших в сочетания с выборкой не меньше порога. «Нужно» —
      экстраполяция: сколько матчей патча понадобится всего, если распределение чемпионов и ролей не
      изменится; время — по текущему темпу сбора ({{ fmt(rate) }} матчей в час).
    </p>
  </section>
</template>
