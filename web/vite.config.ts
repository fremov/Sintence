import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

// Сборка кладётся в web/dist, откуда её раздаёт C++-сервер.
// В режиме разработки (npm run dev) страница живёт на :5173, а запросы
// к /api проксируются в sintence.exe — так не нужен CORS и можно править
// интерфейс с горячей перезагрузкой, не пересобирая C++.
export default defineConfig({
  plugins: [vue()],
  base: './',
  build: {
    outDir: 'dist',
    emptyOutDir: true,
  },
  server: {
    port: 5173,
    proxy: {
      '/api': 'http://127.0.0.1:8777',
    },
  },
})
