<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import type { Matchup, PreferencesResponse } from '../types'

const props = defineProps<{
  you: PreferencesResponse['you']
  matchups: Matchup[]
}>()

// Выбранный противник. По умолчанию — тот, с кем стоишь на линии:
// против него сборка и порядок прокачки решают больше всего.
const selected = ref(0)

watch(
  () => props.matchups.map((matchup) => matchup.versus).join(),
  () => {
    selected.value = 0
  },
)

const current = computed<Matchup | null>(() => props.matchups[selected.value] ?? null)

const roleLabel: Record<string, string> = {
  TOP: 'верх',
  JUNGLE: 'лес',
  MIDDLE: 'центр',
  BOTTOM: 'низ',
  UTILITY: 'поддержка',
}

const tierLabel: Record<string, string> = {
  BRONZE: 'бронза-железо',
  GOLD: 'золото-серебро',
  EMERALD: 'изумруд-платина',
  DIAMOND: 'алмаз+',
  ALL: 'все ранги',
}

// Лучший вариант — не самый частый, а самый проверенный: сортируем
// по нижней границе Вильсона, иначе вариант с пятью играми из пяти
// вылезет вперёд проверенного с пятью сотнями.
function best(variants: Matchup['runePages']) {
  if (!variants.length) return null
  return [...variants].sort((a, b) => b.winrateLow - a.winrateLow)[0]
}

const runes = computed(() => best(current.value?.runePages ?? [])?.page ?? null)
const runeStats = computed(() => best(current.value?.runePages ?? []) ?? null)
const skills = computed(() => best(current.value?.skillOrders ?? []) ?? null)
const items = computed(() => best(current.value?.itemChains ?? []) ?? null)

const percent = (value: number) => `${Math.round(value * 100)}%`
</script>

<template>
  <section class="advice">
    <header class="head">
      <span class="label">совет</span>
      <b class="me">{{ you.championName || you.champion }}</b>
      <span class="role">{{ roleLabel[you.role] ?? you.role }}</span>

      <nav class="enemies">
        <button
          v-for="(matchup, index) in matchups"
          :key="matchup.versus"
          :class="{ active: index === selected, lane: matchup.lane }"
          @click="selected = index"
        >
          {{ matchup.versusName || matchup.versus }}
          <i v-if="matchup.lane">линия</i>
        </button>
      </nav>
    </header>

    <div v-if="current" class="body">
      <div class="source">
        <template v-if="current.exact">
          против <b>{{ current.versusName || current.versus }}</b>
        </template>
        <template v-else>
          <!-- Выборки по матчапу не набралось. Молчать об этом нельзя:
               «против всех» прочиталось бы как «против него». -->
          против всех <span class="warn">(по матчапу данных мало)</span>
        </template>
        <span class="dim">
          · {{ current.games }} игр · {{ percent(current.winrate) }} побед ·
          {{ tierLabel[current.tier] ?? current.tier }} · патч {{ current.patch }}
        </span>
      </div>

      <div class="grid">
        <div class="block">
          <div class="block-head">
            руны
            <span v-if="runeStats" class="dim">
              {{ percent(runeStats.share) }} · {{ runeStats.games }} игр
            </span>
          </div>
          <div v-if="runes" class="runes">
            <div class="tree">
              <span class="tree-name">{{ runes.primaryTree }}</span>
              <b class="keystone">{{ runes.keystone }}</b>
              <span v-for="rune in runes.primary" :key="rune" class="rune">{{ rune }}</span>
            </div>
            <div class="tree">
              <span class="tree-name">{{ runes.secondaryTree }}</span>
              <span v-for="rune in runes.secondary" :key="rune" class="rune">{{ rune }}</span>
            </div>
            <div class="tree shards">
              <span class="tree-name">осколки</span>
              <span v-for="(shard, index) in runes.shards" :key="index" class="rune">
                {{ shard }}
              </span>
            </div>
          </div>
          <p v-else class="none">данных не хватило</p>
        </div>

        <div class="block">
          <div class="block-head">
            прокачка
            <span v-if="skills" class="dim">
              {{ percent(skills.share) }} · {{ skills.games }} игр
            </span>
          </div>
          <ol v-if="skills?.steps?.length" class="chain">
            <li v-for="(step, index) in skills.steps" :key="index" class="skill">{{ step }}</li>
          </ol>
          <p v-else class="none">данных не хватило</p>
        </div>

        <div class="block">
          <div class="block-head">
            предметы по порядку
            <span v-if="items" class="dim">
              {{ percent(items.share) }} · {{ items.games }} игр
            </span>
          </div>
          <ol v-if="items?.steps?.length" class="chain items">
            <li v-for="(step, index) in items.steps" :key="index">{{ step }}</li>
          </ol>
          <p v-else class="none">данных не хватило</p>
        </div>
      </div>
    </div>

    <div v-else class="body">
      <p class="none">по этому чемпиону в базе пока нет данных</p>
    </div>
  </section>
