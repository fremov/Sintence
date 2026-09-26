<script setup lang="ts">
import { computed } from 'vue'

// Полоса заполнения. mark — риска на шкале (например, порог пака).
const props = withDefaults(
  defineProps<{ value: number; max: number; tone?: 'gold' | 'ok' | 'warn' | 'bad' | 'blue'; mark?: number | null }>(),
  { tone: 'gold', mark: null },
)

const width = computed(() => (props.max > 0 ? Math.min(100, (props.value / props.max) * 100) : 0))
const markAt = computed(() =>
  props.mark !== null && props.max > 0 ? Math.min(100, (props.mark / props.max) * 100) : null,
)
const colors = { gold: 'bg-gold', ok: 'bg-ok', warn: 'bg-warn', bad: 'bg-red', blue: 'bg-blue' }
</script>

<template>
  <div class="relative h-1.5 overflow-visible rounded-full bg-[#0b0f14]">
    <div class="absolute inset-y-0 left-0 rounded-full" :class="colors[tone]" :style="{ width: `${width}%` }" />
    <div
      v-if="markAt !== null"
      class="absolute -inset-y-0.5 w-0.5 bg-text/60"
      :style="{ left: `${markAt}%` }"
    />
  </div>
</template>
