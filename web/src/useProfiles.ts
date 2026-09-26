import { ref, onMounted, onUnmounted } from 'vue'
import type { PlayerProfile, ProfilesProgress, ProfilesResponse } from './types'

// Опрос GET /api/profiles.
//
// Реже, чем табло: профиль за время матча не меняется, а докачка идёт
// в фоне на стороне C++ и упирается в лимит ключа Riot (100 запросов
// за 2 минуты). Три секунды — достаточно частo, чтобы карточки
// «проявлялись» на глазах, и достаточно редко, чтобы не мешать.
//
// Ответ 501 означает «ключа Riot нет»: это не ошибка, а выключенная
// функция, и опрос в таком случае прекращается совсем.
export function useProfiles(intervalMs = 3000) {
  const profiles = ref<Record<string, PlayerProfile>>({})
  const progress = ref<ProfilesProgress>({ done: 0, total: 0, running: false })
  const enabled = ref(true)

  let timer: number | undefined

  function stop() {
    window.clearInterval(timer)
    timer = undefined
  }

  async function poll() {
    try {
      const response = await fetch('/api/profiles')
      if (response.status === 501) {
        enabled.value = false
        stop()
        return
      }
      if (!response.ok) {
        // 503 — матч ещё не начался. Прошлые профили не выбрасываем:
        // те же игроки часто остаются в следующей игре подряд.
        return
      }
      const data = (await response.json()) as ProfilesResponse
      const next: Record<string, PlayerProfile> = {}
      for (const profile of data.profiles) {
        next[profile.riotId] = profile
      }
      profiles.value = next
      progress.value = data.progress
    } catch {
      // Сервер внутри sintence.exe умер — об этом уже кричит табло.
    }
  }

  onMounted(() => {
    void poll()
    timer = window.setInterval(() => void poll(), intervalMs)
  })

  onUnmounted(stop)

  return { profiles, progress, enabled }
}
