import { reactive } from 'vue'

// Имена и иконки чемпионов на русском — Data Dragon, как в самом Sintence.
// Без интернета страница показывает ключи ("MonkeyKing") без иконок.

const DDRAGON = 'https://ddragon.leagueoflegends.com'

export const champions = reactive({
  version: '',
  list: [] as Array<{ id: string; name: string }>,
  byKey: {} as Record<string, { id: string; name: string }>,
})

export async function loadChampions(): Promise<void> {
  try {
    const versions = (await (await fetch(`${DDRAGON}/api/versions.json`)).json()) as string[]
    const version = versions[0]
    const data = (await (await fetch(`${DDRAGON}/cdn/${version}/data/ru_RU/champion.json`)).json()) as {
      data: Record<string, { id: string; name: string }>
    }
    champions.list = Object.values(data.data).map((c) => ({ id: c.id, name: c.name }))
    champions.byKey = Object.fromEntries(champions.list.map((c) => [c.id.toLowerCase(), c]))
    champions.version = version
  } catch {
    // Нет сети — останутся ключи.
  }
}

export function championName(key: string): string {
  return champions.byKey[key.toLowerCase()]?.name ?? key
}

export function championIcon(key: string): string {
  const champion = champions.byKey[key.toLowerCase()]
  return champion && champions.version ? `${DDRAGON}/cdn/${champions.version}/img/champion/${champion.id}.png` : ''
}
