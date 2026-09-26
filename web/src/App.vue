<script setup lang="ts">
import { computed } from 'vue'
import ActivePlayerBar from './components/ActivePlayerBar.vue'
import AdvicePanel from './components/AdvicePanel.vue'
import PlayerCard from './components/PlayerCard.vue'
import { useChampionNames } from './useChampionNames'
import { useLiveGame } from './useLiveGame'
import { useProfiles } from './useProfiles'
import { usePreferences } from './usePreferences'

const { game, state } = useLiveGame(1000)
const { profiles, progress, enabled: profilesEnabled } = useProfiles(3000)
const { matchups, you, enabled: packEnabled } = usePreferences(5000)
const { nameOf } = useChampionNames()

const clock = computed(() => {
  const total = Math.trunc(game.value?.stats.gameTimeSeconds ?? 0)
  const minutes = Math.trunc(total / 60)
  const seconds = total % 60
  return `${minutes}:${String(seconds).padStart(2, '0')}`
})

const minutes = computed(() => (game.value?.stats.gameTimeSeconds ?? 0) / 60)

const order = computed(() => game.value?.players.filter((p) => p.team === 'ORDER') ?? [])
const chaos = computed(() => game.value?.players.filter((p) => p.team === 'CHAOS') ?? [])

const orderKills = computed(() => order.value.reduce((sum, p) => sum + p.kills, 0))
const chaosKills = computed(() => chaos.value.reduce((sum, p) => sum + p.kills, 0))

const mapLabel: Record<string, string> = {
  Map11: 'Ущелье призывателей',
  Map12: 'Бездна воющих',
  Map21: 'Арена',
}

const subtitle = computed(() => {
  const stats = game.value?.stats
  if (!stats) return ''
  return `${stats.gameMode} · ${mapLabel[stats.mapName] ?? stats.mapName}`
})

const feedLabel: Record<string, string> = {
  live: 'живой матч',
  idle: 'матч не идёт',
  loading: 'подключаюсь',
  error: 'нет связи',
}

const emptyHeadline = computed(() => {
  if (state.value === 'idle') return 'Матч не идёт'
  if (state.value === 'loading') return 'Подключаюсь к игре'
  return 'Sintence не отвечает'
})

const emptyHint = computed(() => {
  if (state.value === 'idle') {
    return 'Табло появится, когда начнётся игра. Practice Tool тоже считается.'
  }
  if (state.value === 'loading') {
    return 'Спрашиваю игровой клиент на 127.0.0.1:2999'
  }
  return 'Окно живёт, а сервер внутри sintence.exe — нет.'
})

// Подпись о докачке профилей. Тридцать запросов при лимите ключа
// растягиваются секунд на сорок, и молчащий интерфейс в это время
// выглядит сломанным.
const profilesLabel = computed(() => {
  if (!profilesEnabled.value) return 'профили выключены: нет ключа Riot'
  const { done, total, running } = progress.value
  if (total === 0) return 'профили: жду лобби'
  if (running || done < total) return `профили: ${done} из ${total}`
  return `профили: ${done}`
})

// Пак статичен в пределах патча, поэтому подпись короткая: он или есть,
// или его надо собрать. Молчание тут хуже — выглядит как поломка.
const packLabel = computed(() =>
  packEnabled.value ? 'мета загружена' : 'меты нет: собери пак',
)
</script>

