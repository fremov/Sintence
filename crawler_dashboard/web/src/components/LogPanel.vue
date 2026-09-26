<script setup lang="ts">
import { nextTick, ref, watch } from 'vue'
import type { LogLine } from '../api'

// Вывод запущенных скриптов — то, что раньше было видно в терминале.

const props = defineProps<{ lines: LogLine[] }>()
const follow = ref(true)
const box = ref<HTMLElement | null>(null)

const ERROR = /traceback|error|ошибк|не найден|\b(429|403|401)\b/i
const time = (t: number) => new Date(t * 1000).toLocaleTimeString('ru-RU')

watch(
  () => props.lines.length,
  async () => {
    if (!follow.value) return
    await nextTick()
    if (box.value) box.value.scrollTop = box.value.scrollHeight
  },
)
</script>

<template>
  <section class="card">
    <div class="mb-2 flex items-baseline justify-between gap-3">
      <h2 class="card-title">Журнал</h2>
      <label class="flex items-center gap-2 text-xs text-muted">
        <input v-model="follow" type="checkbox" class="accent-gold" /> прокручивать к новым строкам
      </label>
    </div>
    <p class="hint mb-2">
      Вывод сборщика и сборки пака. Золотым — события пульта, красным — ошибки. 429 — Riot попросил подождать:
      сборщик выдержит паузу сам.
    </p>
    <div
      ref="box"
      class="h-80 overflow-auto rounded-md border border-edge bg-[#090c10] px-3 py-2 font-mono text-xs leading-relaxed whitespace-pre-wrap text-[#b8c2d0]"
    >
      <div v-if="!lines.length" class="text-muted">Пусто.</div>
      <div v-for="line in lines" :key="line.n">
        <span class="text-[#4c5666]">{{ time(line.t) }}</span>
        <span :class="line.system ? 'text-gold' : ERROR.test(line.text) ? 'text-red' : ''">{{ line.text }}</span>
      </div>
    </div>
  </section>
</template>