</template>

<style scoped>
.advice {
  border-bottom: 1px solid var(--edge);
  background: #10141b;
  font-size: 11px;
}

.head {
  display: flex;
  align-items: baseline;
  gap: 8px;
  padding: 8px 16px 6px;
}

.label {
  font-size: 10px;
  letter-spacing: 1px;
  text-transform: uppercase;
  color: var(--muted);
}

.me {
  color: var(--gold);
  font-size: 13px;
}

.role {
  color: var(--muted);
}

.enemies {
  display: flex;
  gap: 5px;
  margin-left: auto;
  flex-wrap: wrap;
}

.enemies button {
  display: flex;
  align-items: baseline;
  gap: 5px;
  padding: 3px 8px;
  border-radius: 5px;
  border: 1px solid var(--edge);
  background: var(--panel-soft);
  color: var(--muted);
  font: inherit;
  cursor: pointer;
}

.enemies button.lane {
  border-color: #3a4351;
  color: var(--text);
}

.enemies button.active {
  border-color: var(--gold);
  color: var(--text);
}

.enemies i {
  font-style: normal;
  font-size: 9px;
  letter-spacing: 0.5px;
  text-transform: uppercase;
  color: var(--gold);
}

.body {
  padding: 0 16px 10px;
}

.source {
  padding-bottom: 7px;
}

.source b {
  color: var(--chaos);
}

.warn {
  color: var(--danger);
}

.dim {
  color: #4d5765;
}

.grid {
  display: grid;
  grid-template-columns: minmax(0, 1.4fr) minmax(0, 0.7fr) minmax(0, 1.2fr);
  gap: 12px;
}

.block {
  min-width: 0;
}

.block-head {
  display: flex;
  align-items: baseline;
  gap: 6px;
  margin-bottom: 4px;
  font-size: 10px;
  letter-spacing: 0.6px;
  text-transform: uppercase;
  color: var(--muted);
}

.runes {
  display: flex;
  flex-direction: column;
  gap: 3px;
}

.tree {
  display: flex;
  align-items: baseline;
  gap: 5px;
  flex-wrap: wrap;
}

.tree-name {
  min-width: 82px;
  color: #4d5765;
}

.keystone {
  color: var(--gold);
}

.rune {
  padding: 1px 5px;
  border-radius: 4px;
  background: var(--panel-soft);
  border: 1px solid var(--edge);
}

.chain {
  display: flex;
  align-items: center;
  gap: 4px;
  margin: 0;
  padding: 0;
  list-style: none;
  flex-wrap: wrap;
}

.chain li {
  padding: 2px 7px;
  border-radius: 4px;
  background: var(--panel-soft);
  border: 1px solid var(--edge);
}

/* Стрелка между звеньями: цепочка читается как последовательность,
   а не как набор. Первый элемент без стрелки. */
.chain li + li::before {
  content: '→ ';
  color: #4d5765;
}

.chain .skill {
  font-weight: 600;
  min-width: 22px;
  text-align: center;
}

.none {
  margin: 0;
  color: #4d5765;
}
</style>
