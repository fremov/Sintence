<script setup lang="ts">
import { computed } from 'vue'
import type { LivePlayer, PlayerProfile } from '../types'

const props = defineProps<{
  player: LivePlayer
  minutes: number
  profile?: PlayerProfile
  championName: (championId: number) => string
}>()

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

const tierLabel: Record<string, string> = {
  IRON: 'Железо',
  BRONZE: 'Бронза',
  SILVER: 'Серебро',
  GOLD: 'Золото',
  PLATINUM: 'Платина',
  EMERALD: 'Изумруд',
  DIAMOND: 'Алмаз',
  MASTER: 'Мастер',
  GRANDMASTER: 'Грандмастер',
  CHALLENGER: 'Претендент',
}

// Ранг одной строкой: «Изумруд II · 42 LP».
// Мастер и выше дивизиона фактически не имеют — Riot всё равно
// присылает "I", и показывать его незачем.
const rank = computed(() => {
  const solo = props.profile?.solo
  if (!solo) return null

  const tier = tierLabel[solo.tier] ?? solo.tier
  const apex = solo.tier === 'MASTER' || solo.tier === 'GRANDMASTER' || solo.tier === 'CHALLENGER'
  return apex ? `${tier} · ${solo.leaguePoints} LP` : `${tier} ${solo.division} · ${solo.leaguePoints} LP`
})

// Винрейт считается здесь, а не в C++: это производное от wins и losses,
// и хранить его вторым числом значит однажды получить расхождение.
const soloRecord = computed(() => {
  const solo = props.profile?.solo
  if (!solo) return null

  const games = solo.wins + solo.losses
  if (games === 0) return null
  return `${solo.wins}\u2013${solo.losses} · ${Math.round((solo.wins / games) * 100)}%`
})

// «Не играл в соло» и «профиль ещё не приехал» — разные состояния,
// и подпись под ними тоже разная.
const rankNote = computed(() => (props.profile ? 'без соло-ранга' : 'профиль грузится'))

function shortPoints(points: number): string {
  if (points >= 1000000) return `${(points / 1000000).toFixed(1)}M`
  if (points >= 1000) return `${Math.round(points / 1000)}k`
  return String(points)
}

// Дата последней игры на чемпионе важнее самих очков: миллион очков
// двухлетней давности не говорит о том, что человек играет на нём сейчас.
function daysAgo(timestampMs: number): string {
  if (timestampMs <= 0) return ''
  const days = Math.floor((Date.now() - timestampMs) / 86400000)
  if (days <= 0) return 'сегодня'
  if (days === 1) return 'вчера'
  if (days < 30) return `${days} дн`
  if (days < 365) return `${Math.floor(days / 30)} мес`
  return `${Math.floor(days / 365)} г`
}

const masteries = computed(() =>
  (props.profile?.masteries ?? []).map((mastery) => ({
    id: mastery.championId,
    name: props.championName(mastery.championId),
    level: mastery.level,
    points: shortPoints(mastery.points),
    when: daysAgo(mastery.lastPlayTimeMs),
  })),
)

// Инвентарь: шесть ячеек плюс тринкет в седьмой. Riot присылает только
// занятые слоты, поэтому пустые достраиваются здесь — иначе предметы
// прыгали бы при каждой покупке, и взгляд не находил бы нужный.
const inventory = computed(() => {
  const slots: Array<{ key: number; name: string; count: number } | null> = Array(7).fill(null)
  for (const item of props.player.items) {
    if (item.slot >= 0 && item.slot < slots.length) {
      slots[item.slot] = { key: item.itemId, name: item.name, count: item.count }
    }
  }
  return slots
})

const itemsKnown = computed(() => props.player.items.length > 0)
</script>

<template>
  <article
    class="card"
    :class="[player.team === 'ORDER' ? 'order' : 'chaos', { dead: player.isDead }]"
  >
    <div class="row">
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
    </div>

    <ul v-if="itemsKnown" class="items">
      <li
        v-for="(item, index) in inventory"
        :key="index"
        :class="{ empty: !item, trinket: index === 6 }"
        :title="item ? item.name : ''"
      >
        <template v-if="item">
          <span class="item-name">{{ item.name }}</span>
          <b v-if="item.count > 1" class="item-count">{{ item.count }}</b>
        </template>
      </li>
    </ul>

    <div class="profile">
      <div class="rank">
        <template v-if="rank">
          <b>{{ rank }}</b>
          <span v-if="soloRecord" class="record">{{ soloRecord }}</span>
        </template>
        <span v-else class="pending">{{ rankNote }}</span>
      </div>

      <ul v-if="masteries.length" class="pool">
        <li v-for="mastery in masteries" :key="mastery.id">
          <span class="pool-name">{{ mastery.name }}</span>
          <span class="pool-meta">{{ mastery.level }} ур · {{ mastery.points }}</span>
          <span v-if="mastery.when" class="pool-when">{{ mastery.when }}</span>
        </li>
      </ul>
    </div>
  </article>
</template>

<style scoped>
.card {
  display: flex;
  flex-direction: column;
  gap: 7px;
  padding: 9px 14px 9px 12px;
  background: var(--panel-soft);
  border: 1px solid var(--edge);
  border-left-width: 3px;
  border-radius: 10px;
}

/* Инвентарь: семь ячеек фиксированной ширины. Названия длинные,
   поэтому обрезаются — полное имя в подсказке. */
.items {
  display: grid;
  grid-template-columns: repeat(7, 1fr);
  gap: 4px;
  margin: 0;
  padding: 0;
  list-style: none;
}

.items li {
  position: relative;
  min-width: 0;
  height: 20px;
  padding: 0 4px;
  display: flex;
  align-items: center;
  border-radius: 4px;
  background: #10151c;
  border: 1px solid var(--edge);
  font-size: 9px;
  color: var(--muted);
}

.items li.empty {
  background: #0d1117;
  border-style: dashed;
}

.items li.trinket {
  border-color: #3a4351;
}

.item-name {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.item-count {
  position: absolute;
  right: 2px;
  bottom: 1px;
  font-size: 9px;
  color: var(--gold);
}

.row {
  display: grid;
  grid-template-columns: 46px minmax(0, 1fr) 96px 74px;
  align-items: center;
  gap: 12px;
}

/* Вторая строка карточки: кто это вне матча. Ранг слева, пул справа —
   взгляд читает ранг у всех десяти одним движением сверху вниз. */
.profile {
  display: flex;
  align-items: center;
  gap: 10px;
  padding-top: 7px;
  border-top: 1px solid var(--edge);
  font-size: 10px;
  line-height: 1.2;
}

.rank {
  display: flex;
  align-items: baseline;
  gap: 6px;
  white-space: nowrap;
}

.rank b {
  color: var(--gold);
  font-weight: 600;
}

.record {
  color: var(--muted);
  font-variant-numeric: tabular-nums;
}

.pending {
  color: #4d5765;
}

.pool {
  display: flex;
  gap: 6px;
  margin: 0 0 0 auto;
  padding: 0;
  list-style: none;
  min-width: 0;
  overflow: hidden;
}

.pool li {
  display: flex;
  align-items: baseline;
  gap: 4px;
  padding: 2px 6px;
  border-radius: 5px;
  background: #10151c;
  border: 1px solid var(--edge);
  white-space: nowrap;
}

.pool-name {
  color: var(--text);
}

.pool-meta {
  color: var(--muted);
  font-variant-numeric: tabular-nums;
}

.pool-when {
  color: #4d5765;
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
