<script setup lang="ts">
import { computed } from 'vue'
import type { LiveActivePlayer } from '../types'

const props = defineProps<{ active: LiveActivePlayer }>()

// Пассивка в отдельной ячейке: у неё нет уровня, и ставить её в один
// ряд с Q/W/E/R значило бы показывать «0» там, где числа не существует.
const spells = computed(() => props.active.abilities.filter((a) => a.slot !== 'Passive'))
const passive = computed(() => props.active.abilities.find((a) => a.slot === 'Passive'))

const gold = computed(() => Math.round(props.active.currentGold))
</script>

<template>
  <section class="bar">
    <div class="who">
      <span class="label">ты</span>
      <span class="gold">{{ gold }} з</span>
    </div>

    <ul class="spells">
      <li v-for="spell in spells" :key="spell.slot" :title="spell.name">
        <b>{{ spell.slot }}</b>
        <span class="lvl">{{ spell.level }}</span>
      </li>
      <li v-if="passive" class="passive" :title="passive.name">
        <b>P</b>
      </li>
    </ul>

    <div class="runes">
      <span class="keystone">{{ active.runes.keystone || '—' }}</span>
      <span class="trees">{{ active.runes.primaryTree }} / {{ active.runes.secondaryTree }}</span>
      <span v-if="active.runes.minorRunes.length" class="minor">
        {{ active.runes.minorRunes.join(' · ') }}
      </span>
    </div>
  </section>
</template>

<style scoped>
.bar {
  display: flex;
  align-items: center;
  gap: 14px;
  padding: 8px 16px;
  border-bottom: 1px solid var(--edge);
  background: #10141b;
  font-size: 11px;
}

.who {
  display: flex;
  align-items: baseline;
  gap: 7px;
}

.label {
  font-size: 10px;
  letter-spacing: 1px;
  text-transform: uppercase;
  color: var(--muted);
}

.gold {
  color: var(--gold);
  font-weight: 600;
  font-variant-numeric: tabular-nums;
}

.spells {
  display: flex;
  gap: 5px;
  margin: 0;
  padding: 0;
  list-style: none;
}

.spells li {
  display: flex;
  align-items: baseline;
  gap: 4px;
  padding: 2px 7px;
  border-radius: 5px;
  background: var(--panel-soft);
  border: 1px solid var(--edge);
}

.spells b {
  font-size: 10px;
  color: var(--muted);
}

.lvl {
  font-weight: 600;
  font-variant-numeric: tabular-nums;
}

.spells .passive {
  color: var(--muted);
}

.runes {
  display: flex;
  align-items: baseline;
  gap: 8px;
  margin-left: auto;
  min-width: 0;
}

.keystone {
  color: var(--gold);
  font-weight: 600;
  white-space: nowrap;
}

.trees {
  color: var(--muted);
  white-space: nowrap;
}

.minor {
  color: #4d5765;
  font-size: 10px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
</style>
