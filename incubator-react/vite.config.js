import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import tailwindcss from '@tailwindcss/vite'
import package_file from './package.json'

export default defineConfig({
  plugins: [react(), tailwindcss()],
  define: {
    'import.meta.env.APP_VERSION': JSON.stringify(package_file.version)
  },
  test: {
    globals: true,
    environment: 'jsdom',
    setupFiles: './src/test/setup.js'
  }
})