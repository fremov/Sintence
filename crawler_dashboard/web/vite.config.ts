import tailwindcss from '@tailwindcss/vite'
import vue from '@vitejs/plugin-vue'
import { defineConfig } from 'vite'

// Сборка — в dist/, её раздаёт crawler_dashboard/server.py.
// npm run dev: страница на :5175 с горячей перезагрузкой, /api — на сервер
// пульта (python crawler_dashboard/server.py --no-browser). Origin
// подменяется на адрес сервера: действия он принимает только со своего.
export default defineConfig({
  plugins: [vue(), tailwindcss()],
  base: './',
  build: { outDir: 'dist', emptyOutDir: true },
  server: {
    port: 5175,
    proxy: {
      '/api': {
        target: 'http://127.0.0.1:8790',
        changeOrigin: true,
        headers: { origin: 'http://127.0.0.1:8790' },
      },
    },
  },
})
