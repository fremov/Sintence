export const ROLE: Record<string, string> = {
  TOP: 'верх',
  JUNGLE: 'лес',
  MIDDLE: 'центр',
  BOTTOM: 'низ',
  UTILITY: 'поддержка',
}

export const TIER: Record<string, string> = {
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
  '?': 'неизвестно',
}

export const fmt = (n: number | null | undefined) => (n ?? 0).toLocaleString('ru-RU')
export const pct = (x: number | null | undefined) => `${Math.round((x ?? 0) * 100)}%`

export function hours(h: number): string {
  if (!isFinite(h) || h <= 0) return '0 ч'
  if (h < 1) return `${Math.max(1, Math.round(h * 60))} мин`
  if (h < 48) return `${h.toFixed(h < 10 ? 1 : 0)} ч`
  return `${(h / 24).toFixed(1)} сут`
}

export function duration(seconds: number): string {
  const h = Math.floor(seconds / 3600)
  const m = Math.floor((seconds % 3600) / 60)
  const s = seconds % 60
  if (h) return `${h} ч ${m} мин`
  if (m) return `${m} мин ${s} с`
  return `${s} с`
}

export function ago(unix: number | undefined): string {
  if (!unix) return '—'
  const minutes = Math.round((Date.now() / 1000 - unix) / 60)
  if (minutes < 1) return 'только что'
  if (minutes < 60) return `${minutes} мин назад`
  if (minutes < 60 * 48) return `${Math.round(minutes / 60)} ч назад`
  return new Date(unix * 1000).toLocaleString('ru-RU')
}

export function bytes(n: number): string {
  if (n > 1 << 30) return `${(n / (1 << 30)).toFixed(1)} ГБ`
  if (n > 1 << 20) return `${Math.round(n / (1 << 20))} МБ`
  return `${Math.round(n / 1024)} КБ`
}
