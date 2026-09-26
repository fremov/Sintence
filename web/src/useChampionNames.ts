import { ref, onMounted } from 'vue'

// Справочник «числовой id чемпиона -> имя» из Data Dragon.
//
// champion-mastery-v4 отдаёт только championId, имён у Riot в этом ответе
// нет вовсе. Data Dragon — статика на CDN: без ключа, без лимитов и без
// какого-либо отношения к нашему C++. Поэтому её тянет фронт, а не бэкенд:
// иначе в data/ появился бы ещё один справочник, который надо обновлять
// на каждом патче.
//
// Ответ кладётся в localStorage вместе с версией патча: файл весит около
// сотни килобайт, и качать его на каждый запуск окна незачем.

const STORAGE_KEY = 'sintence.championNames'

interface Cached {
  version: string
  names: Record<string, string>
}

async function fetchVersion(): Promise<string> {
  const response = await fetch('https://ddragon.leagueoflegends.com/api/versions.json')
  const versions = (await response.json()) as string[]
  return versions[0]
}

async function fetchNames(version: string): Promise<Record<string, string>> {
  const url = `https://ddragon.leagueoflegends.com/cdn/${version}/data/ru_RU/champion.json`
  const response = await fetch(url)
  const data = (await response.json()) as {
    data: Record<string, { key: string; name: string }>
  }

  const names: Record<string, string> = {}
  for (const champion of Object.values(data.data)) {
    // key — это тот самый числовой id, который приходит в мастери,
    // но в Data Dragon он лежит строкой.
    names[champion.key] = champion.name
  }
  return names
}

export function useChampionNames() {
  const names = ref<Record<string, string>>({})

  function nameOf(championId: number): string {
    return names.value[String(championId)] ?? `#${championId}`
  }

  onMounted(async () => {
    const cached = localStorage.getItem(STORAGE_KEY)
    if (cached) {
      try {
        const parsed = JSON.parse(cached) as Cached
        names.value = parsed.names
      } catch {
        localStorage.removeItem(STORAGE_KEY)
      }
    }

    try {
      const version = await fetchVersion()
      const stored = cached ? (JSON.parse(cached) as Cached) : null
      if (stored && stored.version === version) {
        return
      }
      const fresh = await fetchNames(version)
      names.value = fresh
      localStorage.setItem(STORAGE_KEY, JSON.stringify({ version, names: fresh }))
    } catch {
      // Интернета нет — покажем «#157» вместо «Ясуо». Матч это не ломает.
    }
  })

  return { nameOf }
}
