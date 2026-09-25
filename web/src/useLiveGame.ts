import { ref, onMounted, onUnmounted } from 'vue'
import type { FeedState, LiveGame } from './types'

// Опрос sintence.exe раз в секунду.
//
// Почему опрос, а не SSE или WebSocket: данные меняются медленно (KDA и CS —
// единицы раз в минуту), запрос локальный, а опрос не требует ни держать
// соединение, ни переподключаться после конца матча. Усложнять будем тогда,
// когда появится причина, а не заранее.
export function useLiveGame(intervalMs = 1000) {
  const game = ref<LiveGame | null>(null)
  const state = ref<FeedState>('loading')

  let timer: number | undefined

  async function poll() {
    try {
      const response = await fetch('/api/live')
      if (response.status === 503) {
        // Сервер жив, игры нет. Прошлый снимок убираем: показывать
        // застывшее табло закончившегося матча — значит врать.
        game.value = null
        state.value = 'idle'
        return
      }
      if (!response.ok) {
        state.value = 'error'
        return
      }
      game.value = (await response.json()) as LiveGame
      state.value = 'live'
    } catch {
      state.value = 'error'
    }
  }

  onMounted(() => {
    void poll()
    timer = window.setInterval(() => void poll(), intervalMs)
  })

  onUnmounted(() => window.clearInterval(timer))

  return { game, state }
}
