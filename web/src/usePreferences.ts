import { ref, onMounted, onUnmounted } from 'vue'
import type { Matchup, PreferencesResponse } from './types'

// Опрос GET /api/preferences — советы для активного игрока.
//
// Данные статичны в пределах матча: чемпион, роль и состав вражеской
// команды не меняются, пак пересобирается раз в патч. Опрос нужен только
// чтобы подхватить начало матча и уточнить ранговую корзину, когда
// догрузится профиль.
//
// 501 — пака нет, функция выключена, опрос прекращается.
// 503 — матча нет или клиент в режиме наблюдателя: активного игрока,
// для которого считать советы, не существует.
export function usePreferences(intervalMs = 5000) {
  const matchups = ref<Matchup[]>([])
  const you = ref<PreferencesResponse['you'] | null>(null)
  const patch = ref('')
  const enabled = ref(true)

  let timer: number | undefined

  function stop() {
    window.clearInterval(timer)
    timer = undefined
  }

  async function poll() {
    try {
      const response = await fetch('/api/preferences')
      if (response.status === 501) {
        enabled.value = false
        stop()
        return
      }
      if (!response.ok) {
        matchups.value = []
        you.value = null
        return
      }
      const data = (await response.json()) as PreferencesResponse
      matchups.value = data.matchups
      you.value = data.you
      patch.value = data.patch
    } catch {
      // Сервер не отвечает — об этом уже кричит табло.
    }
  }

  onMounted(() => {
    void poll()
    timer = window.setInterval(() => void poll(), intervalMs)
  })

  onUnmounted(stop)

  return { matchups, you, patch, enabled }
}
