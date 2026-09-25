<script setup lang="ts">
import { computed } from 'vue'
import type { LivePlayer } from '../types'

const props = defineProps<{ player: LivePlayer; minutes: number }>()

// CS в минуту — единственная производная величина, которую считает фронт.
// Остальное приходит готовым из C++: считать метрики в двух местах значит
// однажды получить два разных числа.
const csPerMinute = computed(() => {
  if (props.minutes <= 0) return '0.0'
  return (props.player.creepScore / props.minutes).toFixed(1)
})

// KDA числом: (убийства + помощь) / смерти. Ноль смертей делим на единицу —
// то же соглашение, что в ChampionReport::Kda().
const kdaRatio = computed(() => {
  const deaths = props.player.deaths === 0 ? 1 : props.player.deaths
  return ((props.player.kills + props.player.assists) / deaths).toFixed(2)
})

const positionLabel: Record<string, string> = {
  TOP: 'верх',
  JUNGLE: 'лес',
  MIDDLE: 'центр',
  BOTTOM: 'низ',
  UTILITY: 'поддержка',
}

const position = computed(() => positionLabel[props.player.position] ?? '')

// Инициалы вместо иконки: имена чемпионов приходят на языке клиента
// («Владимир»), а Data Dragon ждёт каноническое id. Пока поле rawChampionName
// не читается, честнее показать буквы, чем битую картинку.
const initials = computed(() => props.player.championName.slice(0, 2))
</script>

<template>
  <article
    class="card"
    :class="[player.team === 'ORDER' ? 'order' : 'chaos', { dead: player.isDead }]"
  >
    <div class="portrait">
      <span>{{ initials }}</span>
      <b class="level">{{ player.level }}</b>
    </div>

    <div class="identity">
      <div class="champion">
        {{ player.championName }}
        <em v-if="position">{{ position }}</em>
      </div>
      <div class="player">{{ player.riotId || '—' }}</div>
      <div v-if="player.isDead" class="status">убит</div>
    </div>

    <div class="stats">
      <div class="kda">
        <span>{{ player.kills }}</span>
        <i>/</i>
        <span class="deaths">{{ player.deaths }}</span>
        <i>/</i>
        <span>{{ player.assists }}</span>
      </div>
      <div class="ratio">{{ kdaRatio }} KDA</div>
    </div>

    <div class="farm">
      <div class="cs">{{ player.creepScore }}</div>
      <div class="rate">{{ csPerMinute }} /мин</div>
    </div>
  </article>
</template>

<style scoped>
.card {
  display: grid;
  grid-template-columns: 46px minmax(0, 1fr) 96px 74px;
  align-items: center;
  gap: 12px;
  padding: 10px 14px 10px 12px;
  background: var(--panel-soft);
  border: 1px solid var(--edge);
  border-left-width: 3px;
  border-radius: 10px;
}

.card.order {
  border-left-color: var(--order);
}

.card.chaos {
  border-left-color: var(--chaos);
}

.card.dead {
  background: #161b23;
}

.card.dead .portrait span,
.card.dead .champion {
  color: var(--muted);
}

.portrait {
  position: relative;
  width: 46px;
  height: 46px;
  border-radius: 8px;
  background: #222a35;
  display: grid;
  place-items: center;
  font-size: 15px;
  font-weight: 600;
  color: #9aa6b6;
}

.card.dead .portrait {
  filter: grayscale(1) brightness(0.6);
}

.level {
  position: absolute;
  right: -5px;
  bottom: -5px;
  min-width: 18px;
  height: 18px;
  padding: 0 3px;
  border-radius: 9px;
  background: #0d1117;
  border: 1px solid var(--edge);
  font-size: 11px;
  font-weight: 600;
  font-variant-numeric: tabular-nums;
  display: grid;
  place-items: center;
  color: var(--text);
}

.identity {
  min-width: 0;
}

.champion {
  font-size: 14px;
  font-weight: 600;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.champion em {
  margin-left: 6px;
  font-style: normal;
  font-size: 10px;
  letter-spacing: 0.6px;
  text-transform: uppercase;
  color: var(--muted);
}

.player {
  margin-top: 2px;
  font-size: 11px;
  color: var(--muted);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.status {
  margin-top: 3px;
  font-size: 10px;
  letter-spacing: 0.6px;
  text-transform: uppercase;
  color: var(--danger);
}

.stats {
  text-align: right;
}

.kda {
  font-size: 15px;
  font-variant-numeric: tabular-nums;
  letter-spacing: 0.3px;
}

.kda i {
  font-style: normal;
  color: #47526180;
  margin: 0 1px;
}

.kda .deaths {
  color: var(--danger);
}

.ratio {
  margin-top: 2px;
  font-size: 11px;
  color: var(--muted);
  font-variant-numeric: tabular-nums;
}

.farm {
  text-align: right;
}

.cs {
  font-size: 15px;
  font-weight: 600;
  font-variant-numeric: tabular-nums;
}

.rate {
  margin-top: 2px;
  font-size: 11px;
  color: var(--muted);
  font-variant-numeric: tabular-nums;
}
</style>
