<script setup lang="ts">
import { computed, ref } from 'vue'
import type { Coverage } from '../api'
import { championIcon, championName, champions } from '../ddragon'
import { ROLE, fmt, pct } from '../format'
import ProgressBar from './ProgressBar.vue'

// Основные роли чемпионов, которым не хватает игр, — что именно добирает сбор.

const props = defineProps<{ coverage: Coverage }>()

const filter = ref<'pack' | 'reliable' | 'all'>('pack')
const query = ref('')

const rows = computed(() => {
  const { pack, reliable } = props.coverage.thresholds
  const text = query.value.trim().toLowerCase()
  return props.coverage.mains
    .filter((m) => {
      if (filter.value === 'pack' && m.games >= pack) return false
      if (filter.value === 'reliable' && m.games >= reliable) return false
      return !text || championName(m.champion).toLowerCase().includes(text) || m.champion.toLowerCase().includes(text)
    })
    .slice(0, 300)
    .map((m) => {
      const target = m.games < pack ? pack : reliable
      return {
        ...m,
        status: m.games < pack ? 'bad' : m.games < reliable ? 'warn' : 'ok',
        need: m.games < target ? Math.ceil((props.coverage.matches * target) / Math.max(1, m.games)) : null,
      }
    })
})

// Чемпионы без единой игры на патче: их нет среди основных ролей вовсе.
const unseen = computed(() => {
  const seen = new Set(props.coverage.mains.map((m) => m.champion.toLowerCase()))
  return champions.list.filter((c) => !seen.has(c.id.toLowerCase()))
})

const FILTERS = [
  { id: 'pack', label: 'не в паке' },
  { id: 'reliable', label: 'меньше надёжного' },
  { id: 'all', label: 'все основные роли' },
] as const
</script>

<template>
  <section class="card">
    <div class="mb-2 flex flex-wrap items-baseline justify-between gap-3">
      <h2 class="card-title">Чего не хватает</h2>
      <div class="flex flex-wrap items-center gap-1.5">
        <button
          v-for="f in FILTERS"
          :key="f.id"
          class="btn px-2.5 py-1 text-xs"
          :class="{ 'border-gold text-gold': filter === f.id }"
          @click="filter = f.id"
        >
          {{ f.label }}
        </button>
        <input v-model="query" class="field w-36 py-1 text-xs" placeholder="чемпион" />
      </div>
    </div>
    <p class="hint mb-3">
      Основные роли чемпионов (от 15% его игр — Джинкс в лесу не в счёт) и сколько игр у них на патче.
      Шкала — до надёжных {{ coverage.thresholds.reliable }} игр, риска — порог пака {{ coverage.thresholds.pack }}.
      «Нужно матчей» — сколько всего матчей патча понадобится, чтобы сочетание набрало следующий порог при
      нынешней доле этого чемпиона.
    </p>

    <div v-if="unseen.length" class="mb-3 rounded-md border border-dashed border-edge px-3 py-2 text-xs text-muted">
      Ни одной игры на патче: {{ unseen.map((c) => c.name).join(', ') }}
    </div>

    <table v-if="rows.length" class="w-full">
      <thead>
        <tr class="text-left text-[11px] text-muted">
          <th class="border-b border-edge px-2 py-1.5 font-medium">Чемпион</th>
          <th class="border-b border-edge px-2 py-1.5 font-medium">Роль</th>
          <th class="border-b border-edge px-2 py-1.5 text-right font-medium">Игр</th>
          <th class="w-[26%] border-b border-edge px-2 py-1.5 font-medium">до надёжных {{ coverage.thresholds.reliable }}</th>
          <th class="border-b border-edge px-2 py-1.5 text-right font-medium" title="доля игр чемпиона на этой роли">Доля роли</th>
          <th class="border-b border-edge px-2 py-1.5 text-right font-medium" :title="`матчапов с ${coverage.thresholds.pack}+ играми`">
            Матчапов
          </th>
          <th class="border-b border-edge px-2 py-1.5 font-medium">Статус</th>
          <th class="border-b border-edge px-2 py-1.5 text-right font-medium">Нужно матчей</th>
        </tr>
      </thead>
      <tbody>
        <tr v-for="m in rows" :key="m.champion + m.role" class="hover:bg-soft">
          <td class="border-b border-[#1a2029] px-2 py-1">
            <span class="flex items-center gap-2">
              <img v-if="championIcon(m.champion)" :src="championIcon(m.champion)" class="size-6 rounded border border-edge" alt="" loading="lazy" />
              {{ championName(m.champion) }}
            </span>
          </td>
          <td class="border-b border-[#1a2029] px-2 py-1">{{ ROLE[m.role] ?? m.role }}</td>
          <td class="border-b border-[#1a2029] px-2 py-1 text-right">{{ fmt(m.games) }}</td>
          <td class="border-b border-[#1a2029] px-2 py-1">
            <ProgressBar
              :value="m.games"
              :max="coverage.thresholds.reliable"
              :tone="m.status === 'ok' ? 'ok' : m.status === 'warn' ? 'warn' : 'bad'"
              :mark="coverage.thresholds.pack"
            />
          </td>
          <td class="border-b border-[#1a2029] px-2 py-1 text-right">{{ pct(m.share) }}</td>
          <td class="border-b border-[#1a2029] px-2 py-1 text-right">{{ m.matchups }}</td>
          <td class="border-b border-[#1a2029] px-2 py-1 text-xs whitespace-nowrap">
            <span v-if="m.status === 'bad'" class="text-red">не в паке — ещё {{ m.toPack }}</span>
            <span v-else-if="m.status === 'warn'" class="text-warn">в паке, до надёжного {{ m.toReliable }}</span>
            <span v-else class="text-ok">надёжно</span>
          </td>
          <td class="border-b border-[#1a2029] px-2 py-1 text-right">{{ m.need ? `≈ ${fmt(m.need)}` : '—' }}</td>
        </tr>
      </tbody>
    </table>
    <p v-else class="hint">{{ filter === 'all' ? 'Нет данных.' : 'Таких нет — всё набрано.' }}</p>
  </section>
</template>