<template>
  <div class="panel">
    <header>
      <template v-if="state === 'live'">
        <span class="clock">{{ clock }}</span>
        <span class="mode">{{ subtitle }}</span>
      </template>
      <span v-else class="brand">Sintence</span>

      <span class="feed" :class="state">{{ feedLabel[state] }}</span>
    </header>

    <ActivePlayerBar v-if="state === 'live' && game?.activePlayer" :active="game.activePlayer" />

    <AdvicePanel v-if="state === 'live' && you" :you="you" :matchups="matchups" />

    <main v-if="state === 'live' && game" class="board">
      <section class="team order">
        <div class="team-head">
          <span class="team-name">Синие</span>
          <span class="score">{{ orderKills }}</span>
        </div>
        <PlayerCard
          v-for="p in order"
          :key="p.riotId + p.championName"
          :player="p"
          :minutes="minutes"
          :profile="profiles[p.riotId]"
          :champion-name="nameOf"
        />
      </section>

      <section class="team chaos">
        <div class="team-head">
          <span class="team-name">Красные</span>
          <span class="score">{{ chaosKills }}</span>
        </div>
        <PlayerCard
          v-for="p in chaos"
          :key="p.riotId + p.championName"
          :player="p"
          :minutes="minutes"
          :profile="profiles[p.riotId]"
          :champion-name="nameOf"
        />
      </section>
    </main>

    <main v-else class="empty">
      <p class="headline">{{ emptyHeadline }}</p>
      <p class="hint">{{ emptyHint }}</p>
    </main>

    <footer>
      <span>табло — раз в секунду</span>
      <span class="dot">·</span>
      <span>{{ profilesLabel }}</span>
      <span class="dot">·</span>
      <span>{{ packLabel }}</span>
      <span class="spacer"></span>
      <span><kbd>PgDn</kbd> — скрыть</span>
    </footer>
  </div>
</template>

<style scoped>
.panel {
  height: 100%;
  display: flex;
  flex-direction: column;
  background: var(--panel);
  border: 1px solid var(--edge);
}

header {
  display: flex;
  align-items: baseline;
  gap: 10px;
  padding: 12px 16px;
  border-bottom: 1px solid var(--edge);
}

.clock {
  font-size: 20px;
  font-weight: 600;
  font-variant-numeric: tabular-nums;
}

.brand {
  font-size: 14px;
  font-weight: 600;
  color: var(--gold);
  letter-spacing: 0.4px;
}

.mode {
  color: var(--muted);
  font-size: 11px;
  text-transform: uppercase;
  letter-spacing: 0.8px;
}

.feed {
  margin-left: auto;
  display: flex;
  align-items: center;
  gap: 7px;
  color: var(--muted);
  font-size: 11px;
}

.feed::before {
  content: '';
  width: 6px;
  height: 6px;
  border-radius: 50%;
  background: var(--muted);
}

.feed.live::before {
  background: var(--ok);
  animation: pulse 2s ease-in-out infinite;
}

.feed.error::before {
  background: var(--chaos);
}

@keyframes pulse {
  0%,
  100% {
    opacity: 1;
  }
  50% {
    opacity: 0.3;
  }
}

/* Две колонки по командам: команда противника читается одним движением
   глаз, а не поиском по общему списку. Прокрутки нет — окно подобрано
   под пять карточек в колонке, больше в матче не бывает. */
.board {
  flex: 1;
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 14px;
  padding: 14px 16px;
  overflow: hidden;
}

.team {
  display: flex;
  flex-direction: column;
  gap: 8px;
  min-width: 0;
}

.team-head {
  display: flex;
  align-items: baseline;
  gap: 8px;
  padding: 0 2px 2px;
}

.team-name {
  font-size: 11px;
  letter-spacing: 1px;
  text-transform: uppercase;
  color: var(--muted);
}

.score {
  margin-left: auto;
  font-size: 16px;
  font-weight: 600;
  font-variant-numeric: tabular-nums;
}

.team.order .score {
  color: var(--order);
}

.team.chaos .score {
  color: var(--chaos);
}

.empty {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 8px;
  text-align: center;
  padding: 32px;
}

.headline {
  margin: 0;
  font-size: 17px;
  font-weight: 600;
}

.hint {
  margin: 0;
  max-width: 340px;
  color: var(--muted);
  font-size: 12px;
  line-height: 1.5;
}

footer {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 9px 16px;
  border-top: 1px solid var(--edge);
  color: var(--muted);
  font-size: 11px;
}

.dot {
  color: #3a4351;
}

.spacer {
  flex: 1;
}

kbd {
  font: inherit;
  background: #1e2631;
  border: 1px solid #303a49;
  border-bottom-width: 2px;
  border-radius: 4px;
  padding: 1px 5px;
}
</style>
